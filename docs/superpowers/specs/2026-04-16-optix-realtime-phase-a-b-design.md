# OptiX Realtime Phase A/B Design

Date: 2026-04-16

## Goal

Define the next design stage after the current CUDA/OptiX smoke baseline:

- replace placeholder radiance with real OptiX path tracing
- keep support for `1..4` cameras
- preserve `pinhole32` and `equi62_lut1d`
- preserve `Lambertian`, `Metal`, `Dielectric`, and `DiffuseLight`
- introduce a profile-driven performance path that can move toward `4x 640x480 @ 30fps`

This stage is intentionally split into correctness first, then performance.

## Current Baseline

The branch already has:

- explicit frame convention helpers
- host-side camera math for `pinhole32` and `equi62_lut1d`
- packed `CameraRig` support for `1..4` cameras
- backend-neutral `SceneDescription`
- OptiX/CUDA context bring-up
- direction-debug output
- placeholder radiance output
- a multi-camera smoke pipeline
- a CPU/GPU baseline comparison test
- a `render_realtime` CLI for smoke output and timing

What is still missing is the real radiance integrator and the performance path beyond smoke behavior.

## Chosen Direction

Use a two-phase design:

- **Phase A:** build a correct, testable OptiX path tracing baseline for `1..4` cameras
- **Phase B:** add profile-driven realtime optimizations that move the system toward the target framerate

This is preferred over jumping directly to multi-camera reuse because the repository still lacks a real GPU radiance baseline. Without that baseline, performance work has no stable reference.

## Non-Goals

- rewriting the CPU renderer into a unified CPU/GPU implementation
- targeting more than `4` cameras
- adding volumetric transport in this stage
- guaranteeing `30fps` before the correctness baseline exists
- pixel-exact CPU/GPU image matching
- introducing advanced global illumination reuse methods in the first correctness pass

## Phase Structure

### Phase A: Real OptiX Radiance Baseline

Phase A replaces placeholder radiance with a real path tracing integrator and must satisfy:

- real image generation for `1..4` cameras
- preserved support for diffuse, reflective, and refractive materials
- preserved support for `beauty`, `normal`, `albedo`, and `depth` outputs
- continued CPU/GPU statistical comparison on small scenes
- no dependence on temporal accumulation or denoising for correctness

Completion of Phase A means the branch has a real GPU renderer that can be validated independently of realtime approximations.

### Phase B: Realtime Performance Layer

Phase B builds on the Phase A renderer and is allowed to use approximations to approach the target operating point:

- `4` active cameras
- `640x480`
- `30fps`
- RTX 3090 / 4080 Super / 4090 class GPUs

Phase B may use:

- lower `spp`
- lower `max_bounces`
- stronger temporal accumulation
- denoising
- more aggressive path termination

Phase B must remain profile-driven so correctness and performance modes can be compared cleanly.

## Runtime Architecture

The runtime is split into four layers.

### 1. Scene / Rig Description Layer

Stable input-facing structures:

- `SceneDescription`
- `CameraRig`
- `RenderProfile`

This layer describes what is rendered, not how GPU execution is arranged.

### 2. Packed GPU Data Layer

This layer compiles scene and camera descriptions into GPU-facing buffers:

- packed materials
- packed geometry
- light sampling buffers
- packed cameras
- launch parameters
- dynamic update ranges

This layer owns layout, upload, and synchronization decisions. It must stay separate from shading logic.

### 3. Core OptiX Integrator Layer

This is the Phase A center of gravity. It owns:

- ray generation
- miss behavior
- hit behavior
- shadow visibility queries
- path state progression
- auxiliary buffer output

Its output must remain meaningful even when temporal accumulation and denoising are disabled.

### 4. Realtime Scheduling / Accumulation Layer

This is the Phase B performance layer. It owns:

- per-camera temporal accumulation
- pose-jump reset logic
- denoiser invocation
- multi-camera frame orchestration
- future hooks for shared sampling or reuse

This layer may approximate aggressively, but it must not redefine the semantics of the core integrator.

## Core Integrator Design

### Ray Types

The initial real renderer uses two ray types:

- `radiance`
- `shadow`

This is enough for path continuation plus direct-light visibility checks, while keeping SBT complexity controlled.

### Material Scope

The first real integrator supports:

- `Lambertian`
- `Metal`
- `Dielectric`
- `DiffuseLight`

These are the same material classes already used by the CPU reference path and cover the required diffuse, mirror-like, and refractive behavior.

### Light Transport Strategy

