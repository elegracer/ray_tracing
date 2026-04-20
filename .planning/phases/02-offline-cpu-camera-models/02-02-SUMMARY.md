---
phase: 02-offline-cpu-camera-models
plan: 02
subsystem: shared-scene
tags: [camera, offline, shared-scene, catalog, pinhole32, equi62_lut1d]
requires: [02-01]
provides:
  - unified offline shared-scene render helper for preset and explicit camera entrypoints
  - shared-scene preset-path proof that camera model selection changes offline output
  - structural regression coverage for file-backed equi CPU presets
affects: [offline, shared-scene, scene-catalog, tests]
tech-stack:
  added: []
  patterns: [single render helper, file-backed temp scenes, catalog-backed regression]
key-files:
  created: []
  modified: [src/core/offline_shared_scene_renderer.cpp, tests/test_offline_shared_scene_renderer.cpp, tests/test_shared_scene_regression.cpp]
key-decisions:
  - "Both public offline entrypoints now pass through one internal render helper rather than open-coding scene loading and rendering twice."
  - "Preset-authored camera model switching is proven with temp file-backed scenes scanned into the global scene catalog."
patterns-established:
  - "Shared-scene offline tests can use temp scene roots plus global_scene_file_catalog().scan_directory(...) to verify render-path behavior."
  - "Structural preset regressions should validate model-specific camera payload preservation through SceneFileCatalog."
requirements-completed: [CAM-02]
duration: 28min
completed: 2026-04-20
---

# Phase 2: Plan 02 Summary

**Offline shared-scene rendering now uses one canonical camera path for both preset and explicit-camera entrypoints, and the shared-scene path has direct proof that camera model changes affect output**

## Performance

- **Duration:** 28 min
- **Started:** 2026-04-20T00:26:00+08:00
- **Completed:** 2026-04-20T00:54:00+08:00
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Collapsed `render_shared_scene(...)` and `render_shared_scene_from_camera(...)` onto one internal helper that loads the scene once and then applies a canonical offline camera configuration.
- Introduced an `OfflineCameraConfig` adapter so preset cameras and explicit `PackedCamera` renders both describe the same offline rendering contract before entering `Camera`.
- Added deterministic integration coverage that scans temp file-backed pinhole and equi scenes into the global catalog, renders both through `render_shared_scene(...)`, and proves the preset path honors `camera.model`.
- Added a structural regression that loads a temp equi YAML scene through `SceneFileCatalog` and verifies the authored CPU preset model/intrinsics survive parsing.

## Task Commits

Each task was committed atomically:

1. **Task 1: Route both offline entrypoints through canonical camera data** - `0f6a274` (feat)
2. **Task 2: Prove shared-scene model switching changes offline output** - `5cef154` (test)

## Verification

- `cmake --build build --target test_offline_shared_scene_renderer test_shared_scene_regression test_scene_file_catalog -j4`
- `ctest --test-dir build -R '^(test_offline_shared_scene_renderer|test_shared_scene_regression|test_scene_file_catalog)$' --output-on-failure`

## Files Created/Modified

- `src/core/offline_shared_scene_renderer.cpp` - Introduced a shared render helper and canonical offline camera adapter for both public offline entrypoints.
- `tests/test_offline_shared_scene_renderer.cpp` - Added temp-catalog shared-scene model-switch integration coverage.
- `tests/test_shared_scene_regression.cpp` - Added file-backed equi preset structural regression coverage.

## Decisions Made

- Kept the shared-scene unification local to `offline_shared_scene_renderer.cpp` instead of expanding the API surface with another public helper.
- Used temp file-backed scenes rather than mutating repo-owned fixtures so the regression stays phase-local and deterministic.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The first temp-scene fixture wrote no files because its parent directories were not created before opening `scene.yaml`.
- The initial temp scene also rendered identically for both models because it had no lighting contribution; switching the background to a non-black color made the model-switch assertion meaningful without adding extra rendering complexity.

## User Setup Required

None.

## Next Phase Readiness

- Phase 2 plan 03 can now treat `render_shared_scene_from_camera(...)` as the stabilized explicit CPU-reference path and focus on downstream viewer regression alignment.

---
*Phase: 02-offline-cpu-camera-models*
*Completed: 2026-04-20*
