---
status: complete
phase: 04-default-intrinsics-and-fisheye-defaults
source:
  - .planning/phases/04-default-intrinsics-and-fisheye-defaults/04-01-SUMMARY.md
  - .planning/phases/04-default-intrinsics-and-fisheye-defaults/04-02-SUMMARY.md
  - .planning/phases/04-default-intrinsics-and-fisheye-defaults/04-03-SUMMARY.md
started: 2026-04-21T23:20:00+08:00
updated: 2026-04-21T23:24:00+08:00
---

## Current Test

[testing complete]

## Tests

### 1. Default Intrinsics Utility
expected: Inspect `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp`, and `utils/derive_default_camera_intrinsics.cpp`, or run `./bin/derive_default_camera_intrinsics --model pinhole32 --width 640 --height 480 --hfov-deg 90` and `./bin/derive_default_camera_intrinsics --model equi62_lut1d --width 640 --height 480 --hfov-deg 120`. You should see one shared helper deriving defaults, with `640x480` outputs of `320/320/320/240` for pinhole and approximately `305.57749/305.57749/320/240` for equi.
result: pass

### 2. Builtin Default Vs Explicit Authored Presets
expected: Inspect `src/scene/shared_scene_builders.cpp`, `tests/test_shared_scene_builders.cpp`, and `tests/test_realtime_scene_factory.cpp`. Builtin helper-generated defaults such as `earth_sphere` should now author `equi62_lut1d`, while explicitly authored catalog/file-backed presets such as `smoke`, `final_room`, and `cornell_box` should remain `pinhole32`.
result: pass

### 3. Viewer Default Rig
expected: Inspect `src/realtime/viewer/four_camera_rig.cpp`, `tests/test_viewer_four_camera_rig.cpp`, and `tests/test_viewer_quality_reference.cpp`. The no-argument viewer rig should now fabricate `equi62_lut1d` cameras by default with shared 120-degree default intrinsics, while explicit pinhole and explicit equi authored viewer cameras should still pack and render correctly.
result: pass

### 4. Phase 4 Regression Slice
expected: Running `ctest --test-dir build -R '^(test_camera_models|test_shared_scene_builders|test_realtime_scene_factory|test_viewer_four_camera_rig|test_viewer_quality_reference)$' --output-on-failure` should pass, confirming the shared default-intrinsics helper, builtin default switch, and viewer default switch stay green together.
result: pass

## Summary

total: 4
passed: 4
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

None yet.
