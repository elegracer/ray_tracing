# External Scene Description Design

Date: 2026-04-19

## Goal

Move scene content out of hard-coded C++ builders into editable scene files so that:

- scene content can be changed without recompiling;
- CPU presets and realtime presets are defined alongside the scene data;
- the viewer can reload changed scenes and discover newly added scenes;
- the format remains simple enough for hand editing;
- the design leaves room for later renderer work such as explicit light registries, better light sampling, and MIS, without forcing another scene-format rewrite.

This design intentionally targets the current repository's rendering model rather than trying to become a general DCC interchange format.

## Non-Goals

- First-version support for full glTF or USD import.
- First-version support for modern PBR material interchange.
- A general-purpose asset pipeline.
- Auto-generated authoring tools or editors.
- Full backward-compatible support for every existing hard-coded helper API forever; migration shims are allowed, but the long-term source of truth becomes file-based scenes.

## Constraints

- Default workflow must stay simple: one scene directory, one main YAML file, local assets next to it.
- Advanced composition is allowed, but must remain optional.
- Scene reload failures must not blank the currently displayed scene.
- CPU and realtime adapters must continue to share the same scene semantics.
- The format must cover current repository features:
  - background;
  - constant/checker/image/noise textures;
  - diffuse/metal/dielectric/emissive/isotropic-volume materials;
  - sphere/quad/box geometry;
  - transformed instances;
  - media;
  - CPU presets;
  - realtime default view preset.

## Recommended Approach

Use a repository-native YAML scene format as the primary source of truth, with optional support for importing external `OBJ + MTL` assets from within the YAML scene.

This is preferred over:

1. A thin config wrapper around the existing hard-coded C++ builders.
   That would externalize preset tweaks but would not solve editable scene content.

2. Adopting glTF/USD as the primary scene format.
   Those formats are more suitable for external asset interchange than for this repository's current combination of procedural scene semantics, media, and renderer presets. They would add substantial complexity before solving the actual editing workflow.

3. A custom DSL.
   That would provide perfect control but with the highest parser and maintenance cost.

YAML is accepted here as the practical compromise even though it is not ideal aesthetically: it is expressive enough for nested scene data, easy to hand edit, and easy to split into reusable fragments.

## Architecture

The new system is split into three layers.

### 1. Scene Source Files

Files under `assets/scenes/` become the editable source of truth.

Each scene is defined by:

- one main `scene.yaml` entrypoint;
- optional local assets such as textures, `OBJ`, and `MTL`;
- optional included YAML fragments.

### 2. Scene Loader and Catalog

A new loader parses YAML scene files into an in-memory `SceneDefinition` model.

The loader is responsible for:

- scanning scene directories;
- parsing YAML;
- resolving includes and references;
- resolving relative asset paths;
- validating schemas and cross-references;
- building a runtime scene catalog;
- tracking file dependencies for reload.

This layer becomes the shared source for both CPU and realtime scene lookup.

### 3. Existing Adapters

Existing CPU and realtime adapter logic remains conceptually intact.

- CPU adapter consumes `SceneDefinition` and produces CPU world/lights plus CPU presets.
- Realtime adapter consumes `SceneDefinition` and produces realtime scene descriptions plus realtime default-view presets.

This keeps the implementation focused on changing scene data input rather than rewriting renderers.

## Directory Layout

Recommended repository layout:

```text
assets/
  scenes/
    common/
      materials/
        common_materials.yaml
      presets/
        shared_presets.yaml
      fragments/
        room_pieces.yaml
    final_room/
      scene.yaml
      textures/
      models/
    cornell_box/
      scene.yaml
    imported_chair_room/
      scene.yaml
      models/
        chair.obj
        chair.mtl
      textures/
        chair_albedo.png
```

Rules:

- every discoverable scene lives in its own directory;
- the discoverable entrypoint is `scene.yaml`;
- local assets should be referenced relatively from the scene directory;
- reusable fragments should live under `assets/scenes/common/`.

## YAML Format

### Top-Level Structure

Each main scene file uses this top-level shape:

```yaml
format_version: 1

scene: ...
cpu_presets: ...
realtime: ...
imports: ...
includes: ...
```

