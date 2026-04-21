---
phase: 05-camera-model-regression-coverage
plan: 01
subsystem: camera-regression
tags: [camera, regression, reference, pinhole32, equi62_lut1d, tests]
requires: []
provides:
  - direct reference-backed parity checks for pinhole32
  - direct reference-backed parity checks for equi62_lut1d
  - minimal test-only shim layer for compiling bundled reference camera headers
affects: [tests, build]
tech-stack:
  added: [test-only reference shims]
  patterns: [reference-backed parity, existing-test extension, test-local compatibility shim]
key-files:
  created:
    - tests/reference_camera_shims/cam_type.h
    - tests/reference_camera_shims/common/android_log.h
    - tests/reference_camera_shims/common/matrix_utils.h
  modified:
    - CMakeLists.txt
    - tests/test_camera_models.cpp
key-decisions:
  - "Direct parity stays inside `test_camera_models` instead of adding a new dedicated executable."
  - "Bundled reference headers are compiled through a tiny test-only shim layer rather than copying their math into repo code."
  - "Parity tolerances remain model-specific: pinhole stays tighter, equi allows small LUT-backed error."
requirements-completed: [VER-01, VER-02]
completed: 2026-04-22
---

# Phase 5: Plan 01 Summary

**`test_camera_models` now anchors both supported camera models directly to the bundled reference implementations**

## Accomplishments

- Added test-only include plumbing in [CMakeLists.txt](/home/huangkai/codes/ray_tracing/CMakeLists.txt) so [`test_camera_models.cpp`](/home/huangkai/codes/ray_tracing/tests/test_camera_models.cpp) can compile the reference headers under [`docs/reference/src-cam/`](/home/huangkai/codes/ray_tracing/docs/reference/src-cam).
- Created a minimal compatibility shim under [`tests/reference_camera_shims/`](/home/huangkai/codes/ray_tracing/tests/reference_camera_shims) to satisfy the reference headers' missing shared dependencies without touching production camera code.
- Extended [`test_camera_models.cpp`](/home/huangkai/codes/ray_tracing/tests/test_camera_models.cpp) with direct `world2pixel` / `pixel2world` parity checks against `CamPinhole32` and `CamEqui62Lut1D`, including a helper-derived default fisheye case.

## Verification

- `cmake --build build --target test_camera_models -j4`
- `ctest --test-dir build -R '^test_camera_models$' --output-on-failure`

## Deviations from Plan

- The bundled reference headers were not self-contained; they depended on missing shared headers and constants. I resolved that with the smallest test-local shim layer instead of widening build or production dependencies.

## User Setup Required

None.
