---
status: complete
phase: 01-shared-camera-schema
source:
  - .planning/phases/01-shared-camera-schema/01-01-SUMMARY.md
  - .planning/phases/01-shared-camera-schema/01-02-SUMMARY.md
  - .planning/phases/01-shared-camera-schema/01-03-SUMMARY.md
started: 2026-04-20T01:30:00+08:00
updated: 2026-04-20T01:39:00+08:00
---

## Current Test

[testing complete]

## Tests

### 1. Shared Camera Contract
expected: Inspect `src/scene/camera_spec.h` and `src/realtime/camera_rig.h`. You should see a single authored `rt::scene::CameraSpec` with explicit `model`, `width`, `height`, `fx`, `fy`, `cx`, `cy`, `T_bc`, plus model-specific slots for `pinhole32` and `equi62_lut1d`. You should also see `CameraRig` expose `add_camera(const scene::CameraSpec&)`, so shared scene code can hand a canonical camera description to the runtime rig without going through `PackedCamera`.
result: pass

### 2. Explicit Preset Schema
expected: Inspect builtin preset code and repo-owned scene YAML such as `src/scene/shared_scene_builders.cpp`, `assets/scenes/final_room/scene.yaml`, or `assets/scenes/cornell_box/scene.yaml`. CPU and realtime cameras should now be authored with explicit `model`, `width`, `height`, `fx`, `fy`, `cx`, `cy`, and `T_bc` fields, rather than legacy `vfov`, `vfov_deg`, or `use_default_viewer_intrinsics` camera declarations.
result: pass

### 3. YAML Loader Canonical Parsing
expected: Inspect `src/scene/yaml_scene_loader.cpp` and `tests/test_yaml_scene_loader.cpp`. The loader should require explicit camera schema, reject missing `camera.model`, and reject legacy camera fields instead of silently mapping them.
result: pass

### 4. Phase 1 Regression Slice
expected: Running `ctest --test-dir build -R '^(test_camera_rig|test_realtime_scene_factory|test_offline_shared_scene_renderer|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_viewer_scene_reload|test_viewer_scene_switch_controller)$' --output-on-failure` should pass cleanly, confirming the shared camera schema, builtin/file-backed scene paths, and viewer reload fixtures all stay green.
result: pass

## Summary

total: 4
passed: 4
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps
