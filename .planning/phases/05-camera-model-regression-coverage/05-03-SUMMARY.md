---
phase: 05-camera-model-regression-coverage
plan: 03
subsystem: testing
tags: [camera, regression, defaults, authored-presets, scene-catalog, viewer, optix]
requires:
  - phase: 05-camera-model-regression-coverage
    provides: reference-backed camera-model parity and shared CPU/GPU camera-contract fixtures
provides:
  - explicit catalog-path assertions that authored pinhole presets remain pinhole
  - a widened final Phase 5 regression slice covering scene, viewer, and live OptiX fisheye paths
  - phase-ready verification state for the completed camera regression milestone
affects: [tests, planning]
tech-stack:
  added: []
  patterns: [explicit authored-vs-default assertions, widened final regression slice]
key-files:
  created: []
  modified:
    - tests/test_shared_scene_regression.cpp
key-decisions:
  - "The final closure kept production code untouched; only regression proof was tightened."
  - "Catalog-path authored pinhole behavior is now asserted directly instead of being inferred from preserved focal lengths or poses."
  - "Phase 5 exits execution on the widened slice, not a narrow math-only subset."
requirements-completed: [VER-04]
completed: 2026-04-22
---

# Phase 5: Plan 03 Summary

**The milestone now closes on an explicit authored-vs-default regression lattice and a fully green widened final slice**

## Accomplishments

- Tightened [`test_shared_scene_regression.cpp`](/home/huangkai/codes/ray_tracing/tests/test_shared_scene_regression.cpp) so catalog-path `cornell_box` and `simple_light` cases assert `pinhole32` directly, not just preserved numeric intrinsics.
- Re-ran the scene-factory, shared-scene, and viewer-reload regression group to confirm helper-generated fisheye defaults still coexist with explicit authored pinhole presets.
- Closed Phase 5 on the widened final slice that includes reference parity, offline seam, OptiX direction, cross-path smoke, viewer quality, shared-scene regressions, reload coverage, and live OptiX equi rendering.

## Verification

- `cmake --build build --target test_realtime_scene_factory test_shared_scene_regression test_viewer_scene_reload -j4`
- `ctest --test-dir build -R '^(test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload)$' --output-on-failure`
- `cmake --build build --target test_camera_models test_offline_shared_scene_renderer test_optix_direction test_reference_vs_realtime test_viewer_quality_reference test_realtime_scene_factory test_shared_scene_regression test_viewer_scene_reload test_optix_equi_path_trace -j4`
- `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure`

## Deviations from Plan

- `test_realtime_scene_factory.cpp` and `test_viewer_scene_reload.cpp` already expressed the required authored/default and equi-reload contracts clearly enough after earlier phases, so no additional code changes were needed there.

## User Setup Required

None.
