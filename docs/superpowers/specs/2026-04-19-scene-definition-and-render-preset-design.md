# Scene Definition And Render Preset Design

## Summary

Refactor scene registration so that scene content is defined exactly once per canonical scene, while CPU and realtime rendering consume separate presets.

This removes duplicated scene ids such as `cornell_box_extreme` and `rttnw_final_scene_extreme` from the public catalog. CPU rendering keeps equivalent behavior through internal presets that carry camera configuration and default sample counts. Realtime keeps only canonical scene ids and uses dedicated spawn and movement presets that are suitable for interactive navigation.

## Goals

- Store scene geometry, materials, textures, and media once per canonical scene.
- Remove duplicated public scene ids whose only difference is CPU render configuration.
- Preserve CPU offline rendering behavior for existing canonical scenes.
- Preserve the ability to have multiple internal CPU presets per scene, such as `default` and `extreme`.
- Keep realtime scene selection limited to canonical scenes only.
- Give realtime each scene its own safe initial body pose and base movement speed.
- Add viewer support for scroll-wheel movement speed adjustment, with upward scroll increasing speed and downward scroll decreasing it.

## Non-Goals

- Automatic runtime collision-free spawn search.
- Dynamic collision detection for viewer movement.
- Image-perfect CPU output matching at the pixel level.
- A public API for exposing CPU preset ids as scene ids.

## Current Problems

The current scene system mixes three separate concerns:

- scene content definition
- CPU offline render configuration
- realtime/viewer startup configuration

This creates duplication and brittle coupling:

- several scene ids map to the same `SceneIR` content and only differ in CPU sample count defaults
- CPU offline camera configuration is maintained in a large `if/else` chain keyed by scene id
- realtime startup pose is inferred separately and cannot express scene-specific movement speed
- CPU framing choices are sometimes intentionally far from scene content and are not suitable for realtime navigation

## Proposed Model

### Canonical Scene Definition

Introduce a canonical scene definition object that owns only scene content and descriptive metadata:

- `id`
- `label`
- `builder`

The builder remains responsible for producing `SceneIR`.

There is exactly one canonical scene definition for each distinct scene:

- `cornell_box`
- `cornell_box_and_sphere`
- `cornell_smoke`
- `rttnw_final_scene`
- `final_room`
- other existing unique scenes

Duplicate public ids such as `*_extreme` are removed from the canonical registry.

### CPU Render Preset

Introduce internal CPU render presets attached to a canonical scene. A CPU preset describes how offline rendering should frame and sample that scene:

- `preset_id`
- `samples_per_pixel`
- full offline camera parameters

The camera parameters must include the fields currently needed by `Camera` setup:

- image width
- aspect ratio or width/height pair
- vfov
- lookfrom
- lookat
- vup
- background
- defocus angle
- focus distance
- max depth

Each canonical scene may have more than one internal CPU preset:

- `default`
- `extreme`
- optional future presets such as `wide_shot`

These presets are internal selection data. They are not public scene ids and do not appear in the scene catalog.

### Realtime View Preset

Introduce a dedicated realtime view preset per canonical scene:

- `initial_body_pose`
- `base_move_speed`

`initial_body_pose` is hand-authored, near the scene center, and chosen to avoid obvious intersection with geometry.

`base_move_speed` is hand-authored per scene so large scenes can move faster than small scenes.

Realtime does not reuse CPU camera presets for startup.

## Registry Structure

The shared scene layer should expose canonical scene definitions and preset lookup by canonical scene id.

Recommended interfaces:

- `scene_definition(scene_id)`
- `scene_cpu_default_preset(scene_id)`
- `scene_cpu_preset(scene_id, preset_id)`
- `scene_realtime_view_preset(scene_id)`
- `scene_metadata()`

`scene_metadata()` becomes canonical-only. It no longer returns `*_extreme` entries.

The registry remains the single source of truth for:

- which scenes exist publicly
- which scenes support CPU render
- which scenes support realtime

Support flags belong to canonical scene metadata, not to duplicated alias ids.

## CPU Rendering Flow

`render_shared_scene(scene_id, samples_per_pixel)` should change from ad hoc scene-id branching to preset-driven setup:

1. Resolve the canonical scene definition for `scene_id`.
2. Resolve that scene's default CPU preset.
3. Build `SceneIR` from the canonical scene definition.
4. Configure `Camera` from the preset.
5. If the caller passed `samples_per_pixel > 0`, override the preset default sample count.
6. Render as before.

