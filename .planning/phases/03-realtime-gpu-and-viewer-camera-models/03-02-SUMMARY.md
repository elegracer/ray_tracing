---
phase: 03-realtime-gpu-and-viewer-camera-models
plan: 02
subsystem: realtime-gpu
tags: [camera, realtime, gpu, optix, pinhole32, equi62_lut1d]
requires:
  - phase: 03-realtime-gpu-and-viewer-camera-models
    provides: spec-driven viewer/runtime camera rigs from authored camera specs
provides:
  - active-camera direction debug driven by the selected packed realtime camera
  - live OptiX mixed-model equi regression coverage
  - model-preserving packed-camera upload checks for pinhole and equi payloads
affects: [realtime, gpu, tests]
tech-stack:
  added: []
  patterns: [selected-camera debug rendering, mixed-model optix regression coverage]
key-files:
  created: []
  modified:
    - CMakeLists.txt
    - src/realtime/gpu/optix_renderer.h
    - src/realtime/gpu/optix_renderer.cpp
    - src/realtime/gpu/programs.cu
    - tests/test_optix_direction.cpp
    - tests/test_optix_equi_path_trace.cpp
key-decisions:
  - "Direction debug now accepts an explicit `camera_index` so the debug path and live radiance path select cameras through the same contract."
  - "The OptiX equi regression now uses the repo's already-visible lit scene fixture pattern instead of a scene that rendered black for both pinhole and equi."
  - "The core target now compiles `four_camera_rig.cpp` because Phase 3 moved realtime factory construction onto viewer rig helpers."
requirements-completed: [CAM-03]
duration: 0min
completed: 2026-04-21
---

# Phase 3: Plan 02 Summary

**OptiX active-camera execution now preserves selected pinhole or equi payloads through debug and live radiance paths**

## Accomplishments

- Updated [optix_renderer.h](/home/huangkai/codes/ray_tracing/src/realtime/gpu/optix_renderer.h), [optix_renderer.cpp](/home/huangkai/codes/ray_tracing/src/realtime/gpu/optix_renderer.cpp), and [programs.cu](/home/huangkai/codes/ray_tracing/src/realtime/gpu/programs.cu) so direction debug renders from the selected active packed camera instead of a camera-agnostic gradient.
- Extended [test_optix_direction.cpp](/home/huangkai/codes/ray_tracing/tests/test_optix_direction.cpp) with explicit pinhole and equi direction checks plus selected-camera coverage for a mixed-model rig.
- Reworked [test_optix_equi_path_trace.cpp](/home/huangkai/codes/ray_tracing/tests/test_optix_equi_path_trace.cpp) into a mixed-model `camera_index = 1` regression that proves equi background rays stay live and lit OptiX beauty/normal/albedo/depth outputs are non-empty with varying depth.
- Fixed the missing [four_camera_rig.cpp](/home/huangkai/codes/ray_tracing/src/realtime/viewer/four_camera_rig.cpp) linkage in [CMakeLists.txt](/home/huangkai/codes/ray_tracing/CMakeLists.txt) so Phase 3's new shared viewer/runtime camera path links cleanly through `core`.

## Verification

- `cmake --build build --target test_optix_direction test_optix_equi_path_trace -j4`
- `ctest --test-dir build -R '^(test_optix_direction|test_optix_equi_path_trace)$' --output-on-failure`
- `rg -n "camera.model == CameraModelType::equi62_lut1d|active\\.equi|active\\.pinhole" src/realtime/gpu/programs.cu src/realtime/gpu/optix_renderer.cpp`

## Task Commits

1. **Task 1: Preserve packed camera model data through the OptiX upload and debug path** - `71001ab` (feat)
2. **Task 2: Recover and regress live equi radiance rendering in a mixed-model rig** - `cca0d55` (test)

## Deviations from Plan

- The original `test_optix_equi_path_trace` scene rendered black for both pinhole and equi, so the regression was rebased onto the already-proven lit scene fixture pattern used by the other OptiX path-trace tests. This kept the scope on camera-model preservation instead of debugging an invalid scene setup.
- Building the Wave 2 slice exposed a missing `core` linkage to `four_camera_rig.cpp`, so [CMakeLists.txt](/home/huangkai/codes/ray_tracing/CMakeLists.txt) was updated as a correctness fix for Phase 3's shared rig path.

## Issues Encountered

- `test_optix_equi_path_trace` initially failed with a black frame even after the equi path was confirmed to preserve background rays. The failure was traced to the test fixture scene, not to the equi camera upload or unprojection path.

## User Setup Required

None.
