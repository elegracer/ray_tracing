---
phase: 02-offline-cpu-camera-models
plan: 01
subsystem: offline-renderer
tags: [camera, offline, pinhole32, equi62_lut1d, cpu]
requires: []
provides:
  - model-aware offline primary-ray seam backed by shared camera math
  - pinhole-only depth-of-field policy for the CPU renderer
  - seam-level regression coverage for pinhole and fisheye offline rays
affects: [offline, shared-scene, tests]
tech-stack:
  added: []
  patterns: [shared-math reuse, tracer-seam extension, seam-level regression]
key-files:
  created: []
  modified: [src/common/camera.h, src/core/offline_shared_scene_renderer.cpp, tests/test_offline_shared_scene_renderer.cpp]
key-decisions:
  - "Camera keeps the existing tracer loop and gains an optional canonical shared-camera ray configuration."
  - "Offline shared-scene rendering reuses unproject_pinhole32 and unproject_equi62_lut1d instead of duplicating camera math."
  - "Depth of field remains pinhole-only; equi62_lut1d always renders from the camera center."
patterns-established:
  - "Offline code should configure canonical camera math through Camera::SharedCameraRayConfig rather than reconstructing vfov viewports."
  - "Offline seam tests should assert ray-direction parity against shared camera-model helpers directly."
requirements-completed: [CAM-02]
duration: 26min
completed: 2026-04-20
---

# Phase 2: Plan 01 Summary

**The offline CPU renderer now emits primary rays from the same shared pinhole/equi camera math used elsewhere, without forking the tracer loop**

## Performance

- **Duration:** 26 min
- **Started:** 2026-04-20T00:00:00+08:00
- **Completed:** 2026-04-20T00:26:00+08:00
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Extended `Camera` with an optional `SharedCameraRayConfig` plus `debug_primary_ray(...)`, letting offline rendering switch from legacy `vfov` viewport rays to canonical `pinhole32` / `equi62_lut1d` unprojection while keeping the existing render loop intact.
- Reworked `offline_shared_scene_renderer.cpp` so shared-scene presets and explicit packed cameras can both configure the offline camera through canonical model/intrinsics data, with pinhole depth of field preserved and fisheye forced to no-defocus.
- Added seam-level tests that check pinhole and equi ray-direction parity against shared camera math, verify the defocus policy split, and confirm explicit equi offline renders keep full rectangular dimensions.

## Task Commits

Each task was committed atomically:

1. **Task 1: Model-aware offline ray-emission seam** - `afa02b8` (feat)
2. **Task 2: Seam-level offline camera contract tests** - `3809f72` (test)

## Verification

- `cmake --build build --target test_camera_models test_offline_shared_scene_renderer -j4`
- `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer)$' --output-on-failure`

## Files Created/Modified

- `src/common/camera.h` - Added the canonical shared-camera ray configuration path while preserving the existing CPU tracer loop.
- `src/core/offline_shared_scene_renderer.cpp` - Adapts preset and packed-camera inputs into shared-camera offline ray configuration.
- `tests/test_offline_shared_scene_renderer.cpp` - Adds ray-parity, defocus-policy, and rectangular-output assertions for the new seam.

## Decisions Made

- Reused `unproject_pinhole32(...)` and `unproject_equi62_lut1d(...)` directly for offline primary rays instead of building another camera-math layer.
- Kept the shared-camera integration optional inside `Camera` so existing non-shared-scene callers can still use the legacy viewport path unchanged.
- Preserved pinhole depth of field by applying the defocus disk only when the configured model is `pinhole32`.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- The first pass failed to compile because `camera.h` referenced camera-model types without the `rt::` namespace qualifier. Fixing the namespaced references was sufficient; no design change was needed.

## User Setup Required

None.

## Next Phase Readiness

- Phase 2 plan 02 can now route both offline entrypoints through the same canonical camera adapter instead of preserving a pinhole-only branch.
- Later regression work can build on `debug_primary_ray(...)` and the new seam-level tests when checking cross-path parity.

---
*Phase: 02-offline-cpu-camera-models*
*Completed: 2026-04-20*
