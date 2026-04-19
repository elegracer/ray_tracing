---
phase: 01-shared-camera-schema
plan: 02
subsystem: scene
tags: [camera, presets, realtime, offline, yaml]
requires:
  - phase: 01-01
    provides: shared CameraSpec contract and rig adapter
provides:
  - builtin CPU and realtime presets author cameras through CameraSpec
  - realtime factory resizes authored intrinsics to requested runtime resolution
  - offline preset consumption reads shared camera schema without vfov-owned preset structs
affects: [yaml, scene-catalog, offline, realtime]
tech-stack:
  added: []
  patterns: [preset-embeds-camera-spec, authored-resolution-runtime-resize]
key-files:
  created: []
  modified: [src/scene/shared_scene_builders.h, src/scene/shared_scene_builders.cpp, src/realtime/realtime_scene_factory.cpp, src/core/offline_shared_scene_renderer.cpp, src/scene/yaml_scene_loader.cpp, tests/test_realtime_scene_factory.cpp, tests/test_offline_shared_scene_renderer.cpp]
key-decisions:
  - "Realtime presets now store authored calibration resolution and resize fx/fy/cx/cy per runtime request."
  - "YAML keeps a temporary legacy parse shim so the project compiles until repo-owned scene files migrate in plan 03."
patterns-established:
  - "Builtin preset tables should encode camera model and intrinsics explicitly through CameraSpec."
  - "Runtime factories should preserve authored intrinsics at calibration size and scale them proportionally when output resolution changes."
requirements-completed: [CAM-01, CAM-05, DEF-01]
duration: 25min
completed: 2026-04-20
---

# Phase 1: Plan 02 Summary

**Builtin CPU and realtime presets now carry explicit `CameraSpec` payloads, and realtime rig creation preserves or rescales authored intrinsics instead of rebuilding them from legacy preset fields**

## Performance

- **Duration:** 25 min
- **Started:** 2026-04-20T00:20:00+08:00
- **Completed:** 2026-04-20T00:45:00+08:00
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments

- Embedded `CameraSpec` into `CpuCameraPreset` and `RealtimeViewPreset`, replacing builtin `vfov_deg` and `use_default_viewer_intrinsics` ownership.
- Migrated builtin scene preset authoring to explicit pinhole model and intrinsics, including `final_room`’s old default-viewer intrinsics path.
- Updated realtime rig construction to preserve authored intrinsics at calibration size and scale them for resized output requests, with test coverage for both modes.

## Task Commits

Each task was committed atomically:

1. **Task 1-2: Builtin presets and consumer shims** - `20fc819` (feat)

**Plan metadata:** pending in this summary commit

## Files Created/Modified

- `src/scene/shared_scene_builders.h` - Preset structs now embed `CameraSpec`.
- `src/scene/shared_scene_builders.cpp` - Builtin CPU/realtime presets now author explicit camera model and intrinsics.
- `src/realtime/realtime_scene_factory.cpp` - Realtime rig creation adapts from authored `CameraSpec` and rescales intrinsics for runtime dimensions.
- `src/core/offline_shared_scene_renderer.cpp` - Offline preset consumption derives its current vfov behavior from `CameraSpec` instead of a vfov-owned preset struct.
- `src/scene/yaml_scene_loader.cpp` - Temporary compatibility shim maps legacy YAML vfov fields into the new preset structs until plan 03 migrates repo-owned YAML.
- `tests/test_realtime_scene_factory.cpp` - Covers same-size preservation, resized-output scaling, and explicit authored default-viewer intrinsics.
- `tests/test_offline_shared_scene_renderer.cpp` - Asserts builtin CPU presets now expose explicit `CameraSpec` data.

## Decisions Made

- Chose `640x480` as the authored calibration resolution for builtin realtime presets so current viewer-path tests keep the same baseline behavior.
- Kept offline renderer behavior unchanged in substance by deriving its current vfov from `CameraSpec`, leaving actual offline model switching for Phase 2.

## Deviations from Plan

### Auto-fixed Issues

**1. Temporary legacy YAML parse shim**
- **Found during:** Task 1 (preset struct migration)
- **Issue:** Changing preset structs would have broken `yaml_scene_loader.cpp` and blocked core compilation before plan 03 migrates repo-owned YAML files.
- **Fix:** Added a narrow compatibility mapping from legacy YAML vfov fields into the new `CameraSpec`-backed structs.
- **Files modified:** `src/scene/yaml_scene_loader.cpp`
- **Verification:** `ctest --test-dir build -R '^(test_realtime_scene_factory|test_offline_shared_scene_renderer)$' --output-on-failure`
- **Committed in:** `20fc819`

---

**Total deviations:** 1 auto-fixed
**Impact on plan:** Limited to a compile-time bridge; repo-owned YAML content still remains to be migrated explicitly in plan 03.

## Issues Encountered

None beyond the planned compatibility bridge.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03 can now migrate repo-owned YAML scenes onto the same `CameraSpec` field layout used by builtin presets.
- The temporary YAML compatibility path added here should be removable once explicit YAML camera declarations land.

---
*Phase: 01-shared-camera-schema*
*Completed: 2026-04-20*
