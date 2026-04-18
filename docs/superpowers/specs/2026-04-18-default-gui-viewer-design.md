# Default GUI Realtime Viewer Design

Date: 2026-04-18

## Goal

Add a default interactive GUI mode for the realtime renderer so the user can move a body through the scene like a game and see all four attached cameras update live.

The default viewer behavior should be:

- start in the `final_room` scene
- open a window that shows four camera views in a `2x2` grid
- control the body with mouse `yaw + pitch`
- move the body with `WASD`
- keep four cameras rigidly attached to the body
- initialize the body from a fixed in-code spawn position and orientation

This is the new default interactive mode. The existing PNG-writing CLI remains available for smoke runs, profiling, and offline correctness checks.

## Current State

The repository already contains the core pieces needed for a first viewer:

- a realtime GPU rendering path under `src/realtime/`
- a CLI entrypoint in `utils/render_realtime.cpp`
- scene-selection support for `smoke` and `final_room`
- a four-camera surround rig for `final_room`
- per-camera radiance download into CPU-side frame buffers

What is missing is the interactive shell around that renderer:

- no window creation
- no realtime input loop
- no body-pose controller
- no onscreen presentation of multiple cameras
- no default viewer executable

The current CLI is frame-batch oriented. It renders frames, optionally writes PNGs, and exits. That is the wrong execution model for the requested default experience.

## Scope

This sub-project includes:

1. adding a dedicated GUI viewer executable for interactive realtime rendering
2. making `final_room` the default viewer scene
3. displaying four attached cameras in a `2x2` layout
4. controlling a single body pose with mouse and keyboard
5. deriving all four camera poses from that body pose
6. defining a fixed default spawn pose in code

This sub-project does not include:

- replacing the existing benchmark CLI
- adding runtime scene editing tools
- adding UI widgets, overlays, or ImGui panels
- exposing spawn pose through config files or CLI flags in this phase
- adding roll control
- adding a true OptiX denoiser in this phase
- guaranteeing 30 FPS in the first viewer version

## Requirements

### Functional

- The default interactive mode opens a window and continuously updates while the process is running.
- The viewer renders four camera outputs simultaneously.
- The four outputs are presented in a `2x2` equal-size grid.
- The user controls a body pose, not an individual camera pose.
- The four cameras are rigidly attached to that body pose.
- The default camera yaw offsets are:
  - camera 0: `0 deg`
  - camera 1: `+90 deg`
  - camera 2: `-90 deg`
  - camera 3: `180 deg`
- Mouse movement updates body `yaw` and `pitch`.
- `WASD` movement follows the current facing direction, including pitch, so motion behaves like a flying camera rather than a ground-locked FPS controller.
- The viewer starts in `final_room` from a fixed code-defined spawn pose.

### Product

- The first version should prioritize immediate interactivity over final image quality.
- The viewer should feel continuous and responsive enough for scene inspection.
- Existing CLI smoke, profiling, and correctness flows must remain intact.

## Options Considered

### Option A: Keep extending the existing CLI

- run a loop inside `render_realtime`
- show images through a lightweight display layer

Pros:

- fewer files
- reuses the current executable

Cons:

- mixes batch rendering and interactive viewer concerns
- makes the default CLI behavior harder to reason about
- encourages more branching inside a file that is already doing too much

### Option B: Add a dedicated native viewer executable using `GLFW + OpenGL`

- keep the existing CLI for offline and benchmark use
- create a separate interactive viewer binary
- use GLFW for windowing and input
- use OpenGL only for presentation of the rendered images

Pros:

- best fit for mouse capture, keyboard polling, and continuous redraw
- clean separation between viewer mode and batch mode
- straightforward `2x2` presentation model

Cons:

- adds a windowing dependency
- first version likely uses CPU round-trip presentation instead of CUDA/OpenGL interop

### Option C: Use OpenCV windowing for the first interactive mode

- concatenate the four frames
- display with OpenCV HighGUI

Pros:

- minimal dependency change
- quick to prototype

Cons:

- weak input model for game-like control
- poor fit for cursor capture and continuous navigation
- unlikely to produce the requested default experience

## Chosen Direction

Use Option B.

Add a separate viewer executable named `render_realtime_viewer`, backed by `GLFW + OpenGL`.

The renderer remains responsible for producing camera images. The viewer layer is responsible for:

- window lifetime
- input handling
- body pose updates
- generating the four-camera rig from that body pose
- presenting the four images in a `2x2` layout

This keeps the offline CLI intact and makes the new default interactive mode explicit rather than overloaded into the benchmark entrypoint.

## Design

### 1. Executable Model

Add a new interactive executable instead of mutating the current CLI into a dual-purpose tool.

Responsibilities:

- `render_realtime`
  - offline smoke runs
  - benchmark/profiling output
  - correctness image dumps
- `render_realtime_viewer`
  - realtime window
  - default `final_room` interactive mode
  - body navigation
  - four-camera display

The viewer should be treated as the default human-facing mode. The CLI remains a utility and test harness.

### 2. Viewer Runtime Structure

The main loop should follow this order:

1. poll window/input events
2. update body pose from mouse and keyboard state
3. build the current four-camera rig from the body pose
4. render the four camera views
5. upload the rendered images to display textures
6. draw the `2x2` layout
7. swap buffers

This loop is deliberately simple. The first version should avoid background render scheduling, UI frameworks, and speculative architecture.

### 3. Body Pose Model

The body pose is the only user-controlled pose.

State:

- position: `Eigen::Vector3d`
- yaw: scalar angle
- pitch: scalar angle

Not included:

