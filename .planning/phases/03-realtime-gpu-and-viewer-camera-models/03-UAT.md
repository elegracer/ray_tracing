---
status: complete
phase: 03-realtime-gpu-and-viewer-camera-models
source:
  - .planning/phases/03-realtime-gpu-and-viewer-camera-models/03-01-SUMMARY.md
  - .planning/phases/03-realtime-gpu-and-viewer-camera-models/03-02-SUMMARY.md
  - .planning/phases/03-realtime-gpu-and-viewer-camera-models/03-03-SUMMARY.md
started: 2026-04-21T01:16:40+08:00
updated: 2026-04-21T01:16:40+08:00
---

## Current Test

[testing complete]

## Tests

### 1. Spec-Driven Viewer Rig
expected: Inspect `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/viewer/four_camera_rig.h`, and `src/realtime/realtime_scene_factory.cpp`. You should see viewer rig construction accept canonical `scene::CameraSpec` input, preserve per-camera `model` and scaled `fx/fy/cx/cy`, and let realtime scene factory reuse that same spec-driven rig path instead of rebuilding a separate pinhole-only viewer rig.
result: pass

### 2. OptiX Active Camera Contract
expected: Inspect `src/realtime/gpu/optix_renderer.h`, `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/programs.cu`, and `tests/test_optix_direction.cpp`. `render_direction_debug(...)` should accept an explicit `camera_index`, upload the selected packed camera into the device debug path, and the kernel should color pixels from the actual selected camera ray direction instead of a fixed x/y gradient. The tests should cover both pinhole and equi selection in a mixed-model rig.
result: pass

### 3. File-Backed Realtime Preset Preservation
expected: Inspect `tests/test_realtime_scene_factory.cpp` and `tests/test_viewer_scene_reload.cpp`. You should see temp file-backed `scene.yaml` fixtures authored with `model: equi62_lut1d`, and both the runtime factory path and the viewer reload/rescan path should assert that the authored equi model survives with correctly scaled `fx/fy/cx/cy` instead of collapsing back to pinhole assumptions.
result: pass

### 4. Phase 3 Regression Slice
expected: Running `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace|test_viewer_scene_reload)$' --output-on-failure` should pass, confirming the scene factory, mixed-model viewer rig, OptiX active-camera execution, and file-backed viewer reload flows stay green together.
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
