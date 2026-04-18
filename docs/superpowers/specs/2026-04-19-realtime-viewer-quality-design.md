# Realtime Viewer Quality Improvement Design

Date: 2026-04-19

## Goal

Improve realtime viewer image quality so that:

- `final_room` is the first acceptance target
- the mechanism is reusable across viewer scenes
- interactive movement stays at or above `30 FPS`
- when the viewer becomes stationary, the image converges toward the offline reference within roughly `0.5-1.0` seconds

The main reported symptom is many stable black speckles / dark pinholes during realtime rendering, even while the camera is stationary.

## Current State

The repository already has:

- a working realtime viewer executable at `utils/render_realtime_viewer.cpp`
- a realtime renderer that produces a single radiance frame per camera
- viewer scene switching and four-camera rig generation
- offline reference rendering paths and basic GPU-vs-CPU comparison coverage

However, the current viewer quality path is intentionally minimal:

- `src/realtime/viewer/default_viewer_scene.cpp` uses a very aggressive default profile:
  - `samples_per_pixel = 1`
  - `max_bounces = 2`
  - `enable_denoise = false`
- the viewer displays single-frame output directly
- there is no true temporal accumulation path in the viewer
- the current denoiser wrapper is not a real OptiX denoiser and is not part of the default viewer path

This means the viewer is trying to meet both interactivity and quality entirely within a highly constrained single-frame budget.

## Scope

This work includes:

1. adding a viewer-only quality controller between the viewer loop and final display
2. splitting viewer rendering into `preview` and `converge` modes
3. adding per-camera temporal accumulation for the displayed beauty output
4. adding simple invalid-sample / firefly suppression before accumulation
5. resetting or rebuilding history when motion, scene identity, or frame shape changes invalidate it
6. defining tests that verify stationary convergence improves agreement with an offline reference

This work does not include:

- rewriting the realtime renderer into a full offline-quality path tracer
- implementing a full OptiX denoiser integration in this phase
- redesigning scene switching, camera rig generation, or body-pose controls
- adding scene-specific quality branches for `final_room`
- guaranteeing offline-equivalent quality

## Requirements

### Functional

- the default realtime viewer must maintain at least `30 FPS` while the body is moving
- the viewer must improve image quality while stationary through temporal accumulation
- the quality mechanism must be scene-agnostic at the viewer level
- `final_room` is the first scene used for acceptance and tuning
- scene switches must not reuse accumulation history from the previous scene
- frame-size changes must rebuild the corresponding history buffers
- invalid pixel values must not poison temporal history

### Product

- stationary realtime output should move materially closer to the offline reference than the current single-frame viewer output
- black speckles / dark pinholes should stop being the dominant visual defect during normal stationary inspection
- movement should remain responsive, with quality degraded deliberately during motion rather than allowing frame rate collapse

## Options Considered

### Option A: Tune The Existing Single-Frame Profile Only

Raise viewer `samples_per_pixel`, `max_bounces`, or both, while keeping the current direct-display model.

Pros:

- smallest code change
- low implementation risk

Cons:

- still bounded by single-frame quality
- poor fit for the `30 FPS while moving` constraint
- unlikely to close enough distance to the offline reference in `final_room`

### Option B: Add A Viewer Quality Controller With Motion/Stillness Modes And Temporal Accumulation

Introduce a viewer-side controller that chooses between a low-cost motion profile and a higher-quality stationary convergence path, with temporal history per camera.

Pros:

- matches the desired interaction model directly
- allows movement to stay responsive while stationary quality improves
- reusable across viewer scenes

Cons:

- requires new history and mode-management code
- needs targeted tests for motion thresholds and accumulation resets

### Option C: Prioritize Kernel-Side Sampling Upgrades Before Viewer Accumulation

Invest first in better light sampling, material handling, and kernel-side stability, then decide whether temporal accumulation is still needed.

Pros:

- improves the renderer itself
- benefits any future realtime path

Cons:

- slower path to user-visible improvement
- does not address the absence of a stationary convergence mechanism
- still leaves single-frame quality constrained by movement-time budgets

## Chosen Direction

Use Option B.

The first phase should add a viewer-only quality layer that:

- preserves movement responsiveness through a lightweight `preview` mode
- switches to a stationary `converge` mode once motion becomes small enough
- temporally accumulates beauty output while stationary
- filters invalid or clearly unstable pixel contributions before they enter history

If this first phase still leaves a large gap to the offline reference, a later phase can improve kernel-side estimators. That later work should be driven by evidence from the new convergence path, not guessed upfront.

## Design

### 1. Architecture

Insert a thin viewer quality layer between the viewer loop and final display output.

Responsibility split:

- `utils/render_realtime_viewer.cpp`
  - input polling
  - body pose integration
  - scene switching
  - per-camera render invocation
  - passing raw frames into the quality controller
- `viewer quality controller`
  - motion/stillness classification
  - active profile selection
  - per-camera temporal history management
  - pre-accumulation sample validation / suppression
  - output of final display frames
- `OptixRenderer`
  - continue rendering a single frame for a chosen profile
- existing scene, rig, and pose code
  - remain unchanged except for integration points

Resulting data flow:

1. viewer updates body pose
2. quality controller classifies current motion state
3. quality controller chooses the active render profile
4. renderer produces the current single-frame radiance result
5. quality controller validates and accumulates the beauty output
6. viewer displays the filtered / accumulated output

