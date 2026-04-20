# Phase 4: Default Intrinsics And Fisheye Defaults - Patterns

## Purpose

Map Phase 4 changes to the current default-camera construction seams so execution can replace implicit pinhole defaults with a single shared fisheye-first derivation path instead of scattering new focal-length formulas across scene, viewer, and utility code.

This phase was planned on the skip-research path. The patterns below come from direct code inspection of the current default-entry points and nearby tests.

## File Pattern Map

| Target area | Likely files | Closest analogs | Notes |
|-------------|--------------|-----------------|-------|
| Shared default-intrinsics math | `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp` | existing `make_equi62_lut1d_params(...)`, `project_*`, `unproject_*` helpers | Put the `model + width/height + hfov -> fx/fy/cx/cy` math next to the existing camera-model seam so offline, realtime, viewer, and tests all consume the same formula. |
| Utility wrapper for ad hoc intrinsics calculation | `utils/derive_default_camera_intrinsics.cpp`, optional thin wrapper script under `utils/` | existing small utility executables in `utils/` | Prefer a tiny CLI over duplicating the formula in shell logic. If a wrapper script is added, it should only forward arguments to the compiled utility. |
| Builtin CPU/realtime default camera presets | `src/scene/shared_scene_builders.cpp` | existing `make_cpu_camera(...)`, `make_realtime_view_preset(...)` | This is the main builtin/default migration point. Default helpers still derive pinhole intrinsics from `vfov`. |
| Viewer no-arg default rig | `src/realtime/viewer/four_camera_rig.cpp` | existing `default_pinhole_viewer_camera_spec(...)` | This is the clearest implicit viewer-side pinhole default and should switch to the shared default-intrinsics helper. |
| Builtin/default regression coverage | `tests/test_shared_scene_builders.cpp`, `tests/test_realtime_scene_factory.cpp`, `tests/test_viewer_four_camera_rig.cpp`, `tests/test_viewer_quality_reference.cpp` | existing default-rig and preset assertions | Extend existing regression tests instead of adding a parallel default-camera test stack. |

## Reusable Patterns

### 1. Camera-model math already has a central home
- `src/realtime/camera_models.h`
- `src/realtime/camera_models.cpp`
- Planning implication:
  - The new default-intrinsics derivation helper should live here, not in scene builders or viewer code.
  - Default camera construction should call into this helper and then populate `CameraSpec`.

### 2. Builtin scene presets already funnel through a small number of helpers
- `src/scene/shared_scene_builders.cpp`
  - `make_cpu_camera(...)`
  - `make_realtime_view_preset(...)`
- Planning implication:
  - Switching builtin defaults is mostly a helper migration task, not a per-scene rewrite task.
  - Preserve explicit pinhole call sites by keeping a narrow compatibility wrapper where needed.

### 3. Viewer default cameras already have a single local fabrication point
- `src/realtime/viewer/four_camera_rig.cpp`
  - `default_pinhole_viewer_camera_spec(...)`
- Planning implication:
  - Phase 4 should replace this one-off pinhole block with a shared default camera helper.
  - Keep the existing pose and yaw-offset behavior unchanged.

### 4. Explicit authored scenes are already on canonical camera schema
- YAML and file-backed scenes now explicitly author `model`, `width`, `height`, `fx`, `fy`, `cx`, and `cy`.
- Planning implication:
  - Do not churn repo-owned scene YAML in this phase.
  - Focus only on implicit construction paths and helper-generated presets.

### 5. Realtime resizing and viewer resizing already scale authored intrinsics
- `src/realtime/realtime_scene_factory.cpp`
- `src/realtime/viewer/four_camera_rig.cpp`
- Planning implication:
  - Phase 4 should derive authored base intrinsics once, then reuse existing resize behavior.
  - Do not invent a second runtime-only focal-length formula.

## Recommended Read Order For Executors

1. `src/realtime/camera_models.h`
2. `src/realtime/camera_models.cpp`
3. `src/scene/shared_scene_builders.cpp`
4. `src/realtime/viewer/four_camera_rig.cpp`
5. `tests/test_camera_models.cpp`
6. `tests/test_shared_scene_builders.cpp`
7. `tests/test_realtime_scene_factory.cpp`
8. `tests/test_viewer_four_camera_rig.cpp`
9. `tests/test_viewer_quality_reference.cpp`

## Planning Guidance

- Put the shared intrinsics derivation helper first. Every later default switch depends on one canonical formula.
- Keep builtin helper migration separate from viewer default-rig migration so regressions can isolate scene-helper churn from viewer churn.
- Use existing tests to prove two things in parallel:
  - implicit defaults flip to fisheye
  - explicit pinhole authorship still stays pinhole
- Treat the utility wrapper as a consumer of the shared helper, not as a second source of truth.

---
## PATTERN MAPPING COMPLETE