The integrator uses a conservative but complete path tracing structure:

1. generate a primary camera ray
2. intersect geometry
3. accumulate emitted radiance if the hit material is emissive
4. sample direct lighting at the hit point
5. continue the path through BSDF sampling
6. terminate by bounce budget or Russian roulette

This is preferred over a pure random-walk integrator because it behaves better at low `spp`, which directly matters for realtime-oriented profiles.

### Execution Form

The preferred implementation uses a fixed upper-bound loop with path state instead of deep recursive shader logic. This keeps GPU resource usage more predictable and makes future profile tuning easier.

### Output Buffers

Per camera, the integrator must produce at least:

- `beauty`
- `normal`
- `albedo`
- `depth`

These outputs serve both user-visible rendering and downstream denoising, temporal accumulation, and debugging.

### Explicit Deferrals

The first real integrator does not attempt:

- volumetric transport
- BDPT / MLT / ReSTIR GI class methods
- multi-camera reservoir sharing
- texture LOD systems
- broad renderer-side material expansion beyond the current CPU reference set

## Profile Design

The performance layer is expressed as named profiles instead of a bag of unrelated toggles.

### `quality`

Purpose:

- GPU correctness reference
- debugging
- small-scene CPU/GPU comparison

Expected traits:

- higher `spp`
- higher `max_bounces`
- weak or disabled temporal accumulation
- optional denoiser

### `balanced`

Purpose:

- default development profile
- first stable real-time looking profile

Expected traits:

- moderate or low `spp`
- moderate bounce budget
- enabled temporal accumulation
- enabled denoising
- moderate path-cost controls

### `realtime`

Purpose:

- aggressive path toward `4x 640x480 @ 30fps`

Expected traits:

- minimal `spp`
- reduced bounce budget
- strong temporal accumulation
- strong denoiser dependence
- aggressive Russian roulette and cost controls

## Optimization Order

Performance work should follow this order:

1. make single-camera real path tracing correct
2. extend the same real renderer to `1..4` independently rendered cameras
3. introduce temporal accumulation and denoising
4. reduce per-camera path cost through profile settings
5. only then evaluate multi-camera shared optimization strategies

This order is deliberate. Multi-camera reuse is high value, but it is also the easiest place to lose correctness and testability.

## Expected Bottlenecks

For the target operating point, likely first-order bottlenecks are:

- camera ray generation cost for `pinhole32` and `equi62_lut1d`
- total radiance path count
- shadow ray traffic
- auxiliary buffer bandwidth
- denoiser cost
- multi-camera frame orchestration overhead

The design assumes the main budget problem is the aggregate work of four cameras, not a single isolated shader.

## Validation Strategy

Validation is split into four layers.

### 1. Math and Interface Tests

Keep and extend tests for:

- frame conventions
- camera model projection/unprojection
- camera rig packing
- scene description packing

These protect the SLAM-facing contracts and prevent silent axis or layout regressions.

### 2. Integrator Semantics Tests

Add or extend tests that verify:

- real radiance is no longer placeholder output
- each material class has a minimal smoke case
- `beauty`, `normal`, `albedo`, and `depth` are all populated and dimensionally correct
- small-scene CPU/GPU comparisons remain statistically consistent

Comparisons should stay statistical, not per-pixel exact.

### 3. Multi-Camera Behavior Tests

Verify:

- `1`, `2`, `3`, and `4` camera cases all render
- per-camera outputs are independent
- history and reset behavior do not leak across cameras
- changing extrinsics changes image statistics as expected

### 4. Performance / Benchmark Validation

Extend or reuse `render_realtime` so benchmark runs record:

- camera count
- resolution
- `spp`
- bounce budget
- denoiser enabled/disabled
- frame count
- average frame time
- percentile frame time
- accel update time
- denoiser time

The target is repeatable profiling data, not one-off peak numbers.

## Acceptance Criteria

This design stage is considered complete when:

1. placeholder radiance is replaced by a real OptiX path tracing output
2. `1..4` cameras can render real images
3. CPU/GPU small-scene statistical comparisons still pass
4. `render_realtime` can emit profile-aware timing data
5. at least one reproducible `balanced` or `realtime` benchmark configuration exists

## Recommended Next Step

The next implementation plan should decompose this design into:

- Phase A integrator correctness tasks
- Phase A validation tasks
- Phase B profile and accumulation tasks
- Phase B benchmark/reporting tasks

That plan should stay on the current branch and build on the existing smoke and baseline infrastructure rather than replacing it.