Required:

- `format_version`
- `scene`

Optional:

- `cpu_presets`
- `realtime`
- `imports`
- `includes`

### `scene`

The `scene` block contains shared scene semantics:

- `id`
- `label`
- `background`
- `textures`
- `materials`
- `shapes`
- `instances`
- `media`
- `lights` (optional but reserved for future renderer work)

### `cpu_presets`

Named preset map for offline CPU rendering.

Each preset contains:

- `samples_per_pixel`
- `camera`
  - `lookfrom`
  - `lookat`
  - `vfov`
  - `aspect_ratio`
  - `image_width`
  - `max_depth`
  - `vup`
  - `defocus_angle`
  - `focus_dist`

The default preset is the one named `default`.

Additional named presets such as `extreme` remain allowed.

### `realtime`

Realtime viewer configuration lives here.

First version supports:

- `default_view`
  - `initial_body_pose`
    - `position`
    - `yaw_deg`
    - `pitch_deg`
  - `frame_convention`
  - `vfov_deg`
  - `use_default_viewer_intrinsics`
  - `base_move_speed`

### `imports`

External asset declarations.

First version supports:

- `type: obj_mtl`
- `obj: relative/path.obj`
- optional `mtl: relative/path.mtl`
- optional transform or material override at the use site

### `includes`

Optional list of other YAML fragments to merge into the current scene-definition context.

Supported use cases:

- shared materials;
- shared shape fragments;
- shared preset fragments.

## Example Scene

```yaml
format_version: 1

includes:
  - ../common/materials/common_materials.yaml

imports:
  chair_mesh:
    type: obj_mtl
    obj: models/chair.obj

scene:
  id: final_room
  label: Final Room
  background: [0.0, 0.0, 0.0]

  materials:
    white:
      type: diffuse
      albedo: [0.73, 0.73, 0.73]
    ceiling_light:
      type: emissive
      emission: [10.0, 10.0, 10.0]
      sample_as_light: true

  shapes:
    ceiling_quad:
      type: quad
      origin: [-1.0, 3.15, -1.0]
      edge_u: [2.0, 0.0, 0.0]
      edge_v: [0.0, 0.0, 2.0]

  instances:
    - shape: ceiling_quad
      material: ceiling_light
    - import: chair_mesh
      transform:
        translation: [1.0, 0.0, -0.5]
        yaw_deg: 25.0

cpu_presets:
  default:
    samples_per_pixel: 500
    camera:
      lookfrom: [13.0, 2.0, 3.0]
      lookat: [0.0, 0.0, 0.0]
      vfov: 20.0
      aspect_ratio: 1.7777778
      image_width: 400
      max_depth: 50
      vup: [0.0, 1.0, 0.0]
      defocus_angle: 0.6
      focus_dist: 10.0

realtime:
  default_view:
    initial_body_pose:
      position: [0.0, 0.0, 1.6]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: world_z_up
    vfov_deg: 67.38
    use_default_viewer_intrinsics: true
    base_move_speed: 2.0
```

## Object Model and References

The format uses explicit ids and explicit references. The first version should avoid hidden inheritance or implicit merging.

Examples:

- `material: white`
- `shape: ceiling_quad`
- `import: chair_mesh`

Rules:

- duplicate ids in the same resolved namespace are errors;
- missing references are errors;
- circular includes are errors;
- include depth should be limited and validated explicitly.

This keeps diagnostics understandable and reduces the risk of future schema drift.

## External Asset Support

### First-Version Goal

Support simple external mesh import at the lowest practical complexity.

That means:

- `OBJ` geometry;
- `MTL` sidecar support;
- basic texture references from `MTL`;
- use from within YAML, not as a standalone scene source.

### Why `OBJ + MTL`

`OBJ + MTL` is explicitly not a modern PBR format, but it is the lowest-complexity route to importing simple common assets.

The importer should support:

- positions;
- normals;
- UVs;
- triangle faces;
- object/group segmentation when practical;
- `Kd`, `Ks`, `Ns`, `d`/`Tr`, `map_Kd` from `MTL`.

The importer may approximate `MTL` materials onto repository-native materials:

