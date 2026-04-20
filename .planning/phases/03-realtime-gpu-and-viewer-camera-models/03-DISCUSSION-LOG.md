# Phase 3: Realtime GPU And Viewer Camera Models - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-20
**Phase:** 03-realtime-gpu-and-viewer-camera-models
**Areas discussed:** Viewer default rig source of truth, mixed-model four-camera behavior, realtime fisheye output contract, realtime/viewer consistency target

---

## Viewer default rig source of truth

| Option | Description | Selected |
|--------|-------------|----------|
| A | Use the scene-authored realtime camera preset as the single source of truth for viewer model and intrinsics, while preserving viewer pose/yaw-offset behavior | ✓ |
| B | Keep a separate viewer-only hardcoded pinhole rig | |
| C | Use scene presets only for single-camera viewer defaults | |
| D | Another definition | |

**User's choice:** `1A`
**Notes:** The user wants realtime scene presets and viewer defaults to share one camera truth rather than preserving a separate viewer-only pinhole path.

---

## Mixed-model four-camera behavior

| Option | Description | Selected |
|--------|-------------|----------|
| A | Allow each camera in the rig to independently choose `pinhole32` or `equi62_lut1d` | ✓ |
| B | Require all four cameras to share one model in this phase | |
| C | Only let the front camera choose independently | |
| D | Another constraint | |

**User's choice:** `2A`
**Notes:** The user wants the per-camera first-class type contract preserved even in the four-camera viewer/realtime rig.

---

## Realtime fisheye output contract

| Option | Description | Selected |
|--------|-------------|----------|
| A | Keep full rectangular output and follow the existing shared-camera fallback semantics for out-of-range fisheye pixels | ✓ |
| B | Keep rectangular output but force out-of-range pixels directly to background | |
| C | Introduce explicit valid-region or mask semantics in this phase | |
| D | Another contract | |

**User's choice:** `3A`
**Notes:** The user wants realtime fisheye to match the Phase 2 offline contract instead of introducing a new GPU/viewer-only invalid-region rule.

---

## Realtime/viewer consistency target

| Option | Description | Selected |
|--------|-------------|----------|
| A | Treat contract preservation across preset, runtime camera spec, packed rig, OptiX active camera, and viewer active camera as the primary acceptance target, plus end-to-end mixed-model regressions | ✓ |
| B | Require full offline/realtime image parity already in this phase | |
| C | Only require the GPU path to run both camera models | |
| D | Another acceptance target | |

**User's choice:** `4A`
**Notes:** The user wants this phase centered on preserving selected model, intrinsics, and pose through the realtime/viewer chain, while leaving stronger cross-path image parity for later regression work.

## Additional constraints

- Viewer and realtime defaults must stay aligned with the authored scene preset camera contract.
- Mixed-model four-camera rigs are in scope for this phase, not deferred.

## the agent's Discretion

- Exact helper boundaries between realtime scene factory and viewer default rig generation.
- Exact regression split between realtime factory, viewer rig, and OptiX tests.

## Deferred Ideas

- Project-wide fisheye default switching.
- Dynamic calibration controls and explicit validity masks.
