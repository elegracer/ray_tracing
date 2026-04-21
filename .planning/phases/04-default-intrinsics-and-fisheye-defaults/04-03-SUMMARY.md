---
phase: 04-default-intrinsics-and-fisheye-defaults
plan: 03
subsystem: viewer-defaults
tags: [camera, defaults, viewer, reference, pinhole32, equi62_lut1d]
requires:
  - phase: 04-default-intrinsics-and-fisheye-defaults
    provides: fisheye-first builtin preset defaults
provides:
  - no-arg viewer rig defaults switched to equi62_lut1d
  - viewer regression coverage updated for fisheye defaults plus explicit pinhole overrides
  - CPU reference compatibility with the new default rig
affects: [viewer, runtime-rig, cpu-reference, tests]
tech-stack:
  added: []
  patterns: [shared-helper-backed viewer defaults, explicit-model regression split]
key-files:
  created: []
  modified:
    - src/realtime/viewer/four_camera_rig.cpp
    - tests/test_viewer_four_camera_rig.cpp
    - tests/test_viewer_quality_reference.cpp
key-decisions:
  - "The no-arg viewer rig now fabricates fisheye defaults through `derive_default_camera_intrinsics(...)` instead of a local pinhole block."
  - "Pinhole-specific viewer CPU-reference assertions now use an explicit pinhole scene rig instead of mutating unused pinhole fields on the new default equi rig."
  - "Viewer pose and yaw-offset semantics remain unchanged; only the implicit default camera payload changed."
requirements-completed: [DEF-02, DEF-03, DEF-04]
completed: 2026-04-21
---

# Phase 4: Plan 03 Summary

**The remaining no-argument viewer camera path now defaults to fisheye, with viewer and CPU-reference regressions updated to separate default-equi behavior from explicit pinhole behavior**

## Accomplishments

- Replaced the local pinhole fabrication block in [`four_camera_rig.cpp`](/home/huangkai/codes/ray_tracing/src/realtime/viewer/four_camera_rig.cpp) with a shared-helper-backed `equi62_lut1d` default camera spec.
- Updated [`test_viewer_four_camera_rig.cpp`](/home/huangkai/codes/ray_tracing/tests/test_viewer_four_camera_rig.cpp) to assert that the no-argument four-camera rig now packs equi cameras with the shared 120-degree default intrinsics, while the existing mixed-model/authored overrides still stay intact.
- Updated [`test_viewer_quality_reference.cpp`](/home/huangkai/codes/ray_tracing/tests/test_viewer_quality_reference.cpp) so default-viewer-reference checks use the new equi default rig, but pinhole-specific invalid/off-center/distorted checks use an explicit pinhole rig from the scene preset.

## Verification

- `cmake --build build --target test_viewer_four_camera_rig test_viewer_quality_reference test_realtime_scene_factory -j4`
- `ctest --test-dir build -R '^(test_viewer_four_camera_rig|test_viewer_quality_reference|test_realtime_scene_factory)$' --output-on-failure`

## Deviations from Plan

- The original pinhole-specific CPU-reference cases in `test_viewer_quality_reference` were mutating pinhole fields on the new default equi rig. Those checks were moved onto an explicit scene-authored pinhole rig so the regression still tests the intended contract instead of writing into inactive fields.

## User Setup Required

None.