- default to diffuse mapping;
- map obvious reflective cases to metal when feasible;
- use simple transparency approximation where supported;
- do not promise full PBR fidelity.

This keeps the first version tractable while still allowing simple external scene content.

## Forward Compatibility for Better Path Tracing

The current realtime renderer already supports:

- emissive hits;
- multiple bounces;
- reflection/refraction/volume scattering;
- a heuristic direct-light term.

But it does not yet have a full light-importance sampling and MIS pipeline comparable to the CPU path tracer.

To avoid a future scene-format rewrite, the YAML schema should distinguish:

- scene semantics;
- render presets;
- sampling-oriented metadata.

### Required Future-Proofing

The schema should already reserve:

- `scene.lights`
- `material.sample_as_light`
- `instance.enabled`
- `instance.visible_in`
- `instance.tags`

First version behavior:

- if `scene.lights` is omitted, derive the light set from emissive instances marked as sampleable;
- if `scene.lights` is present, use the explicit light registry.

Reserved-but-optional future fields:

- `light.kind`
- `light.sampling_weight`
- `light.group`
- `light.enabled_in_cpu`
- `light.enabled_in_realtime`

These fields do not need full first-version renderer support, but the schema shape should allow them so future MIS/light-registry work remains additive instead of structural.

## Runtime Behavior

### Startup Scan

At startup, the loader scans `assets/scenes/**/scene.yaml` and builds the runtime scene catalog.

Each catalog entry contains:

- scene id;
- label;
- availability flags;
- resolved presets;
- file dependency list for reload.

### Automatic Reload

For the currently loaded scene, the viewer tracks modification times of:

- main scene file;
- included YAML fragments;
- referenced `OBJ` / `MTL`;
- referenced textures if used by imported assets or scene textures.

When a dependency changes:

- mark the current scene dirty;
- reload at the next safe point between frames;
- if reload succeeds, swap in the new resolved scene;
- if reload fails, keep the previous resolved scene active and surface the error in the UI.

### Manual Actions

The viewer must expose two separate manual actions:

1. `Reload Current Scene`
   Re-parse the current scene and its dependencies only.

2. `Rescan Scene Directory`
   Rebuild the discoverable scene catalog so newly added or renamed scene directories appear.

These actions should remain separate because they serve different workflows.

## Error Handling

The loader must provide clear diagnostics for:

- schema validation failures;
- missing files;
- bad references;
- duplicate ids;
- circular includes;
- unsupported import features;
- invalid preset definitions.

Behavior rules:

- parse failure must not replace the currently active scene with an empty or partial one;
- failed rescan must not corrupt the existing catalog;
- current-scene reload and catalog rescan should use atomic swap semantics on success.

## GUI Scene Switcher Size

The current viewer scene selector is too small for the existing scene list.

For the current repository scene count, a practical no-scroll target is:

- combo width: approximately `400 px`
- expanded list height: approximately `360 px`
- visible row budget: about `14` rows

This should be enough to show all current scene labels without requiring a mouse wheel under normal scaling.

If the scene count later grows beyond that, the UI may still scroll, but the target for the redesign is that current scenes fit without scrolling.

## Migration Strategy

Migration should be incremental.

1. Introduce the file format, loader, and catalog.
2. Add file-backed scene discovery alongside existing hard-coded scenes.
3. Migrate existing scenes one by one into YAML.
4. Once parity is established, remove hard-coded scene builders or reduce them to compatibility shims.

This keeps the risk manageable and allows renderer parity tests to catch migration regressions scene by scene.

## Testing Strategy

The implementation plan should cover at least:

- parser unit tests for valid and invalid YAML;
- reference-resolution tests;
- include-cycle and duplicate-id tests;
- scene-catalog discovery tests;
- current-scene reload and rescan behavior tests;
- `OBJ + MTL` import smoke tests;
- CPU adapter parity tests for migrated scenes;
- realtime adapter parity tests for migrated scenes;
- viewer integration tests for reload/rescan controls and scene-list sizing logic where practical.

In addition, at least one migrated scene should continue to use CPU-vs-realtime comparison tests to ensure that format migration does not silently change renderer behavior.
