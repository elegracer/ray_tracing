---
phase: 01-shared-camera-schema
plan: 01
subsystem: scene
tags: [camera, schema, rig, pinhole32, equi62_lut1d]
requires: []
provides:
  - shared authored camera schema with explicit model and intrinsics
  - CameraRig adapter from CameraSpec to runtime camera params
  - mixed-model rig regression coverage for schema-driven cameras
affects: [scene, realtime, yaml, presets]
tech-stack:
  added: []
  patterns: [header-only shared schema, authored-to-runtime adapter]
key-files:
  created: [src/scene/camera_spec.h]
  modified: [src/realtime/camera_rig.h, src/realtime/camera_rig.cpp, tests/test_camera_rig.cpp]
key-decisions:
  - "CameraSpec stays in src/scene as authored state and does not embed renderer-derived LUT storage."
  - "CameraRig remains the boundary that converts authored camera specs into runtime math structs."
patterns-established:
  - "Shared scene code should author cameras through CameraSpec rather than PackedCamera."
  - "Realtime packing should adapt from CameraSpec while preserving existing add_pinhole/add_equi62 paths."
requirements-completed: [CAM-01, CAM-05]
duration: 20min
completed: 2026-04-20
---

# Phase 1: Plan 01 Summary

**Shared `CameraSpec` now carries canonical per-camera model and intrinsics, and `CameraRig` can pack both supported models directly from that authored schema**

## Performance

- **Duration:** 20 min
- **Started:** 2026-04-20T00:00:00+08:00
- **Completed:** 2026-04-20T00:20:00+08:00
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Added `src/scene/camera_spec.h` as the authored camera contract with explicit `model`, shared intrinsics, `T_bc`, and zero-default distortion slots.
- Added `CameraRig::add_camera(const scene::CameraSpec&)` to convert authored cameras into existing pinhole/equi runtime params.
- Extended `test_camera_rig` to cover mixed-model packing through the new shared schema path and default-zero distortion behavior.

## Task Commits

Each task was committed atomically:

1. **Task 1-2: CameraSpec contract and rig adapter** - `a466062` (feat)

**Plan metadata:** pending in this summary commit

## Files Created/Modified

- `src/scene/camera_spec.h` - Canonical authored camera schema with pinhole and equi62 parameter slots.
- `src/realtime/camera_rig.h` - Declares `add_camera` for shared-schema ingestion.
- `src/realtime/camera_rig.cpp` - Converts `CameraSpec` into existing runtime param structs and packs them.
- `tests/test_camera_rig.cpp` - Verifies direct runtime path and schema-driven mixed-model path both stay correct.

## Decisions Made

- Kept `CameraSpec` header-only and limited to authored state; derived LUT data remains runtime-only.
- Reused existing runtime math structs instead of inventing a second camera parameter vocabulary.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The first authored-translation assertion used the wrong renderer-frame expectation for body Y translation. Fixed the test to match `body_to_renderer_matrix()` rather than changing production code.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 1 plan 02 can now move builtin presets and consumers onto `CameraSpec` without inventing another schema type.
- YAML migration in plan 03 can reuse the same field names and zero-default distortion behavior.

---
*Phase: 01-shared-camera-schema*
*Completed: 2026-04-20*
