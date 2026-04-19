# Phase 2: Offline CPU Camera Models - Patterns

## Purpose

Map likely Phase 2 file changes to the closest existing repo patterns so planning reuses the current offline renderer and scene-catalog structure instead of inventing a parallel CPU rendering stack.

## File Pattern Map

| Target area | Likely files | Closest analogs | Notes |
|-------------|--------------|-----------------|-------|
| Offline primary-ray generation seam | `src/common/camera.h`, optional narrow helper under `src/core/` | existing `Camera::get_ray(...)`, `Camera::render(...)` | Change the seam that emits primary rays; keep `ray_color(...)` and image assembly intact. |
| Shared camera math reuse | `src/realtime/camera_models.cpp`, `src/realtime/camera_models.h` | existing pinhole/equi project/unproject helpers | Do not re-derive fisheye behavior locally in offline code. |
| Shared-scene offline adapters | `src/core/offline_shared_scene_renderer.cpp` | existing `configure_offline_camera(...)`, `configure_camera_from_packed(...)` | Replace the pinhole-only configuration logic with a canonical model-aware adapter used by both offline entrypoints. |
| Preset-path verification | `tests/test_offline_shared_scene_renderer.cpp` | existing smoke coverage | Extend this test from structural smoke into model-switch and preset-path assertions. |
| Explicit packed-camera reference coverage | `tests/test_viewer_quality_reference.cpp` | existing CPU reference path checks | This is already the strongest consumer of `render_shared_scene_from_camera(...)`; update it deliberately if the contract broadens. |
| File-backed override verification | `tests/test_scene_file_catalog.cpp`, `tests/test_viewer_scene_reload.cpp` | existing temp-root `scene.yaml` patterns | Reuse these temp-catalog patterns if Phase 2 needs a file-backed fisheye CPU preset to prove `render_shared_scene(...)` honors `camera.model`. |

## Reusable Patterns

### 1. Keep the CPU tracer and replace only camera emission
- `src/common/camera.h`
  - `render(...)` already drives all path sampling and output creation.
  - `get_ray(...)` is the only place where pixel coordinates become a primary ray.
- Planning implication:
  - Phase 2 should target the ray-emission seam, not fork the tracer.
  - If a helper is introduced, it should exist only to keep `camera.h` readable.

### 2. Shared camera math is already canonical
- `src/realtime/camera_models.cpp`
  - `unproject_pinhole32(...)`
  - `unproject_equi62_lut1d(...)`
  - `make_equi62_lut1d_params(...)`
- Planning implication:
  - Offline ray directions should be defined by these helpers.
  - Direction-level tests should compare offline ray emission against these functions directly.

### 3. Explicit packed-camera offline rendering is a supported reference path
- `src/core/offline_shared_scene_renderer.h`
  - `render_shared_scene_from_camera(...)`
- `tests/test_viewer_quality_reference.cpp`
  - uses the explicit packed-camera path as the CPU reference renderer for viewer convergence checks.
- Planning implication:
  - Phase 2 plans must treat this as a real contract, not an incidental helper.
  - Any behavior changes here need dedicated regression coverage.

### 4. Scene-file temp roots are the established way to test preset-path behavior
- `tests/test_scene_file_catalog.cpp`
- `tests/test_viewer_scene_reload.cpp`
- Planning implication:
  - If `render_shared_scene(...)` needs proof that a fisheye-authored CPU preset is honored, use a temp catalog root with a minimal `scene.yaml` instead of editing repo-owned scenes mid-phase.

### 5. Shared-scene phase boundaries are enforced through narrow adapters
- Phase 1 already moved authored camera data into `CameraSpec`.
- `CpuCameraPreset` still keeps outer pose/DoF fields while `camera` owns model/intrinsics.
- Planning implication:
  - Phase 2 should consume the existing authored boundary, not redesign it.
  - Pose stays outer; model/intrinsics come from `camera`.

## Recommended Read Order For Executors

1. `src/core/offline_shared_scene_renderer.cpp`
2. `src/common/camera.h`
3. `src/realtime/camera_models.h`
4. `src/realtime/camera_models.cpp`
5. `src/scene/shared_scene_builders.h`
6. `tests/test_offline_shared_scene_renderer.cpp`
7. `tests/test_viewer_quality_reference.cpp`
8. `tests/test_scene_file_catalog.cpp`

## Planning Guidance

- Put the ray-generation seam first; every later task depends on it.
- Keep preset-path wiring separate from explicit packed-camera wiring only at the adapter level; the actual render path should be shared.
- Include one task dedicated to low-level direction parity, not just image smoke tests.
- Include one task dedicated to consumer/regression updates for `render_shared_scene_from_camera(...)` because that path is already externally relied on.

---
## PATTERN MAPPING COMPLETE
