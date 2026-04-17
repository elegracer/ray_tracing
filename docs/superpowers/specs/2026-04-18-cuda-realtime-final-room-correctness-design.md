# CUDA Realtime Final Room Correctness Scene Design

## Goal

Add a new correctness-focused realtime scene that is more spatially complete than the current smoke scene and better suited for validating multi-camera rendering results.

The new scene should:

- be inspired by the visual language of `rttnw_final_scene`
- place content on the floor, walls, and ceiling so different view directions all see meaningful geometry
- use an internal light source so the room is not dark when viewed from inside
- keep the current smoke scene as the default benchmark path

This phase is for correctness and visual inspection first. It does not change the default benchmark scene and does not try to preserve the same runtime cost as the smoke scene.

## Current State

The current realtime CLI in [render_realtime.cpp](/home/huangkai/codes/ray_tracing/.worktrees/cuda-realtime-multi-camera/utils/render_realtime.cpp) always builds a very small smoke scene:

- one emissive quad
- one diffuse sphere
- a simple pinhole rig with camera translations only

That scene is good for pipeline smoke coverage, but it is weak for visual correctness checks because:

- many view directions have little structure
- it does not resemble the content density of a real scene
- it does not stress whether all camera directions see meaningful illuminated geometry

The full `rttnw_final_scene` in [render_scene.cpp](/home/huangkai/codes/ray_tracing/.worktrees/cuda-realtime-multi-camera/utils/render_scene.cpp) contains richer content, but it is not shaped as a closed room for surround-camera inspection and is too heavy to drop directly into the realtime CLI as-is.

## Scope

This sub-project includes:

1. adding a new CLI-selectable realtime scene option, tentatively `final_room`
2. building a new enclosed room scene with content distributed around the interior
3. adding a pinhole four-camera correctness rig better suited for surround inspection
4. preserving the current smoke scene as the default realtime benchmark scene
5. adding a lightweight verification path so the new scene can be rendered and visually checked without disturbing the benchmark workflow

This sub-project does not include:

- making `final_room` the default benchmark scene
- adding fisheye correctness coverage in this phase
- changing existing benchmark CSV/JSON schema
- changing camera math, projection math, or OptiX kernel math
- reproducing the full object count or cost of `rttnw_final_scene`

## Primary Questions to Answer

The implementation should make it easy to answer:

1. do all four pinhole surround views see illuminated, meaningful geometry
2. does the realtime path still behave correctly when the scene contains content on all major room surfaces
3. can scene-selection and rig-selection be added without disturbing the existing benchmark workflow

## Options Considered

### Option A: Reuse `rttnw_final_scene` directly

- port the full scene into realtime
- keep the current rig with only small translations

Pros:

- direct reuse of existing content
- least design work on scene layout

Cons:

- too heavy for a correctness-first realtime check
- not organized around four-direction surround inspection
- does not guarantee all views see dense content

### Option B: Build a new enclosed room using `rttnw_final_scene` content ideas

- use a closed room
- keep the scene materially and compositionally inspired by `rttnw_final_scene`
- distribute objects across floor, walls, ceiling, and room center
- place the light inside the room

Pros:

- directly matches the inspection goal
- easier to control visibility and illumination from all directions
- keeps correctness and benchmark concerns separated

Cons:

- requires scene redesign instead of direct reuse

### Option C: Keep extending the existing smoke scene

- add more spheres and quads to the current smoke layout

Pros:

- smallest implementation delta

Cons:

- likely to become ad hoc
- weak spatial structure for deliberate correctness checks
- does not meaningfully reflect the desired final-scene style

## Chosen Direction

Use Option B.

The scene should be a new enclosed `final_room` correctness scene that borrows the visual themes of `rttnw_final_scene` but is intentionally reorganized for four-view surround inspection.

## Design

### 1. CLI Integration

The realtime CLI should gain a scene selector:

- `--scene smoke|final_room`

Rules:

- default remains `smoke`
- benchmark scripts and default profiling tests continue to exercise `smoke`
- `final_room` is used explicitly for correctness and image inspection

No existing default command should change behavior when `--scene` is omitted.

### 2. Scene Structure

`final_room` should be a closed room, not an open environment.

The room should include:

- floor
- ceiling
- four walls
- at least one internal emissive area light

The light must be inside the room so the interior remains readable from all four camera directions.

The room should avoid large black voids and avoid large empty wall regions dominating any single camera view.

### 3. Content Layout

The scene should preserve the semantic feel of `rttnw_final_scene`, but in reduced and more controllable form.

#### Floor content

The floor should include several large box-like forms or platforms with varying heights to preserve the stepped terrain / stacked-volume feel of the final scene.

#### Wall content

Each wall should carry visible content, such as:

- spheres mounted or placed near the wall
- box-like forms or shelves
- large panels or quads with distinct material response

The point is not decoration for its own sake. The point is that left, right, front, and rear views all have mid-scale geometry to inspect.

#### Ceiling content

The ceiling should include more than just the light.

At least one non-light ceiling-adjacent object or hanging structure should be visible so upward-looking or oblique views do not collapse into empty roof plus light.

#### Central reference content

The room center should contain one or two anchor objects visible from multiple views.

These are useful for:

- cross-camera sanity checks
- checking occlusion and material response
- quickly seeing whether the rig is spatially sensible

### 4. Material and Lighting Intent

The scene should contain enough visual variety to make correctness issues easy to see:

- diffuse materials
- at least one bright emissive element
- optionally one specular or glass-like object if existing realtime materials already support it cleanly

The implementation should still prefer whatever material subset is already stable in the realtime path. This phase should not widen material support just to make the room look richer.

### 5. Correctness Rig

This phase should add or reuse a pinhole four-camera rig dedicated to surround inspection.

Requirements:

- four cameras
- fixed body-relative extrinsics
- distinct look directions rather than only translation offsets
- outputs ordered deterministically by `camera_index`

The rig should behave like a surround inspection rig, not like four copies of the same forward-facing camera.

This phase does not add fisheye correctness coverage yet.

### 6. Validation Path

The primary validation mode is visual inspection through rendered PNG outputs.

The implementation should make it straightforward to run something equivalent to:

```bash
./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-check
```

Success means:

- four output images are produced
- each image is illuminated
- each image contains visible geometry, not mostly empty wall or darkness
- the four images are meaningfully different

This phase may add a lightweight CLI verification for scene selection or output presence, but it should not convert the correctness requirement into brittle image-content assertions.

## Acceptance Criteria

This design is complete when:

1. `render_realtime` supports `--scene final_room`
2. omitting `--scene` still uses the existing smoke scene
3. `--scene final_room --camera-count 4` produces four deterministically ordered views
4. the room contains illuminated content across floor, walls, ceiling, and center
5. the four pinhole views all show meaningful geometry under internal lighting
6. existing benchmark and profiling paths remain centered on the smoke scene

## Risks and Controls

### Risk: scene too heavy for practical correctness iteration

Control:

- reduce object count versus `rttnw_final_scene`
- favor fewer larger forms over dense random scatter

### Risk: scene becomes visually busy but not diagnostically useful

Control:

- use deliberate room-surface placement instead of random clutter
- ensure each wall and the ceiling have visible, distinguishable content

### Risk: correctness scene accidentally pollutes benchmark baseline

Control:

- keep `smoke` as CLI default
- keep matrix runner unchanged unless explicitly extended later

### Risk: camera rig still does not cover enough of the room

Control:

- use directionally distinct extrinsics
- validate with one-frame four-camera PNG output before considering the task done
