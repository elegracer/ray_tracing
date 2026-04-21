# Roadmap: Ray Tracing

## Overview

This brownfield milestone upgrades the existing shared-scene renderer so `pinhole32` and `equi62_lut1d` are first-class per-camera models across scene definitions, the offline CPU renderer, the realtime GPU/viewer path, and the default calibration flow. The work is sequenced to extend the current pipeline safely: carry model choice through shared scene data first, teach each render path to honor it, switch defaults to fisheye with a deterministic intrinsics utility, and then lock the rollout down with regression coverage.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Shared Camera Schema** - Carry per-camera model selection and common intrinsics through shared scene and rig data.
- [x] **Phase 2: Offline CPU Camera Models** - Make the offline renderer honor pinhole and fisheye selection from shared scene inputs.
- [x] **Phase 3: Realtime GPU And Viewer Camera Models** - Preserve per-camera model choice through realtime packing, rendering, and viewer rigs.
- [x] **Phase 4: Default Intrinsics And Fisheye Defaults** - Switch default camera construction to fisheye and derive v1 intrinsics from resolution plus horizontal FOV.
- [ ] **Phase 5: Camera Model Regression Coverage** - Prove reference math parity and cross-path consistency after the default switch.

## Phase Details

### Phase 1: Shared Camera Schema
**Goal**: Scene definitions and shared camera data can represent either supported camera model per camera, along with the v1 common intrinsics needed by both render backends.
**Depends on**: Nothing (first phase)
**Requirements**: CAM-01, CAM-05, DEF-01
**Success Criteria** (what must be TRUE):
  1. Developer can declare each camera in builtin scenes and YAML-backed scene configs as either `pinhole32` or `equi62_lut1d`.
  2. Shared scene and rig structures preserve each camera's selected model plus `fx`, `fy`, `cx`, and `cy`, with v1 distortion coefficients defaulting to zero.
  3. Loading the same scene through builtin and file-backed catalog paths keeps the declared per-camera model data intact for downstream renderers.
**Plans**: 3 plans
Plans:
- [x] 01-01-PLAN.md - Define the canonical CameraSpec contract and rig-packing adapter boundary.
- [x] 01-02-PLAN.md - Move builtin scene presets and shared-scene consumers onto CameraSpec.
- [x] 01-03-PLAN.md - Migrate YAML camera declarations and structural regressions onto the canonical schema.

### Phase 2: Offline CPU Camera Models
**Goal**: Offline CPU rendering uses the selected per-camera model from shared scene data instead of falling back to pinhole-only ray generation.
**Depends on**: Phase 1
**Requirements**: CAM-02
**Success Criteria** (what must be TRUE):
  1. Offline rendering of a pinhole-configured camera uses `pinhole32` ray generation through the shared-scene pipeline.
  2. Offline rendering of a fisheye-configured camera uses `equi62_lut1d` ray generation through the same offline path.
  3. Switching a scene or rig camera between the two supported models changes offline ray generation without requiring a separate code path or scene format.
**Plans**: 3 plans
Plans:
- [x] 02-01-PLAN.md - Introduce the model-aware offline primary-ray seam without forking the CPU tracer.
- [x] 02-02-PLAN.md - Route shared-scene offline entrypoints through canonical camera data and prove preset-path model switching.
- [x] 02-03-PLAN.md - Align viewer CPU-reference regressions with the new offline camera-model contract.

### Phase 3: Realtime GPU And Viewer Camera Models
**Goal**: Realtime rendering and the viewer screen preserve each camera's selected model from shared scene data through rig packing, OptiX launches, and active viewer cameras.
**Depends on**: Phase 1
**Requirements**: CAM-03, CAM-04
**Success Criteria** (what must be TRUE):
  1. Realtime GPU rendering uses each camera's selected `pinhole32` or `equi62_lut1d` model for ray generation instead of assuming pinhole.
  2. Viewer camera rigs and default viewer scene setup keep the selected model for each active camera.
  3. A realtime four-camera run can preserve mixed per-camera model selections across the packed rig and active viewer screen.
**Plans**: 3 plans
Plans:
- [x] 03-01-PLAN.md - Replace the viewer's hardcoded pinhole rig with a spec-driven builder aligned to authored camera specs.
- [x] 03-02-PLAN.md - Preserve packed camera model data through OptiX upload and recover equi realtime rendering regressions.
- [x] 03-03-PLAN.md - Add file-backed and mixed-model end-to-end regressions for realtime factory, viewer reload, and four-camera rigs.
**UI hint**: yes

### Phase 4: Default Intrinsics And Fisheye Defaults
**Goal**: New and default camera setups derive deterministic v1 intrinsics from image resolution plus horizontal FOV, with fisheye becoming the project default while pinhole stays explicitly available.
**Depends on**: Phase 2, Phase 3
**Requirements**: DEF-02, DEF-03, DEF-04, DEF-05, DEF-06
**Success Criteria** (what must be TRUE):
  1. Newly constructed or default camera setups resolve to `equi62_lut1d` unless a scene, rig, or caller explicitly requests `pinhole32`.
  2. A project utility script can compute default `fx`, `fy`, `cx`, and `cy` from image resolution and horizontal FOV for the supported camera models.
  3. Default pinhole intrinsics are derived from a `90` degree horizontal FOV, and default fisheye intrinsics are derived from a `120` degree horizontal FOV.
  4. Project-wide pinhole defaults remain usable anywhere a camera is explicitly configured to stay pinhole.
**Plans**: 3 plans
Plans:
- [x] 04-01-PLAN.md - Add the shared default-intrinsics derivation helper and thin utility wrapper.
- [x] 04-02-PLAN.md - Move builtin default CPU/realtime camera helpers onto the fisheye-first shared derivation path.
- [x] 04-03-PLAN.md - Switch the viewer no-arg default rig to fisheye and lock remaining default-entry regressions.

### Phase 5: Camera Model Regression Coverage
**Goal**: Automated coverage proves both camera models match the reference math and remain consistent across offline and realtime rendering after fisheye becomes the default.
**Depends on**: Phase 4
**Requirements**: VER-01, VER-02, VER-03, VER-04
**Success Criteria** (what must be TRUE):
  1. Automated tests verify `pinhole32` math against `docs/reference/src-cam/cam_pinhole32.h`.
  2. Automated tests verify `equi62_lut1d` math against `docs/reference/src-cam/cam_equi62_lut1d.h`.
  3. Automated tests show offline CPU and realtime GPU paths honor the same selected camera model and derived default intrinsics for equivalent scene inputs.
  4. Existing relevant render, scene, and viewer regression coverage continues to pass after fisheye becomes the default.
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Shared Camera Schema | 3/3 | Complete | 2026-04-20 |
| 2. Offline CPU Camera Models | 3/3 | Complete | 2026-04-20 |
| 3. Realtime GPU And Viewer Camera Models | 3/3 | Complete | 2026-04-21 |
| 4. Default Intrinsics And Fisheye Defaults | 3/3 | Complete | 2026-04-21 |
| 5. Camera Model Regression Coverage | 0/TBD | Not started | - |
