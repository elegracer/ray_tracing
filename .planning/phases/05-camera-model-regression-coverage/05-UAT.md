---
status: complete
phase: 05-camera-model-regression-coverage
source:
  - 05-01-SUMMARY.md
  - 05-02-SUMMARY.md
  - 05-03-SUMMARY.md
started: 2026-04-22T00:00:00+08:00
updated: 2026-04-22T00:13:00+08:00
---

## Current Test

[testing complete]

## Tests

### 1. Reference Math Parity
expected: Inspect `tests/test_camera_models.cpp` and `tests/reference_camera_shims/`. The low-level camera-model test should directly compare repo math against the bundled reference implementations for both `pinhole32` and `equi62_lut1d`, including a helper-derived default fisheye case.
result: pass

### 2. Offline And GPU Ray Contract
expected: Inspect `tests/camera_contract_fixtures.h`, `tests/test_offline_shared_scene_renderer.cpp`, and `tests/test_optix_direction.cpp`. Offline CPU and realtime GPU direction checks should use the same representative `PackedCamera` fixtures and pixel-center samples to prove the same world-space primary-ray contract.
result: pass

### 3. Cross-Path Smoke Coverage
expected: Inspect `tests/test_reference_vs_realtime.cpp`. It should cover both an explicit authored `pinhole32` path and a helper-derived `equi62_lut1d` default path, with image-level comparisons kept as smoke checks rather than the primary proof surface.
result: pass

### 4. Phase 5 Regression Slice
expected: Running `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure` should pass, confirming reference parity, shared camera-contract checks, authored-vs-default regressions, viewer quality, and live OptiX fisheye coverage all stay green together.
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
