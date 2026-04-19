---
phase: 01-shared-camera-schema
plan: 03
subsystem: scene
tags: [yaml, loader, catalog, camera, schema]
requires:
  - phase: 01-02
    provides: CameraSpec-backed builtin presets and consumer shims
provides:
  - YAML loader enforces explicit camera schema
  - repo-owned YAML scenes author cameras with explicit model and intrinsics
  - catalog regressions prove builtin and file-backed camera data stay aligned
affects: [yaml, tests, viewer, scene-catalog]
tech-stack:
  added: []
  patterns: [explicit-yaml-camera-schema, legacy-field-rejection]
key-files:
  created: []
  modified: [src/scene/yaml_scene_loader.cpp, assets/scenes/cornell_box/scene.yaml, assets/scenes/final_room/scene.yaml, assets/scenes/imported_obj_smoke/scene.yaml, assets/scenes/simple_light/scene.yaml, tests/test_yaml_scene_loader.cpp, tests/test_scene_file_catalog.cpp, tests/test_shared_scene_regression.cpp, tests/test_viewer_scene_reload.cpp, tests/test_viewer_scene_switch_controller.cpp]
key-decisions:
  - "Realtime YAML now declares cameras under realtime.default_view.camera instead of flat vfov fields."
  - "Legacy camera declaration fields are rejected once repo-owned YAML and test fixtures are migrated."
patterns-established:
  - "Project-owned YAML camera declarations must spell out model, width, height, fx, fy, cx, cy, T_bc, and model-specific params."
  - "Regression fixtures should use explicit camera schema rather than relying on implicit vfov parsing."
requirements-completed: [CAM-01, CAM-05, DEF-01]
duration: 35min
completed: 2026-04-20
---

# Phase 1: Plan 03 Summary

**Repo-owned YAML scenes and the YAML loader now use the same explicit `CameraSpec` schema as builtin presets, with legacy camera fields rejected and catalog regressions proving the data survives file-backed loading**

## Performance

- **Duration:** 35 min
- **Started:** 2026-04-20T00:45:00+08:00
- **Completed:** 2026-04-20T01:20:00+08:00
- **Tasks:** 2
- **Files modified:** 10

## Accomplishments

- Reworked `yaml_scene_loader.cpp` to parse canonical explicit camera fields, including `model`, intrinsics, `T_bc`, and model-specific parameter blocks.
- Migrated all repo-owned scene YAML camera declarations to explicit `pinhole32` schema and moved realtime cameras under `default_view.camera`.
- Updated loader, scene-catalog, shared-scene, and viewer fixture tests so file-backed paths verify the same explicit camera data as builtin paths.

## Task Commits

Each task was committed atomically:

1. **Task 1-2: YAML schema migration and regressions** - `f03d890` (feat)

**Plan metadata:** pending in this summary commit

## Files Created/Modified

- `src/scene/yaml_scene_loader.cpp` - Canonical camera parsing, `T_bc` parsing, model validation, and legacy-field rejection.
- `assets/scenes/cornell_box/scene.yaml` - Explicit CPU and realtime camera schema.
- `assets/scenes/final_room/scene.yaml` - Explicit CPU and realtime camera schema with default-viewer intrinsics preserved.
- `assets/scenes/imported_obj_smoke/scene.yaml` - Explicit camera schema for file-backed imported scene.
- `assets/scenes/simple_light/scene.yaml` - Explicit camera schema for CPU and realtime presets.
- `tests/test_yaml_scene_loader.cpp` - Positive/negative coverage for explicit schema, missing model, and legacy field rejection.
- `tests/test_scene_file_catalog.cpp` - File reload fixtures migrated to explicit camera schema.
- `tests/test_shared_scene_regression.cpp` - Asserts catalog/YAML paths preserve model and intrinsics.
- `tests/test_viewer_scene_reload.cpp` - Viewer reload fixtures migrated to explicit realtime camera schema.
- `tests/test_viewer_scene_switch_controller.cpp` - Viewer switch fixtures migrated to explicit realtime camera schema.

## Decisions Made

- Required `T_bc` as an explicit map in canonical YAML rather than inferring identity silently.
- Rejected legacy `vfov`, `vfov_deg`, and `use_default_viewer_intrinsics` fields once repo-owned scenes and fixtures were migrated, instead of keeping a long-lived compatibility branch.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- One loader test that used duplicate CPU preset ids had to be updated with valid camera blocks; otherwise it failed earlier on missing required camera schema and never reached the duplicate-id branch it was meant to cover.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 1 execution is fully complete; the next workflow step is phase verification.
- Phase 2 can now consume shared scene data from a single explicit camera schema across builtin and YAML paths.

---
*Phase: 01-shared-camera-schema*
*Completed: 2026-04-20*
