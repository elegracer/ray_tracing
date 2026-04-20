---
phase: 02-offline-cpu-camera-models
plan: 01
subsystem: offline-rendering
tags: [camera-models, offline-renderer, pinhole32, equi62_lut1d, testing]
requires:
  - phase: 01-shared-camera-schema
    provides: canonical shared camera model and packed camera contracts
provides:
  - model-aware offline primary rays for pinhole32 and equi62_lut1d
  - shared-camera adapter from CpuRenderPreset and PackedCamera into the legacy CPU tracer
  - seam-level regression coverage for ray parity, rectangular fisheye output, and pinhole-only defocus
affects: [offline-shared-scene-renderer, viewer-reference, phase-02-plan-02]
tech-stack:
  added: []
  patterns: [single primary-ray seam in Camera, shared unprojection reused across offline and realtime]
key-files:
  created: []
  modified:
    - src/common/camera.h
    - src/core/offline_shared_scene_renderer.cpp
    - tests/test_offline_shared_scene_renderer.cpp
key-decisions:
  - "Offline primary rays now branch by CameraModelType inside Camera while keeping the existing render loop unchanged."
  - "CpuRenderPreset and PackedCamera both adapt into shared unprojection math, with defocus enabled only for pinhole32."
patterns-established:
  - "Packed/shared camera data supplies model and intrinsics; offline preset lookfrom/lookat/vup now provide pose only."
  - "Tests validate ray-level parity against unproject_* helpers before relying on image-level smoke checks."
requirements-completed: [CAM-02]
duration: 23min
completed: 2026-04-20
---

# Phase 2 Plan 01: Offline Camera Seam Summary

**Shared pinhole32 and equi62_lut1d unprojection now drive offline CPU primary rays while preserving the existing tracer loop and pinhole-only depth of field**

## Performance

- **Duration:** 23 min
- **Started:** 2026-04-20T14:32:00Z
- **Completed:** 2026-04-20T14:54:37Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Replaced the legacy pinhole-only primary-ray construction seam with a shared-camera seam inside [`src/common/camera.h`](/home/huangkai/codes/ray_tracing/src/common/camera.h).
- Updated [`src/core/offline_shared_scene_renderer.cpp`](/home/huangkai/codes/ray_tracing/src/core/offline_shared_scene_renderer.cpp) so both preset-driven and packed-camera offline entrypoints feed canonical pinhole/equi data into the same tracer loop.
- Added seam-level regression coverage in [`tests/test_offline_shared_scene_renderer.cpp`](/home/huangkai/codes/ray_tracing/tests/test_offline_shared_scene_renderer.cpp) for shared-math ray parity, rectangular equi output, and pinhole-only defocus behavior.

## Task Commits

1. **Task 1: Add a model-aware offline ray-emission seam** - `afa02b8` (`feat`)
2. **Task 2: Lock the Phase 2 camera contract at the offline seam** - `3809f72` (`test`)

## Files Created/Modified

- `src/common/camera.h` - Added `SharedCameraRayConfig`, shared `unproject_*` primary-ray emission, and a debug seam helper used by contract tests.
- `src/core/offline_shared_scene_renderer.cpp` - Adapted `CpuRenderPreset` and `PackedCamera` into the shared camera seam and gated defocus to `pinhole32`.
- `tests/test_offline_shared_scene_renderer.cpp` - Added seam-level pinhole/equi ray checks and packed-camera render assertions.

## Decisions Made

- Reused `rt::unproject_pinhole32(...)` and `rt::unproject_equi62_lut1d(...)` directly in the offline seam to prevent offline/realtime math drift.
- Kept fisheye output rectangular and left fisheye depth of field disabled by construction rather than inventing a second blur model.

## Verification

- `cmake --build build --target test_camera_models test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer)$' --output-on-failure`
  Result: passed (`2/2` tests, `0` failures)
- `cmake --build build --target test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^test_offline_shared_scene_renderer$' --output-on-failure`
  Result: passed (`1/1` test, `0` failures)
- `rg -n "unproject_pinhole32|unproject_equi62_lut1d|CameraModelType" src/common/camera.h src/core/offline_shared_scene_renderer.cpp`
  Result: shared camera-model seam usage confirmed in both implementation files

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- Initial compilation failed because the new shared-camera types in `Camera` needed `rt::` namespace qualification. After correcting that, the targeted verification commands passed cleanly.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Offline shared-scene rendering now accepts canonical pinhole and equi camera data through a single primary-ray seam.
- Phase 2 follow-up work can build on these seam-level guarantees without replacing the CPU tracer loop.

## Self-Check: PASSED

- Summary file exists at `.planning/phases/02-offline-cpu-camera-models/02-01-SUMMARY.md`.
- Task commits `afa02b8` and `3809f72` are present in `git log`.
