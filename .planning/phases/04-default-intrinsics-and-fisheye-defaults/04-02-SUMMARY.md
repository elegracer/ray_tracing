---
phase: 04-default-intrinsics-and-fisheye-defaults
plan: 02
subsystem: shared-scene-defaults
tags: [camera, defaults, shared-scene, presets, pinhole32, equi62_lut1d]
requires:
  - phase: 04-default-intrinsics-and-fisheye-defaults
    provides: canonical default intrinsics helper and default HFOV constants
provides:
  - fisheye-first builtin CPU and realtime default helper paths
  - explicit-pinhole preservation for authored/file-backed scene presets
  - builder/runtime regression coverage for implicit-default vs explicit-pinhole behavior
affects: [shared-scene, runtime-factory, tests]
tech-stack:
  added: []
  patterns: [helper-driven default cameras, catalog-level authored override preservation]
key-files:
  created: []
  modified:
    - src/scene/shared_scene_builders.cpp
    - tests/test_shared_scene_builders.cpp
    - tests/test_realtime_scene_factory.cpp
key-decisions:
  - "Helper-generated builtin scene presets now ignore legacy per-scene `vfov` defaults and derive equi intrinsics from the shared 120-degree default HFOV."
  - "Repo-owned explicit authored presets that already carry pinhole intrinsics, including file-backed scenes, remain pinhole."
  - "Regression coverage distinguishes builtin-only default helpers from catalog/file-backed authored overrides."
requirements-completed: [DEF-02, DEF-03]
completed: 2026-04-21
---

# Phase 4: Plan 02 Summary

**Builtin default scene cameras now switch to fisheye through the shared helper, while explicit authored pinhole presets remain untouched**

## Accomplishments

- Updated [`shared_scene_builders.cpp`](/home/huangkai/codes/ray_tracing/src/scene/shared_scene_builders.cpp) so helper-generated builtin CPU and realtime presets derive `CameraSpec` from the new shared default-intrinsics helper and default to `equi62_lut1d`.
- Kept an explicit pinhole CPU helper path for the remaining intentionally pinhole-authored builtin preset slice, instead of letting it inherit the new fisheye default silently.
- Reworked [`test_shared_scene_builders.cpp`](/home/huangkai/codes/ray_tracing/tests/test_shared_scene_builders.cpp) and [`test_realtime_scene_factory.cpp`](/home/huangkai/codes/ray_tracing/tests/test_realtime_scene_factory.cpp) so they now prove three separate behaviors:
  - builtin-only defaults like `earth_sphere` switch to equi
  - explicit catalog-authored presets like `cornell_box` remain pinhole
  - runtime rig packing and resize logic preserve the new default model contract

## Verification

- `cmake --build build --target test_shared_scene_builders test_realtime_scene_factory -j4`
- `ctest --test-dir build -R '^(test_shared_scene_builders|test_realtime_scene_factory)$' --output-on-failure`

## Deviations from Plan

- `cornell_box` could not be used as the “helper-generated default switched to equi” proof point because the scene catalog resolves it through an explicit file-backed YAML preset. The regression was updated to use builtin-only `earth_sphere` for the default-switch assertion and keep `cornell_box` as the authored-pinhole preservation proof instead.

## User Setup Required

None.
