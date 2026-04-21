# Ray Tracing

## Current State

- **Latest shipped version:** `v1.0` on `2026-04-22`
- **Milestone result:** shipped dual-model camera support across shared scene, offline CPU, realtime GPU, viewer, defaults, and regression coverage
- **Audit:** `.planning/v1.0-MILESTONE-AUDIT.md` passed with `15/15` requirements satisfied

## What This Is

This is a brownfield C++/CUDA ray tracing project with a shared scene pipeline that drives both offline CPU rendering and realtime OptiX rendering. It ships scene catalogs, file-backed scene loading, a multi-camera realtime viewer, and regression coverage that now spans both `pinhole32` and `equi62_lut1d` camera models across the full pipeline, with fisheye as the default construction path.

## Core Value

Camera behavior must stay consistent across offline and realtime rendering so the same scene and rig produce the intended image no matter which path is used.

## Requirements

### Validated

- ✓ Offline shared-scene rendering already works through `render_scene` and `src/core/offline_shared_scene_renderer.cpp` — existing
- ✓ Realtime OptiX rendering and the four-camera viewer already work through `render_realtime`, `render_realtime_viewer`, and `src/realtime/gpu/` — existing
- ✓ The codebase already contains `pinhole32` and `equi62_lut1d` projection helpers in `src/realtime/camera_models.h` and `src/realtime/camera_models.cpp` — existing
- ✓ Builtin and YAML-backed scenes already flow through the shared scene catalog and adapters in `src/scene/` — existing
- ✓ `pinhole32` and `equi62_lut1d` are first-class per-camera types across shared scene, offline rendering, realtime rendering, and viewer paths — `v1.0`
- ✓ Project defaults now resolve to `equi62_lut1d` while explicit pinhole support remains available everywhere — `v1.0`
- ✓ Default `fx/fy/cx/cy` derivation from `resolution + hfov` is implemented with `90` degree pinhole and `120` degree fisheye defaults — `v1.0`
- ✓ Automated coverage now anchors camera math to the bundled reference headers and checks CPU/GPU cross-path camera contracts — `v1.0`

### Active

No active milestone requirements are defined yet.

### Out of Scope

- Dynamic render-preset-controlled resolution selection in `v1.0` — deferred to the next milestone.
- Runtime-configurable per-camera intrinsics, distortion coefficients, and SE3 extrinsics in `v1.0` — deferred until after the default dual-model path stabilized.
- Additional camera models beyond `pinhole32` and `equi62_lut1d` — keep the scope limited to the two reference-backed models.

## Context

The existing project centers on a backend-agnostic shared scene representation that feeds both CPU and GPU renderers. Camera math lives in `src/realtime/camera_models.h` and `src/realtime/camera_models.cpp`, and `v1.0` standardized the authored/runtime camera contract around explicit shared camera specs, model-aware offline and realtime ray generation, and fisheye-first default construction.

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
| Use `docs/reference/src-cam/cam_pinhole32.h` and `docs/reference/src-cam/cam_equi62_lut1d.h` as the math references | The project already uses those references as the intended camera-model source of truth | Shipped in `v1.0` |
| Treat `pinhole32` and `equi62_lut1d` as first-class per-camera types across every render mode | The requested feature is full-chain support, not a one-off alternate camera path | Shipped in `v1.0` |
| Make fisheye the project default while preserving pinhole everywhere | The desired behavior is dual support with a default switch, not a pinhole removal | Shipped in `v1.0` |
| Derive default `fx/fy/cx/cy` from resolution + horizontal FOV before exposing richer calibration | This provides deterministic defaults for both models and reduces the initial integration surface | Shipped in `v1.0` |
| Defer dynamic resolution, explicit intrinsics, distortion tuning, and SE3 extrinsics to follow-up phases | Those capabilities are important but materially larger than the initial dual-model rollout | Deferred past `v1.0` |

## Next Milestone Goals

- Add render-preset-controlled dynamic output resolution.
- Support explicit per-camera `fx`, `fy`, `cx`, and `cy` instead of only derived defaults.
- Support model-specific distortion coefficients for `pinhole32` and `equi62_lut1d`.
- Support configurable body-relative camera extrinsics through scene or preset data.

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
*Last updated: 2026-04-22 after v1.0 milestone close*
