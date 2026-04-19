# Phase 1: Shared Camera Schema - Context

**Gathered:** 2026-04-19
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase defines the shared camera schema used by scene definitions, scene loading, and rig data so each camera can be represented as either `pinhole32` or `equi62_lut1d`. It covers the canonical data shape, YAML/builtin scene declaration rules, and migration of repo-owned scene/preset data to that schema. It does not yet implement offline CPU ray generation changes, realtime GPU/viewer ray generation changes, or the default-fisheye intrinsics utility.

</domain>

<decisions>
## Implementation Decisions

### Shared camera schema
- **D-01:** Introduce a single canonical shared camera description for the project, centered on a `CameraSpec`-style schema rather than separate CPU and realtime camera config formats.
- **D-02:** The canonical schema must carry `model`, `width`, `height`, `fx`, `fy`, `cx`, `cy`, `T_bc`, and model-specific parameter slots.
- **D-03:** Offline, realtime, viewer, and scene-loading code should adapt from this shared schema instead of maintaining independent long-lived camera representations.

### Scene and YAML declaration
- **D-04:** Every camera declaration in builtin scenes and YAML-backed scene files must explicitly specify `model`.
- **D-05:** The phase must not rely on implicit camera-model defaults inside scene files, because the project default will later switch to fisheye and silent fallback would make scene behavior ambiguous.

### Parameter boundary
- **D-06:** Phase 1 schema should define stable slots for both models' distortion parameters and for body-relative `T_bc`, even though v1 will use zero distortion defaults.
- **D-07:** The shared schema should be designed once now rather than grown incrementally with another schema break in later phases.

### Migration strategy
- **D-08:** Repo-owned builtin scene data and YAML-backed scene files should be migrated to the new schema in this phase.
- **D-09:** Old camera declaration formats are not preserved as a supported compatibility path for repo-owned content; after migration, the new schema becomes the only project-owned representation.

### the agent's Discretion
- Field naming, helper constructors, and exact type layout for the canonical camera schema.
- Whether compatibility shims exist temporarily inside implementation code while the new schema is wired through.
- Exact division of structs between `scene/` and `realtime/` headers as long as the canonical shared schema remains the single source of truth.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Camera model math
- `docs/reference/src-cam/cam_pinhole32.h` — Reference behavior and parameter shape for the `pinhole32` model.
- `docs/reference/src-cam/cam_equi62_lut1d.h` — Reference behavior and parameter shape for the `equi62_lut1d` model.

### Shared scene and scene-definition direction
- `docs/superpowers/specs/2026-04-18-shared-scene-ir-for-cpu-and-realtime-design.md` — Existing shared-scene design direction that this camera schema must fit into.
- `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md` — Current scene-definition and render-preset expectations that will be affected by the camera schema migration.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/realtime/camera_models.h`: Already defines `CameraModelType`, `Pinhole32Params`, and `Equi62Lut1DParams`, which should anchor the canonical schema instead of introducing parallel math types.
- `src/realtime/camera_rig.h`: `PackedCamera`, `PackedCameraRig`, and `CameraRig::Slot` already carry `model`, `width`, `height`, `T_bc`, and a variant over the two camera parameter types.
- `tests/test_camera_rig.cpp`: Existing mixed-model rig coverage can be extended to guard the canonical schema and packing behavior.

### Established Patterns
- Scene data flows through `src/scene/scene_definition.h`, `src/scene/shared_scene_builders.h`, and `src/scene/yaml_scene_loader.cpp` before reaching offline or realtime adapters.
- Repo-owned scenes already use YAML plus builtin scene-definition helpers, so schema changes should be reflected in both paths in the same phase.
- Current scene and viewer presets still encode camera assumptions mostly through `vfov` and preset-specific fields, so Phase 1 needs to normalize representation before downstream render phases can be clean.

### Integration Points
- `src/scene/scene_definition.h` and `src/scene/shared_scene_builders.h` are the natural homes for the shared schema that both offline and realtime paths can consume.
- `src/scene/yaml_scene_loader.cpp` is the migration entry point for explicit `model` declarations in file-backed scenes.
- `src/realtime/camera_rig.h` and `src/realtime/camera_rig.cpp` should adapt from the canonical scene-level representation instead of being treated as the canonical definition themselves.

</code_context>

<specifics>
## Specific Ideas

- The shared schema should behave like a canonical `CameraSpec` rather than a loose bundle of per-renderer camera presets.
- YAML must make `model` explicit for every camera declaration so later default switches do not silently rewrite scene semantics.

</specifics>

<deferred>
## Deferred Ideas

- Dynamic render-preset-controlled resolution selection belongs to a later calibration-focused phase.
- Dynamic explicit intrinsics, model-specific distortion tuning, and body-relative extrinsic configuration belong to later phases even though the schema should pre-allocate their slots now.

</deferred>

---
*Phase: 01-shared-camera-schema*
*Context gathered: 2026-04-19*
