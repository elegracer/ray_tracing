# Phase 3: Realtime GPU And Viewer Camera Models - Patterns

## Purpose

Map the Phase 3 changes to the closest existing repo patterns so planning extends the current realtime/viewer pipeline instead of inventing parallel camera contracts.

## File Pattern Map

| Target area | Likely files | Closest analogs | Notes |
|-------------|--------------|-----------------|-------|
| Authored camera rescaling for runtime rigs | `src/realtime/realtime_scene_factory.cpp` | existing `runtime_camera_spec(...)` | Reuse the same scale-from-authored-calibration rule for any new viewer rig path. |
| Viewer rig construction from canonical camera data | `src/realtime/viewer/four_camera_rig.h`, `src/realtime/viewer/four_camera_rig.cpp` | existing yaw-offset/body-pose logic in `make_default_viewer_rig(...)` + `CameraRig::add_camera(...)` | Keep pose math, replace hardcoded pinhole params. |
| Runtime camera packing | `src/realtime/camera_rig.cpp` | existing `add_camera(...)` and `pack()` | Already supports both camera models; Phase 3 should lean on it rather than bypass it. |
| GPU active-camera upload | `src/realtime/gpu/optix_renderer.cpp` | existing `make_active_camera(...)` | This is the boundary where packed runtime camera data must remain intact for OptiX. |
| Device-side ray generation | `src/realtime/gpu/programs.cu` | existing `unproject_pinhole32(...)` / `unproject_equi62_lut1d(...)` dispatch | The model branch already exists; use tests to lock it down. |
| Scene-preset and file-backed regressions | `tests/test_realtime_scene_factory.cpp`, `tests/test_viewer_scene_reload.cpp` | existing preset-scaling checks and temp-catalog reload coverage | Best home for authored preset preservation and YAML-backed equi scene tests. |
| Four-camera viewer regressions | `tests/test_viewer_four_camera_rig.cpp` | existing pose/yaw-offset assertions | Extend to cover spec-driven viewer rig creation and mixed-model camera preservation. |
| GPU camera regressions | `tests/test_optix_direction.cpp`, `tests/test_optix_equi_path_trace.cpp` | existing OptiX smoke and equi radiance tests | Use these to prove active camera selection and equi rendering stay live. |

## Reusable Patterns

### 1. `runtime_camera_spec(...)` is already the canonical resize seam
- `src/realtime/realtime_scene_factory.cpp` already copies an authored `CameraSpec`, scales `fx/fy/cx/cy`, and composes `T_bc`.
- Planning implication:
  - Do not introduce a second resize helper with different semantics.
  - New viewer helpers should either call this helper or reproduce its exact scaling logic verbatim.

### 2. Viewer pose math is already correct and should be preserved
- `src/realtime/viewer/four_camera_rig.cpp` already owns:
  - `kDefaultSurroundYawOffsetsDeg`
  - frame-convention-based `forward_direction(...)` / `right_direction(...)`
  - translation from body pose into renderer coordinates
- Planning implication:
  - Replace only the camera payload construction.
  - Keep yaw offsets, pitch handling, and body-pose movement semantics intact.

### 3. `CameraRig::add_camera(...)` is the existing model-preserving adapter
- `src/realtime/camera_rig.cpp` already converts `CameraSpec` into either `Pinhole32Params` or `Equi62Lut1DParams`, and `pack()` preserves `model`, `T_rc`, and per-model params.
- Planning implication:
  - New viewer rig code should feed `CameraSpec` into `add_camera(...)` rather than hardcoding `add_pinhole(...)`.
  - Mixed-model viewer rigs should be expressed as multiple `CameraSpec` inputs, not as conditionals around hardcoded pinhole params.

### 4. GPU-side camera math already branches on `CameraModelType`
- `src/realtime/gpu/programs.cu` dispatches `unproject_equi62_lut1d(...)` when `camera.model == equi62_lut1d`.
- Planning implication:
  - Phase 3 should focus on contract preservation and regression coverage.
  - If equi rendering still fails, debug the packed-camera upload or active-camera selection before rewriting kernel math.

### 5. File-backed scene tests should use temp roots, not repo asset mutations
- `tests/test_viewer_scene_reload.cpp` already writes temp `scene.yaml` files and validates catalog reload behavior.
- Planning implication:
  - If Phase 3 needs authored equi realtime presets, prefer temp file-backed scenes scanned/reloaded through existing catalog code.
  - Do not change repo-owned scene YAML merely to manufacture one fisheye realtime test case during this phase.

## Recommended Read Order For Executors

1. `src/realtime/realtime_scene_factory.cpp`
2. `src/realtime/viewer/four_camera_rig.cpp`
3. `src/realtime/camera_rig.h`
4. `src/realtime/camera_rig.cpp`
5. `src/realtime/gpu/optix_renderer.cpp`
6. `src/realtime/gpu/programs.cu`
7. `tests/test_realtime_scene_factory.cpp`
8. `tests/test_viewer_four_camera_rig.cpp`
9. `tests/test_optix_direction.cpp`
10. `tests/test_optix_equi_path_trace.cpp`
11. `tests/test_viewer_scene_reload.cpp`

## Planning Guidance

- Put the viewer-rig contract first. Until viewer helpers stop hardcoding pinhole params, later GPU/viewer regressions will keep mixing two camera truths.
- Treat `PackedCamera` as the handoff boundary. Planner tasks should not bypass it when wiring scene/viewer behavior into GPU tests.
- Include one GPU task explicitly aimed at the failing equi radiance regression; do not assume existing equi kernel support means CAM-03 is already satisfied.
- Include one file-backed realtime preset regression so Phase 3 proves authored YAML/preset camera payloads survive the viewer/realtime path, not just direct in-memory rigs.

---
## PATTERN MAPPING COMPLETE
