# Shared Scene IR For CPU And Realtime Design

Date: 2026-04-18

## Goal

Replace the remaining `render_scene.cpp`-local scene definitions with a single shared scene description layer that can be consumed by:

- `utils/render_scene.cpp`
- `utils/render_realtime.cpp`
- `utils/render_realtime_viewer.cpp`

The intent is not just shared option strings. All scenes that currently live in `render_scene.cpp` should become actual shared scene definitions that all three entrypoints can render.

This change should also move the project toward a more generally useful scene representation, especially for:

- reusable texture descriptions
- volume / medium descriptions
- future PBR-oriented material evolution

## Current State

The repository already has:

- a shared scene catalog used by the three entrypoints
- a small shared realtime scene factory
- ImGui-based runtime scene switching in the realtime viewer

However, the actual scene implementations are still split:

- `render_scene.cpp` contains the large offline-only scene definitions
- realtime currently only has dedicated implementations for `smoke` and `final_room`

That means the catalog is shared, but the scene semantics are not.

## Scope

This work includes:

1. introducing a shared scene IR between scene definitions and render backends
2. moving all remaining `render_scene.cpp` scenes into shared builders
3. adapting CPU rendering to build from the shared IR
4. adapting realtime rendering to build from the shared IR
5. making all current `render_scene.cpp` scenes actually usable from all three entrypoints
6. adding reusable support for textures, media, boxes, and transforms in the shared scene representation

This work does not include:

- preserving moving-sphere behavior in this phase
- adding dynamic-scene support in this phase
- implementing a full glTF asset pipeline
- implementing a full principled BSDF / metallic-roughness system yet
- solving performance problems beyond what is required to make the scenes function correctly

## Requirements

### Functional

- every scene currently implemented in `utils/render_scene.cpp` must be defined through the new shared scene layer
- `render_scene.cpp` must render those scenes through a CPU adapter from the shared scene layer
- `render_realtime.cpp` must render those same scenes through a realtime adapter from the shared scene layer
- `render_realtime_viewer.cpp` must be able to switch to those same scenes and render them
- moving-sphere behavior may be removed in this phase, but the resulting scenes should remain visually close static approximations
- if a scene effect can be represented through a more generally useful, industry-aligned mechanism, use that mechanism instead of adding a one-off scene-specific special case

### Scene Representation

- the shared IR must be expressive enough for:
  - constant-color textures
  - checker textures
  - image textures
  - noise textures
  - diffuse materials
  - metal materials
  - dielectric materials
  - emissive materials
  - isotropic volume materials
  - sphere shapes
  - quad shapes
  - box shapes
  - translate transforms
  - rotate transforms
  - uniform media / constant-density volumes

- BVH and acceleration structures must remain backend details, not shared scene IR concepts

### Product / Architecture

- the new shared scene layer should be designed as a minimal scene-semantics IR, not as a realtime-only packed format
- the design should leave room for future material evolution toward more standard PBR / principled workflows
- the design should avoid introducing temporary compatibility hacks that would block that future direction

## Key Decisions

### 1. Add A Shared Scene IR

Introduce a shared scene IR between scene builders and renderer-specific adapters.

This IR is the single source of scene semantics. It should describe what the scene is, not how a particular backend stores or accelerates it.

That means:

- scene builders return shared IR
- CPU rendering consumes IR through a CPU adapter
- realtime rendering consumes IR through a realtime adapter

### 2. Keep CPU As The Reference Renderer

`render_scene.cpp` should continue to act as the higher-fidelity reference path, but it should no longer own its own independent scene definitions.

The CPU path remains useful as the semantic reference implementation because it can typically absorb richer material and volume behavior with less backend pressure.

The important change is:

- same scene semantics
- different backend adapters

### 3. Remove Moving Spheres In This Phase

Moving spheres should not be carried into the shared scene IR for this round.

For scenes that currently rely on motion:

- convert them to static approximations
- keep the visual intent where possible
- explicitly accept the loss of motion-blur / dynamic-scene behavior

This keeps the scope focused on shared semantics rather than animation support.

### 4. Treat Media As A First-Class Shared Concept

`ConstantMedium` should not stay as a CPU-only implementation detail.

Instead, the new shared IR should represent uniform media explicitly, using a general volume concept:

- boundary geometry
- density
- isotropic scattering / albedo texture or color

This is both more semantically correct and more future-friendly than preserving a one-off offline-only wrapper.

### 5. Use General Texture / Material Concepts

Textures such as checker, image, and noise should become shared texture descriptions instead of staying buried in CPU-only material construction.

The near-term material set stays minimal:

- diffuse
- metal
- dielectric
- emissive
- isotropic volume

But the layout and naming should avoid painting the project into a corner when a future principled or PBR material model is added.

## Scene IR Design

### Textures

The shared IR should include a texture layer with these variants:

- `constant_color`
- `checker`
- `image`
- `noise`

Textures are reusable scene resources. Materials should reference textures rather than re-encoding texture logic in each backend-specific material path.

### Materials

The shared IR should include these material variants:

- `diffuse`
- `metal`
- `dielectric`
- `emissive`
- `isotropic_volume`

This is intentionally smaller than a full PBR system but already much closer to a generally useful material layer than the current ad hoc split.

### Shapes

The shared IR should include these shape primitives:

- `sphere`
- `quad`
- `box`

Boxes should be represented as first-class shared geometry semantics, not as only CPU-side helper expansion logic.

Backend adapters are free to lower boxes however they need:

- CPU may keep a convenient structural representation
- realtime may expand a box into six quads if that is the most practical backend mapping

