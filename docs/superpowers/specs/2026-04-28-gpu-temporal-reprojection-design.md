# Design: GPU-Side Temporal Accumulation with Screen-Space Reprojection

## Problem

Temporal accumulation in `viewer_quality_controller.cpp` does pixel-in-place blending without
camera-motion reprojection. When the camera rotates, old pixel values at screen coordinate (x,y)
correspond to different world-space positions, producing ghosting artifacts during camera movement.

The current mitigation is a rotation-reset threshold (default 2 degrees). Below that threshold,
ghosting still accumulates; above it, history is discarded entirely, losing all temporal denoising.

## Goal

Move temporal accumulation to the GPU, performing screen-space reprojection each frame so history
pixels are correctly mapped to their new screen positions after camera motion.

## Design

### Architecture Overview

```
Before: GPU radiance kernel → download beauty → CPU resolve_beauty_view() → display
After:  GPU radiance kernel → GPU resolve + reprojection kernel → download resolved beauty → CPU pass-through → display
```

The GPU resolve kernel reads the current frame's beauty, normal, and depth buffers, plus the
previous frame's history buffers and camera matrix, and produces a motion-compensated output
directly on the device.

### GPU Resolve Kernel (`resolve_reprojection_kernel`)

A new CUDA kernel launched immediately after `radiance_kernel` on the same stream.

**Inputs from LaunchParams (new fields):**

- `DeviceFrameBuffers history_beauty` — previous frame resolved beauty (float4)
- `DeviceFrameBuffers history_normal` — previous frame normal (float4, encoded 0.5*n+0.5)
- `float* history_depth` — previous frame depth (float)
- `double prev_origin[3]` — previous camera origin
- `double prev_basis_x[3]`, `prev_basis_y[3]`, `prev_basis_z[3]` — previous camera basis rows (for world→camera-space transform)
- `int history_length` — number of accumulated frames (reset on scene/pose jump)

**Per-pixel steps:**

1. **World-position reconstruction** — from current depth and current camera:
   ```
   dir_camera = unproject_camera_ray(current_camera, x+0.5, y+0.5)
   dir_world = transform_direction(current_camera, dir_camera)
   world_pos = camera_origin(current_camera) + depth * dir_world
   ```

2. **Reprojection to previous screen space** — using previous camera:
   ```
   cam_pos = world_pos - prev_origin
   cam_dir = (dot(cam_pos, prev_basis_x), dot(cam_pos, prev_basis_y), dot(cam_pos, prev_basis_z))
   prev_uv = project_to_screen(cam_dir)  // inverse of unproject, camera-model aware
   prev_pixel = (prev_uv.x * prev_width, prev_uv.y * prev_height)
   ```

   `project_to_screen` is the reverse of `unproject_camera_ray`. For pinhole32, this is the
   standard perspective divide + intrinsics remap. For equi62_lut1d, it inverts the LUT-based
   mapping.

3. **Geometry consistency checks:**
   - *Normal consistency:* `dot(decode_normal(current), decode_normal(history)) > 0.85`
   - *Depth continuity:* `0.9 < depth_current / depth_history < 1.1`
   - *UV bounds:* previous screen coordinate must be inside `[0, prev_width) × [0, prev_height)`

4. **EMA blend** (all checks pass):
   ```
   resolved = history_beauty[prev_idx] + (current_beauty[pixel_idx] - history_beauty[prev_idx]) / history_length
   ```
   With bilinear sampling of the history buffer at the reprojected coordinate.

5. **Fallback** (any check fails):
   ```
   resolved = current_beauty
   history_length = 1
   ```

### GPU History Buffer Management

`OptixRenderer` maintains two sets of device buffers:
- `device_frame_` — current frame radiance output (existing)
- `device_history_` — previous frame resolved output (new: beauty, normal, depth)

Per-frame sequence:
1. `launch_radiance_kernel` → writes `device_frame_.beauty/normal/albedo/depth`
2. `launch_resolve_kernel` → reads `device_history_` and `device_frame_`, writes resolved beauty into `device_frame_.beauty`
3. Download resolved beauty from `device_frame_.beauty` to CPU
4. Swap: copy `device_frame_` beauty/normal/depth → `device_history_`

History is allocated lazily on first frame and re-allocated on resolution change.

### LaunchParams Changes

```cpp
struct LaunchParams {
    // ... existing fields ...
    DeviceFrameBuffers history_beauty;   // NEW
    DeviceFrameBuffers history_normal;   // NEW
    float* history_depth;                // NEW
    double prev_origin[3];               // NEW
    double prev_basis_x[3];              // NEW (camera-to-world rows, for world→camera inverse = transpose)
    double prev_basis_y[3];              // NEW
    double prev_basis_z[3];              // NEW
    int history_length;                  // NEW (0 = no history / first frame)
};
```

### CPU-Side Simplification

**`viewer_quality_controller.cpp` changes:**

Remove:
- `CameraHistory::beauty_rgba` buffer and pixel-by-pixel EMA loop
- `bounded_history_value()`, `sanitized_value()` helper functions
- `pose_exceeded_reset_threshold()` rotation/translation thresholds — reprojection handles motion automatically
- Blending branch in `resolve_beauty_view()` — becomes a pass-through

Keep:
- `ViewerQualityMode` enum (`preview`, `converge`)
  - `preview`: signals GPU to set `history_length = 0`, skip accumulation
  - `converge`: normal operation with full reprojection resolve
- `begin_frame()` for large jump detection (scene switches, >30° pose changes) — triggers history clear
- `average_luminance` computation

**`RadianceFrame` changes:**

No structural changes needed. The resolved beauty is downloaded into `beauty_rgba` which
already goes to display. `normal_rgba` and `depth` CPU download can optionally be removed
from non-debug builds to reduce transfer overhead.

### Camera Model Support

`project_to_screen` must handle both camera models:

- **pinhole32:** Standard perspective projection. Use intrinsic params (fx, fy, cx, cy) and
  distortion coefficients (k1, k2, k3, p1, p2) in reverse — apply distortion to get the
  distorted-screen coordinate. This is the inverse of the existing undistort + unproject path.

- **equi62_lut1d:** Invert the LUT-based mapping. Given a 3D direction, compute the equirectangular
  angle, then invert the 1D distortion LUT to recover the distorted radius, then scale by the
  intrinsic fx/fy to get pixel coordinates.

### Edge Cases

| Scenario | Handling |
|----------|----------|
| First frame (no history) | `history_length = 0`, resolve kernel skipped, current frame output directly |
| Scene switch | `begin_frame()` sets `history_length = 0`, GPU history cleared |
| Resolution change | Detect width/height change, re-allocate history buffers, `history_length = 0` |
| Camera count change | Each camera slot maintains independent history, no cross-talk |
| Large pose jump (>30°) | Detected in `begin_frame()`, triggers `history_length = 0` |
| Disocclusion | Caught by geometry consistency check → fallback to current frame |

### Test Plan

1. **Static scene convergence:** Fixed camera, N frames. Verify noise decreases frame-over-frame
   and output matches the old CPU-accumulation result (within float epsilon in EMA math).

2. **Slow rotation:** Rotate camera at ~1 deg/frame. Verify no ghosting on high-contrast edges.

3. **Fast rotation:** Rotate rapidly (~10 deg/frame). Verify disoccluded regions contain no stale
   history artifacts.

4. **Scene switch:** Switch between scenes. Verify no residual from previous scene.

5. **Performance regression:** Measure frame time delta for the resolve kernel. Target: <1ms
   for 1920×1080, not dominating the render budget.

6. **Multi-camera:** Verify each of the 4 cameras accumulates independently and correctly.