- roll
- per-camera free look
- per-camera independent positions

The body pose defines the common frame from which all four camera extrinsics are derived.

### 4. Input Semantics

#### Mouse

Mouse motion controls:

- horizontal delta -> body yaw
- vertical delta -> body pitch

Pitch should be clamped to `[-80 deg, +80 deg]` so the view cannot flip over accidentally.

The cursor should be captured/hidden while the viewer is active so motion feels continuous.

#### Keyboard

`WASD` movement is flying-camera style:

- `W`: move forward along current facing direction
- `S`: move backward along current facing direction
- `A`: strafe left along current right-vector basis
- `D`: strafe right along current right-vector basis

Pitch affects forward motion. Looking upward and pressing `W` should move upward as part of the forward direction.

No extra controls are required in this phase unless needed for viewer exit by the window system.

### 5. Camera Rig Derivation

The cameras do not have independent gameplay controls. They are rigid sensors attached to the body.

The rig is generated every frame from:

- the body translation
- the shared body `yaw + pitch`
- each camera's fixed yaw offset

Default offsets:

- `cam_0 = 0 deg`
- `cam_1 = +90 deg`
- `cam_2 = -90 deg`
- `cam_3 = 180 deg`

All four cameras share the same position in this first version. Their relative difference is orientation only.

This yields a surround rig that:

- turns together with the body
- looks up/down together with the body
- preserves the four-way horizontal coverage pattern

### 6. Default Scene and Spawn

The viewer defaults to `final_room`.

The initial spawn pose is defined in code, not in CLI flags or configuration files, because the user explicitly requested a hardcoded default first.

The default spawn pose is fixed as:

- position: `(0.0, 0.35, 0.8)` in renderer/world coordinates
- yaw: `0 deg`
- pitch: `0 deg`

This spawn was chosen to satisfy these constraints:

- inside the room
- not intersecting geometry
- provides useful forward, side, and rear visibility at startup
- avoids starting directly under or inside a bright emitter

The first implementation should treat this spawn pose as part of the default viewer contract.

### 7. Display Layout

The viewer presents four images in a `2x2` equal-size grid.

Suggested order:

- top-left: camera 0
- top-right: camera 1
- bottom-left: camera 2
- bottom-right: camera 3

The exact order should be documented and kept deterministic.

The layout should not switch dynamically in this phase. The goal is spatial consistency, not UI customization.

### 8. Rendering Profile Strategy

The viewer should not default to the current correctness-oriented `quality` profile. It should use a viewer-specific low-latency profile or an existing low-cost profile tuned for interaction.

The first version should prioritize:

- continuous redraw
- low interaction latency
- immediate pose response

over:

- denoised final images
- high sample counts
- offline-quality lighting convergence

Concretely, this means the first version should tolerate:

- noisier images
- lower bounce budget
- lower render resolution if needed

The user explicitly prefers “get an interactive viewer working first” over waiting for stable high-quality rendering.

### 9. Presentation Path

The simplest first presentation path is:

- render on GPU
- download frame data to CPU using the current renderer path
- upload displayable image data to OpenGL textures
- draw textured quads for the four grid cells

This is not the final optimal path, but it is acceptable for the first viewer because:

- the project already has CPU-side `RadianceFrame` output
- it avoids coupling the first viewer to CUDA/OpenGL interop complexity
- it isolates the GUI problem from deeper renderer optimization work

The design should keep future CUDA/OpenGL interop possible, but it should not be required for phase one.

### 10. Relationship to Existing Tests and Tools

The viewer should be additive.

It must not change the meaning of:

- benchmark CSV/JSON outputs
- existing `render_realtime` smoke commands
- current correctness-focused `final_room` CLI usage

The existing CLI and tests remain the automation path. The viewer is primarily a human inspection tool in this phase.

## Acceptance Criteria

This design is complete when:

1. launching the viewer opens a window instead of writing PNGs by default
2. the default viewer scene is `final_room`
3. the window shows four simultaneously rendered cameras in a `2x2` equal grid
4. mouse motion updates body `yaw + pitch`
5. `WASD` moves the body through the scene according to the current facing direction
6. the four cameras move and rotate with the body while preserving the default yaw offsets
7. the initial spawn pose comes from code-defined defaults
8. the existing batch CLI path remains usable for profiling and correctness runs

## Risks and Controls

### Risk: viewer feels sluggish because current four-camera path is already expensive

Control:

- use a lightweight interactive profile
- allow lower initial resolution if needed
- disable non-essential postprocessing in the first phase

### Risk: viewer code pollutes batch CLI code

Control:

- create a separate executable
- keep interactive state and windowing out of `render_realtime.cpp`

### Risk: body/camera frame conventions become inconsistent

Control:

- derive viewer camera poses through existing rig/frame helpers
- keep body pose as the single source of truth
- avoid ad hoc axis flips inside the viewer

### Risk: startup pose is technically valid but poor for inspection

Control:

- choose the spawn inside `final_room`
- validate that all four default views show useful geometry
- tune the spawn once during implementation instead of adding configuration early

## Non-Goals for This Phase

- true OptiX denoiser integration
- temporal accumulation integrated into interactive camera motion
- runtime scene selection UI
- configurable camera count
- configurable camera offset layout
- multiple viewer layouts
- editor tooling
- perfect frame pacing

## Implementation Notes

The current repository state suggests two practical follow-up tasks after this spec:

1. add the viewer executable, input loop, body-pose state, and four-panel display
2. tune the interactive render profile and spawn pose so the default experience is usable

Those should be planned as concrete implementation steps in the next phase.
