# Phase 5: Camera Model Regression Coverage - Patterns

## Purpose

Map Phase 5 changes to the existing camera-regression seams so execution can strengthen proof without inventing a parallel testing stack or widening production scope.

## File Pattern Map

| Target area | Likely files | Closest analogs | Notes |
|-------------|--------------|-----------------|-------|
| Direct reference parity for both camera models | `tests/test_camera_models.cpp`, possibly `CMakeLists.txt` for include-path plumbing | existing low-level model tests and default-intrinsics checks | Prefer integrating the reference headers into the existing math test target instead of adding a new executable. |
| Offline CPU ray contract | `tests/test_offline_shared_scene_renderer.cpp` | existing `Camera::debug_primary_ray(...)` seam assertions | Reuse the existing packed-camera fixtures and avoid new production debug hooks. |
| Realtime GPU ray contract | `tests/test_optix_direction.cpp` | existing direction-debug comparisons against shared unproject helpers | Best deterministic seam for comparing selected camera model and intrinsics at pixel centers. |
| Cross-path smoke after ray-level contract is locked | `tests/test_reference_vs_realtime.cpp` | existing coarse mean-luminance comparison | Upgrade this file to follow canonical camera data rather than treating legacy pinhole setup as the reference truth. |
| Default-vs-authored regression lattice | `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`, `tests/test_viewer_quality_reference.cpp`, `tests/test_optix_equi_path_trace.cpp` | existing fisheye-default and explicit-pinhole regressions | Use these tests as the widened final slice; only add assertions where the authored/default split is still implicit. |

## Reusable Patterns

### 1. Low-level camera math already has one canonical regression target
- `tests/test_camera_models.cpp`
- Planning implication:
  - Put all direct reference-header parity here.
  - If the reference headers need local wrappers or CMake include-path adjustments, keep them test-only and as small as possible.

### 2. Offline and realtime already expose deterministic ray seams
- `tests/test_offline_shared_scene_renderer.cpp`
- `tests/test_optix_direction.cpp`
- Planning implication:
  - Prove CPU/GPU contract preservation by aligning fixtures and pixel samples across these two tests.
  - Do not add another production-facing debug API unless the existing seams prove insufficient.

### 3. Cross-path image comparisons are intentionally coarse
- `tests/test_reference_vs_realtime.cpp`
- `tests/test_viewer_quality_reference.cpp`
- Planning implication:
  - Keep image-level checks as smoke/regression coverage.
  - Do not make image-level similarity the main pass/fail contract for Phase 5.

### 4. Default-vs-authored behavior is already distributed across existing regressions
- `tests/test_realtime_scene_factory.cpp`
- `tests/test_shared_scene_regression.cpp`
- `tests/test_viewer_scene_reload.cpp`
- Planning implication:
  - Tighten these tests rather than creating a separate “default switch” test suite.
  - Preserve the distinction between helper-generated fisheye defaults and explicitly authored pinhole scenes.

### 5. The widened final slice is already executable and green
- `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure`
- Planning implication:
  - Use this slice as the final validation target.
  - Earlier task-level verification can stay narrower to reduce feedback latency.

## Recommended Read Order For Executors

1. `tests/test_camera_models.cpp`
2. `docs/reference/src-cam/cam_pinhole32.h`
3. `docs/reference/src-cam/cam_equi62_lut1d.h`
4. `tests/test_offline_shared_scene_renderer.cpp`
5. `tests/test_optix_direction.cpp`
6. `tests/test_reference_vs_realtime.cpp`
7. `tests/test_realtime_scene_factory.cpp`
8. `tests/test_shared_scene_regression.cpp`
9. `tests/test_viewer_scene_reload.cpp`
10. `tests/test_viewer_quality_reference.cpp`
11. `tests/test_optix_equi_path_trace.cpp`

## Planning Guidance

- Keep Plan 01 purely about reference-backed math parity. That is the sharpest missing proof and has the smallest surface area.
- Keep Plan 02 focused on contract preservation between offline CPU and realtime GPU using the same packed-camera fixtures.
- Use Plan 03 to widen and harden the authored-vs-default regression lattice, then validate the whole mandatory slice in one go.
- Only touch production code if the stronger tests expose a real mismatch; the default expectation is test-only work.

---
## PATTERN MAPPING COMPLETE
