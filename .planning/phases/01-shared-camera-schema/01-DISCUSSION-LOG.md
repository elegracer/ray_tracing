# Phase 1: Shared Camera Schema - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-19
**Phase:** 01-shared-camera-schema
**Areas discussed:** Schema shape, scene/YAML declaration, parameter boundary, migration strategy

---

## Schema shape

| Option | Description | Selected |
|--------|-------------|----------|
| A | Define a single canonical shared camera description carrying model, size, intrinsics, extrinsics, and model-specific parameters | ✓ |
| B | Keep CPU and realtime camera configs separate and convert between them | |
| C | Use another custom structure | |

**User's choice:** `1A`
**Notes:** The user wants a unified `CameraSpec`-style schema so the full pipeline can treat pinhole and fisheye as first-class types without parallel representations.

---

## Scene and YAML declaration

| Option | Description | Selected |
|--------|-------------|----------|
| A | Every camera declaration must explicitly specify `model` | ✓ |
| B | Allow `model` to be omitted and use the project default | |
| C | Only builtin scenes must be explicit | |
| D | Another rule | |

**User's choice:** `2A`
**Notes:** Explicit declaration is required so the later default switch to fisheye does not silently alter existing scene meaning.

---

## Parameter boundary

| Option | Description | Selected |
|--------|-------------|----------|
| A | Define the stable schema now with `T_bc` and model-specific distortion slots, even if v1 uses zero distortion | ✓ |
| B | Keep only the minimum immediate fields and extend later | |
| C | Pre-allocate distortion only, not `T_bc` | |
| D | Another boundary | |

**User's choice:** `3A`
**Notes:** The user explicitly wants later support for richer calibration, so the schema should be stabilized in Phase 1 rather than changed again later.

---

## Migration strategy

| Option | Description | Selected |
|--------|-------------|----------|
| A | Migrate repo-owned builtin and YAML scene data to the new schema immediately, with no continuing old-format path for project-owned content | ✓ |
| B | Keep compatibility and defer repo-owned migration | |
| C | Keep compatibility for reading while migrating repo-owned content to the new schema | |
| D | Another strategy | |

**User's choice:** `4A`
**Notes:** The user prefers a clean cut for project-owned scene/preset data rather than carrying dual schema formats through later phases.

## the agent's Discretion

- Exact type names and helper APIs for the canonical shared schema.
- Temporary internal adapters used while migrating downstream code.

## Deferred Ideas

- Dynamic render-preset-controlled resolution support.
- Dynamic explicit intrinsics, distortion coefficients, and body-relative extrinsics as runtime-configurable values.
