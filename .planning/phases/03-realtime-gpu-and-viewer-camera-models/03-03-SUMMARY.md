---
phase: 03-realtime-gpu-and-viewer-camera-models
plan: 03
subsystem: testing
tags: [camera, realtime, viewer, reload, scene-catalog, equi62_lut1d]
requires:
  - phase: 03-realtime-gpu-and-viewer-camera-models
    provides: spec-driven viewer/runtime camera rigs and model-aware OptiX active-camera execution
provides:
  - file-backed realtime preset preservation coverage for equi cameras
  - viewer reload and rescan regression coverage for authored equi presets
  - a coherent end-to-end Phase 3 regression slice across factory, viewer, and OptiX
affects: [realtime, viewer, tests]
tech-stack:
  added: []
  patterns: [temp YAML preset fixtures, reload-time camera contract assertions]
key-files:
  created: []
  modified:
    - tests/test_realtime_scene_factory.cpp
    - tests/test_viewer_scene_reload.cpp
key-decisions:
  - "File-backed Phase 3 regressions use temp scene.yaml fixtures instead of repo-owned assets so camera-contract coverage stays isolated and deterministic."
  - "Viewer reload/rescan tests now assert camera model and scaled intrinsics directly, not just move speed and spawn pose."
requirements-completed: [CAM-03, CAM-04]
duration: 0min
completed: 2026-04-21
---

# Phase 3: Plan 03 Summary

**File-backed realtime presets and viewer reload flows now preserve authored equi camera models through the same runtime contract as builtin scenes**

## Accomplishments

- Extended [test_realtime_scene_factory.cpp](/home/huangkai/codes/ray_tracing/tests/test_realtime_scene_factory.cpp) with a temp file-backed `equi62_lut1d` scene fixture that proves `default_camera_rig_for_scene(...)` preserves model, scaled intrinsics, and authored pose.
- Extended [test_viewer_scene_reload.cpp](/home/huangkai/codes/ray_tracing/tests/test_viewer_scene_reload.cpp) so both reload and rescan flows preserve authored equi presets instead of collapsing back to pinhole assumptions.
- Closed Phase 3 with one coherent regression slice spanning scene factory, mixed-model viewer rigs, OptiX active-camera execution, and viewer reload/rescan.

## Verification

- `cmake --build build --target test_realtime_scene_factory test_viewer_scene_reload -j4`
- `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_scene_reload)$' --output-on-failure`
- `cmake --build build --target test_realtime_scene_factory test_viewer_four_camera_rig test_optix_direction test_optix_equi_path_trace test_viewer_scene_reload -j4`
- `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace|test_viewer_scene_reload)$' --output-on-failure`

## Task Commits

1. **Task 1: Add file-backed realtime preset and reload coverage for authored camera models** - `dbad9a2` (test)
2. **Task 2: Close with a coherent mixed-model regression slice** - `dbad9a2` (test)

## Deviations from Plan

None.

## Issues Encountered

- The new factory regression needed direct access to the global scene file catalog, so [test_realtime_scene_factory.cpp](/home/huangkai/codes/ray_tracing/tests/test_realtime_scene_factory.cpp) was updated to include [scene_file_catalog.h](/home/huangkai/codes/ray_tracing/src/scene/scene_file_catalog.h) before the file-backed fixture could compile.

## User Setup Required

None.
