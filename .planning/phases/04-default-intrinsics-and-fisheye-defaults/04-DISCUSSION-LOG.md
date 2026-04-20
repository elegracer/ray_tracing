# Phase 4: Default Intrinsics And Fisheye Defaults - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-21
**Phase:** 04-default-intrinsics-and-fisheye-defaults
**Areas discussed:** Default-switch boundary, intrinsics utility shape, legacy helper migration strategy, existing explicit pinhole preset treatment

---

## Default-switch boundary

| Option | Description | Selected |
|--------|-------------|----------|
| A | Switch all implicit/default construction entrypoints to `equi62_lut1d`, while preserving explicit authored pinhole scenes/presets | ✓ |
| B | Also migrate repo-owned explicit authored default presets to fisheye | |
| C | Only switch the no-argument viewer/default helper paths | |
| D | Another boundary | |

**User's choice:** `1A`
**Notes:** The user wants fisheye to become the project default through implicit construction paths, but does not want explicit authored pinhole presets silently rewritten.

---

## Intrinsics utility shape

| Option | Description | Selected |
|--------|-------------|----------|
| A | Implement one shared derivation path as a reusable C++ helper plus a thin script wrapper | ✓ |
| B | Script only | |
| C | C++ helper only | |
| D | Another utility shape | |

**User's choice:** `2A`
**Notes:** The user wants a script for practical computation, but also wants code and script to share one derivation source of truth.

---

## Legacy helper migration strategy

| Option | Description | Selected |
|--------|-------------|----------|
| A | Move default helper paths toward a shared horizontal-FOV-driven derivation path, keeping only minimal compatibility wrappers where needed | ✓ |
| B | Keep existing `vfov` helpers and add a separate fisheye default helper | |
| C | Remove all `vfov` helpers immediately | |
| D | Another strategy | |

**User's choice:** `3A`
**Notes:** The user wants default helper logic to converge rather than preserving a permanent dual-default system.

---

## Existing explicit pinhole preset treatment

| Option | Description | Selected |
|--------|-------------|----------|
| A | Preserve repo-owned explicit `pinhole32` presets and only switch implicit defaults | ✓ |
| B | Migrate main builtin/default scenes to authored fisheye presets as part of this phase | |
| C | Only migrate the viewer default scene | |
| D | Another treatment rule | |

**User's choice:** `4A`
**Notes:** The user wants Phase 4 focused on the default-construction mechanism rather than recomposing all current authored scene framing.

## Additional constraints

- Default pinhole horizontal FOV is `90` degrees.
- Default fisheye horizontal FOV is `120` degrees.
- v1 default intrinsics keep `cx = width / 2` and `cy = height / 2`.
- v1 continues to use zero distortion coefficients for both supported camera models.

## the agent's Discretion

- Exact helper API and script-wrapper shape.
- Exact compatibility-wrapper strategy while migrating brownfield helper code.

## Deferred Ideas

- Dynamic resolution and broader calibration controls.
- Full migration of existing authored pinhole scene framing to fisheye.
