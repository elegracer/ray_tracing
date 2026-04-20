---
status: complete
phase: 02-offline-cpu-camera-models
source:
  - 02-01-SUMMARY.md
  - 02-02-SUMMARY.md
  - 02-03-SUMMARY.md
started: 2026-04-20T15:13:17Z
updated: 2026-04-20T15:44:49Z
---

## Current Test

[testing complete]

## Tests

### 1. Offline Camera Seam
expected: Inspect `src/common/camera.h` and `src/core/offline_shared_scene_renderer.cpp`. The offline CPU path is configured from canonical shared camera data for both `pinhole32` and `equi62_lut1d`, with pinhole depth of field preserved and fisheye forced to no-defocus.
result: pass

### 2. Unified Shared-Scene Entry Points
expected: Inspect `src/core/offline_shared_scene_renderer.cpp` and `tests/test_offline_shared_scene_renderer.cpp`. `render_shared_scene(...)` and `render_shared_scene_from_camera(...)` should both route through the same canonical camera adapter path, and tests should cover both explicit packed-camera switching and temp-catalog shared-scene preset switching.
result: pass

### 3. Viewer CPU Reference Contract
expected: Inspect `tests/test_viewer_quality_reference.cpp`. Viewer CPU-reference coverage should accept both the default pinhole rig camera and an explicit `equi62_lut1d` packed camera, while explicit rejection remains limited to invalid packed-camera dimensions and non-positive pinhole focal lengths.
result: pass

### 4. Phase 2 Regression Slice
expected: Running `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_shared_scene_regression|test_viewer_quality_reference)$' --output-on-failure` should pass, confirming the offline camera seam, shared-scene model switching, and viewer reference regressions stay green together.
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