This keeps quality logic out of scene construction, rig logic, and the renderer core.

### 2. Components

Add a focused component at:

- `src/realtime/viewer/viewer_quality_controller.h`
- `src/realtime/viewer/viewer_quality_controller.cpp`

This component owns per-camera accumulation state.

Each camera keeps a minimal `AccumulationState` with:

- `history_length`
- `accumulated_beauty`
- `last_pose`
- `last_profile_mode`
- optional lightweight debug counters if useful for validation

The first phase should accumulate beauty only. It should not temporally accumulate auxiliary buffers such as:

- normals
- albedo
- depth

Those buffers remain single-frame outputs for now because the current user-facing problem is the final displayed image, not auxiliary-buffer smoothness.

The controller depends only on:

- current viewer pose or equivalent motion signal
- current scene identity
- render frame dimensions
- raw `RadianceFrame` results from the renderer

It must not depend on `final_room` specifics.

### 3. Quality Strategy

#### Preview Mode

`preview` is active while the viewer is moving enough that stationary accumulation would be unstable or misleading.

Its goal is to preserve interactive responsiveness at or above `30 FPS`.

The preview profile should stay lightweight, but it should be less fragile than the current default `1 spp / 2 bounces / denoise off`. In particular, it should avoid obvious quality collapse in major indirect-light and reflection paths.

#### Converge Mode

`converge` is active once motion remains below reset thresholds long enough for history to be meaningful.

Its goal is not offline quality per frame. Its goal is to produce stable single-frame samples that accumulate well over time.

The converge profile may cost more per frame than preview, but still needs bounded cost because it runs inside the live viewer.

#### Temporal Accumulation

While in `converge`, each camera updates its display buffer via progressive averaging:

- first valid frame initializes history
- each later valid frame blends with history using `1 / history_length`

This simple averaging model is preferred over a fixed alpha because:

- it is easier to reason about
- it is easier to test
- it gives clear semantics for convergence as history grows

Whenever history becomes invalid, the camera resets:

- entering or re-entering `preview`
- crossing motion thresholds
- scene change
- resolution change
- any other condition that makes the prior history semantically stale

#### Black Speckle / Firefly Suppression

Before adding the current frame to history, the controller should apply lightweight guards:

- reject `NaN`, `Inf`, or negative beauty values
- reject or clamp values that are clearly outside the intended realtime display range
- avoid letting obviously corrupted pixels overwrite stable history

The purpose is not aggressive image beautification. The purpose is to stop isolated invalid or unstable samples from causing long-lived black pinholes or similar defects in accumulated output.

### 4. Motion Classification And Reset Rules

The controller should use the thresholds already represented in `RenderProfile`:

- `accumulation_reset_rotation_deg`
- `accumulation_reset_translation`

These values become operational rather than merely descriptive.

Decision model:

- if current pose change exceeds either threshold, the camera enters `preview` and its history resets
- if pose remains below thresholds for enough consecutive frames, the camera enters `converge`
- once in `converge`, history grows until another reset condition occurs

The first implementation should keep this logic simple and deterministic. No predictive filtering or complex hysteresis is needed beyond what is required to avoid immediate flapping.

### 5. Error Handling

Only handle conditions that are plausible for this design:

- scene change
  - clear all camera histories
- frame size change
  - rebuild the affected camera history buffers
- renderer output mismatch or insufficient buffer size
  - skip accumulation and surface the error
- invalid current-frame pixels
  - exclude them from history update and prefer the last valid history value or a safe fallback

This keeps the design surgical and focused on the new quality path.

## Testing

### 1. Unit Tests For The Quality Controller

Add direct tests for controller behavior:

- first valid frame initializes history correctly
- repeated stable poses increment `history_length`
- threshold-breaking pose changes clear history
- scene changes clear history
- frame-size changes rebuild history
- invalid pixels do not contaminate accumulated output

### 2. Integration Tests For Viewer Quality Behavior

Add a non-GUI integration path that feeds a sequence of poses and radiance frames into the controller.

Validate:

- stable-pose sequences converge over time
- pose jumps reset accumulation
- preview and converge modes switch as expected

This should avoid depending on the GLFW event loop.

### 3. Reference-Agreement Acceptance Test

Use `final_room` as the first acceptance scene.

At a fixed pose:

1. render or load an offline reference image
2. render a realtime single-frame image
3. render a realtime stationary accumulated result after several convergence frames
4. compare both realtime results against the offline reference

Acceptance requirement:

- the accumulated realtime result must reduce error relative to the single-frame realtime result

Use a simple, auditable image-distance metric such as:

- mean absolute error
- or mean squared error

Average luminance alone is not sufficient because it is too coarse to capture the actual visual defect being targeted.

## Success Criteria

Phase 1 is successful when:

- stationary `final_room` viewer output is materially closer to the offline reference than current single-frame output
- black speckles / dark pinholes are no longer the dominant visual defect during stationary viewing
- the default viewer stays at or above `30 FPS` during movement
- the mechanism works for viewer scenes in general, without scene-specific branches

## Follow-Up Boundary

If Phase 1 still leaves a substantial reference gap, the next phase should evaluate kernel-side improvements such as better sampling or material handling.

That work should be based on measured residual error after temporal accumulation is in place, not introduced prematurely in this phase.