### Transforms

The shared IR should include transform support for:

- translate
- rotate

These transforms should be part of the shared scene semantics, because several existing scenes already depend on them for box placement and orientation.

### Media

The shared IR should support uniform media attached to boundary geometry.

This should be expressive enough to model the existing smoke scenes and the volumetric components of the final scene without inventing a backend-specific special case.

### Scene Object Layer

The shared scene should be built from instances that combine:

- geometry
- material
- optional transform stack
- optional medium binding where applicable

The exact class/struct split can follow existing project style, but the boundary must stay clear:

- textures and materials are reusable scene resources
- instances are scene placements / bindings

## Mapping Existing Scenes

### High-Fidelity Direct Migrations

These scenes should migrate directly with no intended semantic downgrade:

- `quads`
- `cornell_box`
- `cornell_box_and_sphere`
- `simple_light`
- `smoke`
- `final_room`

They mainly require shared support for:

- sphere / quad / box
- emissive and basic BSDFs
- transforms
- uniform media

### Texture-Driven Migrations

These scenes should migrate through shared texture support:

- `checkered_spheres`
- `earth_sphere`
- `perlin_spheres`

No special-casing is justified here. Shared texture descriptions are the correct abstraction.

### Static Approximation Migrations

These scenes should migrate with explicit removal of motion:

- `bouncing_spheres`
- `rttnw_final_scene`

For `bouncing_spheres`:

- preserve the random-sphere field
- remove moving-sphere behavior
- keep the scene as a static approximation

For `rttnw_final_scene`:

- preserve the large-box ground
- preserve static spheres, media, image texture, and noise texture
- preserve the translated / rotated clustered geometry
- remove moving-sphere behavior only

This is an acceptable semantic downgrade for this phase.

## Backend Adapters

### CPU Adapter

The CPU adapter should transform shared IR into the current reference renderer structures:

- textures
- materials
- hittables
- transforms
- volume wrappers

The CPU adapter is where fidelity should remain highest.

### Realtime Adapter

The realtime adapter should transform shared IR into the current realtime scene representation, expanding or lowering IR concepts as needed.

Examples:

- boxes may lower to quads
- transforms may be baked where appropriate
- medium support may require specific backend-side packing choices

The important requirement is that realtime now consumes the same scene semantics, not a separate hand-maintained scene set.

## Options Considered

### Option A: Keep Extending The Current Realtime `SceneDescription`

Pros:

- smaller immediate code changes
- least disruption to current realtime code

Cons:

- mixes backend storage concerns with shared scene semantics
- makes future material / texture / medium evolution harder
- would continue the pattern of solving each new scene requirement by patching a renderer-specific type

### Option B: Add A Shared Scene IR With Backend Adapters

Pros:

- keeps scene semantics separate from backend data layout
- supports all three entrypoints from one scene source
- creates the right foundation for future textures, media, and more standard material evolution

Cons:

- larger refactor now
- requires adapter code on both CPU and realtime sides

### Option C: Jump Directly To A Full Asset / PBR Pipeline

Pros:

- highest long-term ceiling

Cons:

- far beyond this task
- turns scene unification into a full rendering pipeline rewrite

## Chosen Direction

Use Option B.

Introduce a minimal but future-aware shared scene IR and move all current `render_scene.cpp` scenes onto it.

## File / Responsibility Direction

Expected module boundaries:

- `scene_catalog`
  - scene ids, labels, user-facing registration
- `scene_ir`
  - shared textures, materials, media, shapes, transforms, instances, scene container
- `scene_builders`
  - scene-specific construction returning shared IR
- `cpu_scene_adapter`
  - shared IR to CPU reference renderer types
- `realtime_scene_adapter`
  - shared IR to realtime scene representation
- viewer scene switching
  - remains responsible only for runtime switching state and UI integration

The exact filenames can follow repo style, but the responsibility split should stay stable.

## Error Handling

The long-term goal of this change is that all current scenes work in all three entrypoints.

Still, adapters should fail clearly if they encounter unsupported IR features:

- CLI tools should fail fast with explicit error messages
- viewer runtime switching should keep the previous scene and display the adapter failure

These failures are defensive checks, not intended steady-state behavior for the migrated scenes.

## Testing

The work should add or update tests at four levels:

### 1. IR Construction Tests

Validate:

- texture registration
- material registration
- medium registration
- shape / transform composition

### 2. Scene Builder Coverage Tests

Validate that every scene id previously implemented in `render_scene.cpp` now produces shared IR successfully.

### 3. Adapter Tests

CPU adapter coverage should include representative support for:

- checker textures
- image textures
- noise textures
- uniform media
- transformed boxes

Realtime adapter coverage should include the same semantic set where it maps to the current backend.

### 4. Entry Point Regression Tests

At minimum, use representative smoke/regression coverage for:

- `quads`
- `earth_sphere`
- `cornell_smoke`
- `bouncing_spheres`
- `rttnw_final_scene`

These cover:

- simple geometry
- image textures
- media
- randomized static geometry
- large multi-feature scenes

## Success Criteria

This design is successful when:

- no scene semantics remain exclusively defined inside `render_scene.cpp`
- all current `render_scene.cpp` scenes can be rendered from `render_scene.cpp`, `render_realtime.cpp`, and `render_realtime_viewer.cpp`
- moving-sphere behavior is explicitly removed rather than silently half-preserved
- textures, media, boxes, and transforms become part of a reusable shared scene layer
- the resulting architecture moves the project toward more standard material and scene-system evolution instead of deeper renderer-specific special cases
