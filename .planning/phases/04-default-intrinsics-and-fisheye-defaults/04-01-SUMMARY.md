---
phase: 04-default-intrinsics-and-fisheye-defaults
plan: 01
subsystem: camera-defaults
tags: [camera, defaults, intrinsics, hfov, pinhole32, equi62_lut1d]
requires: []
provides:
  - canonical default intrinsics derivation for pinhole32 and equi62_lut1d
  - reusable default HFOV constants for the two supported camera models
  - thin CLI and shell wrapper for ad hoc intrinsics calculation
affects: [shared-camera-math, utils, tests]
tech-stack:
  added: [argparse CLI utility]
  patterns: [single-source default math, helper-backed CLI wrapper]
key-files:
  created:
    - utils/derive_default_camera_intrinsics.cpp
    - utils/derive_default_camera_intrinsics.sh
  modified:
    - CMakeLists.txt
    - src/realtime/camera_models.h
    - src/realtime/camera_models.cpp
    - tests/test_camera_models.cpp
key-decisions:
  - "Default intrinsics now derive from horizontal FOV with `fx == fy`, `cx = width / 2`, and `cy = height / 2`."
  - "Default pinhole HFOV is fixed at 90 degrees and default equidistant fisheye HFOV is fixed at 120 degrees."
  - "The utility wrapper calls the shared helper instead of carrying its own focal-length formula."
requirements-completed: [DEF-04, DEF-05, DEF-06]
completed: 2026-04-21
---

# Phase 4: Plan 01 Summary

**Phase 4 now has one canonical `hfov -> fx/fy/cx/cy` path for both supported camera models, plus a small CLI to inspect the derived defaults**

## Accomplishments

- Added `DefaultCameraIntrinsics`, `default_hfov_deg(...)`, and `derive_default_camera_intrinsics(...)` in the shared camera-model seam so default pinhole and fisheye calibration values come from one helper.
- Wired a new [`derive_default_camera_intrinsics.cpp`](/home/huangkai/codes/ray_tracing/utils/derive_default_camera_intrinsics.cpp) utility and a thin [`derive_default_camera_intrinsics.sh`](/home/huangkai/codes/ray_tracing/utils/derive_default_camera_intrinsics.sh) wrapper into [CMakeLists.txt](/home/huangkai/codes/ray_tracing/CMakeLists.txt).
- Extended [`test_camera_models.cpp`](/home/huangkai/codes/ray_tracing/tests/test_camera_models.cpp) with default-HFOV assertions, exact default intrinsic values for `640x480`, and invalid-input rejection checks.

## Verification

- `cmake --build build --target derive_default_camera_intrinsics test_camera_models -j4`
- `./bin/derive_default_camera_intrinsics --model pinhole32 --width 640 --height 480 --hfov-deg 90`
- `./bin/derive_default_camera_intrinsics --model equi62_lut1d --width 640 --height 480 --hfov-deg 120`
- `ctest --test-dir build -R '^test_camera_models$' --output-on-failure`

## Deviations from Plan

- The built binary lands under `bin/derive_default_camera_intrinsics` in this repo layout rather than `build/derive_default_camera_intrinsics`; the implementation and verification used the actual generated path.

## User Setup Required

None.
