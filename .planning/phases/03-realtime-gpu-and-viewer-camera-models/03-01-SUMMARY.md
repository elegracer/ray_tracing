---
phase: 03-realtime-gpu-and-viewer-camera-models
plan: 01
subsystem: viewer-rig
tags: [camera, realtime, viewer, pinhole32, equi62_lut1d]
requires: []
provides:
  - spec-driven viewer rig construction built from canonical camera specs
  - shared viewer/runtime rig path for authored realtime presets
  - authored-equi and mixed-model four-camera regression coverage
affects: [viewer, realtime, tests]
tech-stack:
  added: []
  patterns: [spec-driven rig builder, shared preset-to-rig path, mixed-model viewer regressions]
key-files:
  created: []
  modified:
    - src/realtime/viewer/four_camera_rig.h
    - src/realtime/viewer/four_camera_rig.cpp
    - src/realtime/realtime_scene_factory.cpp
    - tests/test_realtime_scene_factory.cpp
    - tests/test_viewer_four_camera_rig.cpp
key-decisions:
  - "Viewer rig helpers now accept canonical `scene::CameraSpec` inputs instead of hardcoding a local pinhole payload."
  - "Realtime scene factory reuses the viewer rig builder for authored realtime presets so pose, yaw offsets, model, and intrinsics flow through one path."
  - "The legacy generic viewer helper remains available, but now builds through a default `CameraSpec` rather than a raw `Pinhole32Params` block."
requirements-completed: [CAM-04]
duration: 0min
completed: 2026-04-21
---

# Phase 3: Plan 01 Summary

**Viewer rig creation now derives runtime cameras from canonical camera specs, and realtime scene presets flow through the same rig-building path**

## Accomplishments

- Reworked [four_camera_rig.cpp](/home/huangkai/codes/ray_tracing/src/realtime/viewer/four_camera_rig.cpp) so viewer rig construction accepts `scene::CameraSpec` input, rescales authored intrinsics to runtime resolution, and materializes each camera through `CameraRig::add_camera(...)`.
- Updated [realtime_scene_factory.cpp](/home/huangkai/codes/ray_tracing/src/realtime/realtime_scene_factory.cpp) so scene-authored realtime presets now reuse the spec-driven viewer rig path instead of maintaining a second camera-construction implementation.
- Extended [test_realtime_scene_factory.cpp](/home/huangkai/codes/ray_tracing/tests/test_realtime_scene_factory.cpp) and [test_viewer_four_camera_rig.cpp](/home/huangkai/codes/ray_tracing/tests/test_viewer_four_camera_rig.cpp) with authored-equi and mixed-model four-camera coverage.

## Verification

- `cmake --build build --target test_realtime_scene_factory test_viewer_four_camera_rig -j4`
- `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig)$' --output-on-failure`

## Task Commits

1. **Task 1: Make viewer rig construction spec-driven instead of pinhole-hardcoded** - `c8cce19` (feat)
2. **Task 2: Lock the authored-spec and mixed-model viewer rig contract in tests** - `b7b8cb3` (test)

## Deviations from Plan

None.

## Issues Encountered

None during Wave 1 execution.

## User Setup Required

None.
