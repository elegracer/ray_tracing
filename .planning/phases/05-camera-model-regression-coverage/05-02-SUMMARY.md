---
phase: 05-camera-model-regression-coverage
plan: 02
subsystem: camera-regression
tags: [camera, regression, offline, optix, direction, smoke, pinhole32, equi62_lut1d]
requires: [05-01]
provides:
  - shared packed-camera fixtures for offline and GPU ray-contract tests
  - explicit pinhole plus helper-derived equi smoke coverage in cross-path regression
  - direction-level agreement checks for offline and OptiX camera seams
affects: [tests, realtime-gpu, offline-renderer]
tech-stack:
  added: [test-only shared camera fixture header]
  patterns: [shared packed-camera fixture, world-ray contract, smoke-plus-parity layering]
key-files:
  created:
    - tests/camera_contract_fixtures.h
  modified:
    - tests/test_offline_shared_scene_renderer.cpp
    - tests/test_optix_direction.cpp
    - tests/test_reference_vs_realtime.cpp
key-decisions:
  - "Offline and GPU tests now consume the same representative packed-camera fixtures and pixel-center samples."
  - "Cross-path smoke coverage now includes both an explicit authored pinhole path and a helper-derived default fisheye path."
  - "Image-level comparisons remain smoke checks; the ray-contract proof stays in the offline and OptiX direction tests."
requirements-completed: [VER-03]
completed: 2026-04-22
---

# Phase 5: Plan 02 Summary

**Offline CPU and realtime GPU now prove the same world-space camera contract, with smoke coverage for explicit pinhole and helper-derived fisheye defaults**

## Accomplishments

- Added [`tests/camera_contract_fixtures.h`](/home/huangkai/codes/ray_tracing/tests/camera_contract_fixtures.h) so the offline seam and OptiX direction test share the same representative `PackedCamera` fixtures and pixel-center samples.
- Updated [`test_offline_shared_scene_renderer.cpp`](/home/huangkai/codes/ray_tracing/tests/test_offline_shared_scene_renderer.cpp) and [`test_optix_direction.cpp`](/home/huangkai/codes/ray_tracing/tests/test_optix_direction.cpp) to validate the same contract cameras in world space, instead of using unrelated local fixtures.
- Reworked [`test_reference_vs_realtime.cpp`](/home/huangkai/codes/ray_tracing/tests/test_reference_vs_realtime.cpp) into a lightweight smoke test with two explicit cases: authored pinhole and helper-derived default fisheye.

## Verification

- `cmake --build build --target test_offline_shared_scene_renderer test_optix_direction -j4`
- `ctest --test-dir build -R '^(test_offline_shared_scene_renderer|test_optix_direction)$' --output-on-failure`
- `cmake --build build --target test_reference_vs_realtime test_offline_shared_scene_renderer test_optix_direction -j4`
- `ctest --test-dir build -R '^(test_reference_vs_realtime|test_offline_shared_scene_renderer|test_optix_direction)$' --output-on-failure`

## Deviations from Plan

- None. The ray-contract proof stayed at the test seam, and the smoke layer stayed intentionally coarse.

## User Setup Required

None.
