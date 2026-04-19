# Ray Tracing

## What This Is

This is a brownfield C++/CUDA ray tracing project with a shared scene pipeline that drives both offline CPU rendering and realtime OptiX rendering. It already ships scene catalogs, file-backed scene loading, a multi-camera realtime viewer, and broad regression coverage. The current milestone extends that renderer so pinhole and fisheye camera models are both first-class across the full pipeline, with fisheye becoming the default.

## Core Value

Camera behavior must stay consistent across offline and realtime rendering so the same scene and rig produce the intended image no matter which path is used.

## Requirements

### Validated

- ✓ Offline shared-scene rendering already works through `render_scene` and `src/core/offline_shared_scene_renderer.cpp` — existing
- ✓ Realtime OptiX rendering and the four-camera viewer already work through `render_realtime`, `render_realtime_viewer`, and `src/realtime/gpu/` — existing
- ✓ The codebase already contains `pinhole32` and `equi62_lut1d` projection helpers in `src/realtime/camera_models.h` and `src/realtime/camera_models.cpp` — existing
- ✓ Builtin and YAML-backed scenes already flow through the shared scene catalog and adapters in `src/scene/` — existing

### Active

- [ ] Promote `pinhole32` and `equi62_lut1d` to full-chain per-camera types across shared scene data, offline rendering, realtime rendering, viewer defaults, and scene definitions.
- [ ] Switch project defaults to `equi62_lut1d` while keeping `pinhole32` available in every render mode and every rig path.
- [ ] Add a utility/script that derives default `fx`, `fy`, `cx`, and `cy` from render resolution and horizontal FOV.
- [ ] Use a `90` degree default horizontal FOV for pinhole and a `120` degree default horizontal FOV for fisheye when deriving default intrinsics.
- [ ] Verify that CPU and realtime paths honor the same per-camera model choice and default intrinsics.

### Out of Scope

- Dynamic render-preset-controlled resolution selection in this milestone — defer until the dual-model pipeline is stable first.
- Runtime-configurable per-camera intrinsics, distortion coefficients, and SE3 extrinsics in this milestone — v1 will use derived `fx/fy/cx/cy` plus zero distortion defaults.
- Additional camera models beyond `pinhole32` and `equi62_lut1d` — keep the scope limited to the two reference-backed models.

## Context

The existing project centers on a backend-agnostic shared scene representation that feeds both CPU and GPU renderers. Camera math already lives in `src/realtime/camera_models.h` and `src/realtime/camera_models.cpp`, where `CameraModelType`, `Pinhole32Params`, and `Equi62Lut1DParams` are defined, but current project defaults and most end-to-end usage still assume the pinhole path.

Reference implementations already exist in `docs/reference/src-cam/cam_pinhole32.h` and `docs/reference/src-cam/cam_equi62_lut1d.h`. The new fisheye work should follow those references the same way the current pinhole work followed the pinhole reference.

The repo already has strong regression coverage across scene loading, camera rigs, offline rendering, realtime rendering, OptiX execution, and viewer behavior under `tests/`. That makes this a compatibility-sensitive brownfield change rather than a greenfield feature.

## Constraints

- **Tech stack**: Stay within the current C++23, CUDA 17, CMake, OptiX, Eigen, and OpenCV stack — the project already builds around these dependencies.
- **Reference parity**: Match the behavior implied by `docs/reference/src-cam/cam_pinhole32.h` and `docs/reference/src-cam/cam_equi62_lut1d.h` — these files define the intended camera model math.
- **Default calibration scope**: v1 uses derived `fx/fy/cx/cy` and zero distortion coefficients for both models — this keeps the first rollout focused and testable.
- **Brownfield safety**: Existing render paths, scene loading, and tests must keep working while defaults move from pinhole to fisheye.
- **Config flexibility**: Existing pinhole-only scene/config assumptions may be changed where necessary — the goal is a clean full-chain dual-model design, not strict text-format compatibility.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Use `docs/reference/src-cam/cam_pinhole32.h` and `docs/reference/src-cam/cam_equi62_lut1d.h` as the math references | The project already uses those references as the intended camera-model source of truth | — Pending |
| Treat `pinhole32` and `equi62_lut1d` as first-class per-camera types across every render mode | The requested feature is full-chain support, not a one-off alternate camera path | — Pending |
| Make fisheye the project default while preserving pinhole everywhere | The desired behavior is dual support with a default switch, not a pinhole removal | — Pending |
| Derive default `fx/fy/cx/cy` from resolution + horizontal FOV before exposing richer calibration | This provides deterministic defaults for both models and reduces the initial integration surface | — Pending |
| Defer dynamic resolution, explicit intrinsics, distortion tuning, and SE3 extrinsics to follow-up phases | Those capabilities are important but materially larger than the initial dual-model rollout | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `$gsd-transition`):
1. Requirements invalidated? -> Move to Out of Scope with reason
2. Requirements validated? -> Move to Validated with phase reference
3. New requirements emerged? -> Add to Active
4. Decisions to log? -> Add to Key Decisions
5. "What This Is" still accurate? -> Update if drifted

**After each milestone** (via `$gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check -> still the right priority?
3. Audit Out of Scope -> reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-19 after initialization*