`render_shared_scene_from_camera(scene_id, packed_camera, samples_per_pixel)` continues to support explicit packed-camera rendering and should still bypass the preset camera pose when a packed camera is provided. It still uses canonical scene content and the same sample-count override rule.

The old scene-id-specific `if/else` camera configuration block in `offline_shared_scene_renderer.cpp` should be replaced by preset lookup.

## Realtime Flow

Realtime scene selection should accept only canonical scene ids.

`scene_catalog()` should list only canonical scenes. As a result:

- duplicated scenes disappear from UI and viewer scene selection
- realtime no longer exposes `*_extreme` variants

`default_spawn_pose_for_scene(scene_id)` should source its result from `scene_realtime_view_preset(scene_id)`.

Add a paired accessor for movement speed, for example:

- `default_move_speed_for_scene(scene_id)`

The viewer should initialize its runtime movement speed from the scene's realtime preset when the scene loads or switches.

## Viewer Speed Control

The viewer currently uses a fixed movement speed constant. Replace this with mutable per-session state:

- initialize from the scene realtime preset
- scroll wheel up multiplies the current base speed by a fixed factor greater than 1
- scroll wheel down multiplies the current base speed by a fixed factor less than 1
- clamp to a reasonable min and max

Recommended initial behavior:

- multiplicative step, not additive
- reset to the scene default speed on scene switch
- display the current speed in the viewer HUD

This matches the requirement that large scenes should be traversable quickly while retaining fine control in small scenes.

## Migration Of Existing Scene Ids

Public duplicated ids are removed entirely from the catalog and realtime support surface.

Examples:

- remove public `cornell_box_extreme`
- remove public `cornell_box_and_sphere_extreme`
- remove public `cornell_smoke_extreme`
- remove public `rttnw_final_scene_extreme`

Their former semantics are preserved as internal CPU presets attached to the canonical scenes:

- `cornell_box` has internal presets `default` and `extreme`
- `cornell_box_and_sphere` has internal presets `default` and `extreme`
- `cornell_smoke` has internal presets `default` and `extreme`
- `rttnw_final_scene` has internal presets `default` and `extreme`

No public alias compatibility layer is kept. Callers must use canonical scene ids.

## Error Handling

Required behavior:

- canonical scene lookup failure remains an error
- CPU preset lookup failure is an internal consistency error and should fail loudly
- realtime view preset lookup failure is an internal consistency error and should fail loudly
- packed-camera offline rendering validation remains unchanged

The system should avoid silent fallback to unrelated scene defaults. If a canonical scene is missing a required preset, that is a bug, not a user input variant.

## Testing

### Registry Tests

Add or extend tests to verify:

- canonical scene ids remain available
- duplicate public ids no longer appear in scene metadata/catalog
- realtime support is reported only for canonical ids

### CPU Preset Regression Tests

Add tests that assert representative canonical scenes still resolve to the expected offline defaults:

- `cornell_box` default CPU preset camera parameters match previous behavior
- `rttnw_final_scene` default CPU preset camera parameters match previous behavior
- internal `extreme` presets keep the previous higher default sample counts

These should be structural tests against preset data, not image diffs.

### CPU Render Smoke Tests

Verify:

- canonical scenes still render through `render_shared_scene`
- explicit packed-camera rendering still works
- sample-count override still supersedes preset defaults

### Realtime Preset Tests

Add tests that assert:

- each canonical realtime scene has a realtime view preset
- spawn pose comes from the realtime preset
- duplicate realtime scene ids are no longer exposed
- scene switch resets move speed to the target scene default
- scroll wheel increases and decreases speed in the expected direction

## Implementation Notes

- Keep the scene content builders where they are if possible; the refactor is about registry structure, not builder math.
- Prefer extracting preset tables and helpers over introducing large inheritance hierarchies.
- Keep CPU and realtime preset types separate. Sharing fields between them is acceptable only when it directly reduces duplication without obscuring intent.

## Success Criteria

- Scene content exists once per canonical scene.
- Public duplicate scene ids are removed from the catalog and realtime selection.
- CPU default and internal extreme sampling semantics are preserved through presets rather than duplicated scene ids.
- CPU offline framing behavior for canonical scenes remains unchanged in structure and smoke tests.
- Realtime startup uses dedicated near-scene spawn presets rather than CPU camera presets.
- Viewer movement speed is scene-specific at startup and scroll-wheel adjustable during navigation.
