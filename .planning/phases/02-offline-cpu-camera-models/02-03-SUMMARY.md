---
phase: 02-offline-cpu-camera-models
plan: 03
subsystem: viewer-reference
tags: [camera, offline, viewer, regression, pinhole32, equi62_lut1d]
requires: [02-02]
provides:
  - explicit packed-camera CPU reference coverage for viewer-quality comparisons
  - synchronized regression expectations across viewer and offline shared-scene tests
affects: [offline, viewer, tests]
tech-stack:
  added: []
  patterns: [explicit contract assertions, packed-camera reference coverage, targeted regression slice]
key-files:
  created: []
  modified:
    - tests/test_viewer_quality_reference.cpp
    - .planning/phases/02-offline-cpu-camera-models/02-03-SUMMARY.md
decisions:
  - "The viewer CPU-reference test now treats both pinhole32 and equi62_lut1d packed cameras as supported explicit reference inputs."
  - "The remaining intentionally unsupported explicit-camera cases stay limited to invalid packed-camera dimensions and non-positive focal lengths."
metrics:
  duration: 20min
  completed: 2026-04-20T15:04:52Z
---

# Phase 2 Plan 03: Viewer CPU Reference Contract Summary

**Viewer-quality regression coverage now matches the post-Phase-2 offline camera contract while keeping `render_shared_scene_from_camera(...)` usable as the explicit packed-camera CPU reference path**

## Accomplishments

- Updated [tests/test_viewer_quality_reference.cpp](/home/huangkai/codes/ray_tracing/tests/test_viewer_quality_reference.cpp) so it no longer encodes the removed centered-pinhole-only restriction.
- Added explicit viewer-reference coverage that accepts both the default pinhole rig camera and an explicit `equi62_lut1d` packed camera for CPU-reference rendering.
- Added explicit rejection checks for the remaining unsupported cases: non-positive packed-camera dimensions and non-positive pinhole focal lengths.
- Verified that [tests/test_offline_shared_scene_renderer.cpp](/home/huangkai/codes/ray_tracing/tests/test_offline_shared_scene_renderer.cpp) already stayed aligned with the same consumer contract, so no further edit was needed there.

## Verification

- `cmake --build build --target test_viewer_quality_reference -j4 && ctest --test-dir build -R '^test_viewer_quality_reference$' --output-on-failure`
- `cmake --build build --target test_camera_models test_offline_shared_scene_renderer test_viewer_quality_reference -j4 && ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_viewer_quality_reference)$' --output-on-failure`

## Task Commits

- `bb46808` — `test(02-03): align viewer reference contract`

## Deviations from Plan

None in code. The first attempt to run both verification commands in parallel caused a transient `ctest` `text file is busy` failure for `test_viewer_quality_reference`; rerunning the task-local command serially produced the expected green result.

## Known Stubs

None.

## Threat Flags

None.

## Self-Check: PASSED

- Summary file exists: [02-03-SUMMARY.md](/home/huangkai/codes/ray_tracing/.planning/phases/02-offline-cpu-camera-models/02-03-SUMMARY.md)
- Task commit exists: `bb46808`
