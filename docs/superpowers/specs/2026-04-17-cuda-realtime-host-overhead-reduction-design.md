# CUDA Realtime Host Overhead Reduction Design

## Goal

Reduce the dominant `host_overhead_ms` cost in the current CUDA realtime benchmark path, starting with the `4x 640x480` case, without changing rendering semantics or profiling schema.

This phase is intentionally narrow. It does not try to solve the full `30 fps` target in one jump, and it does not prioritize integrator-side changes while the current measurements show the main bottleneck is outside the path tracing kernel.

## Current State

The current branch already has:

- a profiled OptiX radiance path with `render_ms` and `download_ms`
- CLI benchmark reporting with `frame_ms`, `render_ms`, `denoise_ms`, `download_ms`, `image_write_ms`, and `host_overhead_ms`
- a reproducible matrix runner for `1 / 2 / 4` cameras and `balanced / realtime`
- a benchmark-mode `--skip-image-write` path

The latest `build/realtime-matrix-final` baseline shows that for `4` cameras:

- `realtime-c4`
  - `frame_ms.avg = 160.286`
  - `render_ms.avg = 0.634`
  - `download_ms.avg = 3.714`
  - `denoise_ms.avg = 44.732`
  - `host_overhead_ms.avg = 111.206`
- `balanced-c4`
  - `frame_ms.avg = 158.991`
  - `render_ms.avg = 0.757`
  - `download_ms.avg = 3.846`
  - `denoise_ms.avg = 45.829`
  - `host_overhead_ms.avg = 108.559`

The dominant cost is therefore host-side orchestration, with denoise second. The path tracing kernel itself is not currently the main limiter.

## Scope

This sub-project includes:

1. reducing repeated host/device setup work inside the realtime benchmark path
2. removing per-frame and per-camera allocation churn on the download path
3. allowing aggressive multi-camera orchestration, including multi-renderer and multi-stream execution
4. optionally parallelizing host-side denoise work when it remains a visible frame cost
5. preserving the current profiling output format so before/after results remain comparable

This sub-project does not include:

- edits to `programs.cu` path tracing math, BSDF sampling, or camera model projection math
- replacing the current denoiser stub with a real OptiX denoiser
- changing benchmark CSV/JSON schema
- changing the current renderer-facing image semantics

## Primary Questions to Answer

The implementation should make it straightforward to answer:

1. how much of `host_overhead_ms` came from repeated scene upload and buffer allocation
2. how much of `frame_ms` can be recovered by overlapping or parallelizing multi-camera execution
3. whether denoise remains the next major bottleneck after host orchestration is reduced

## Options Considered

### Option A: Conservative host cleanup only

- keep one renderer instance
- cache scene upload
- reuse pinned host buffers
- keep camera execution fully serial

Pros:

- smallest code change
- lowest concurrency risk

Cons:

- unlikely to remove the majority of the current `host_overhead_ms`
- leaves multi-camera orchestration fully serialized

### Option B: Per-camera renderer pool with multi-stream execution

- create up to `4` renderer instances
- give each renderer its own CUDA stream and owned buffers
- prepare static scene state once per run
- dispatch all active camera renders first, then collect results
- optionally parallelize denoise on the host

Pros:

- directly attacks the current dominant bottleneck
- keeps renderer responsibilities and CLI scheduling responsibilities separated
- still avoids kernel-side risk

Cons:

- requires a more explicit renderer lifecycle
- adds concurrency and ownership complexity

### Option C: Full frame pipeline overlap

- add cross-frame overlap between render, download, denoise, and write
- introduce deeper buffering across frames

Pros:

- highest eventual upside

Cons:

- substantially larger validation surface
- risks confusing the current profiling interpretation

## Chosen Direction

Use Option B, and include the useful pieces of Option A inside it.

This is the highest-leverage change that still stays on the host/orchestration side of the boundary. It matches the measured bottleneck and leaves shader and camera math untouched.

## Design

### 1. Responsibility Split

This phase should separate three responsibilities more clearly.

#### `OptixRenderer`: per-camera executor

Each renderer instance owns:

- one CUDA stream
- one set of device frame buffers
- one set of pinned host staging buffers
- one uploaded scene state

It is responsible for rendering one camera view at a time on its own stream and returning a profiled frame.

It should no longer behave like a stateless helper that re-uploads scene data on every render call.

#### `render_realtime`: multi-camera scheduler

The CLI remains the benchmark entry point, but it becomes a scheduler over `1..4` renderer instances.

It is responsible for:

- constructing the renderer pool
- preparing shared scene inputs once per run
- dispatching work across active camera slots
- aggregating profiling results
- running denoise and writing artifacts

It should not absorb renderer internals.

#### Denoise and artifact stages: post-processing layer

Denoise and artifact writing remain post-render stages outside `OptixRenderer`.

This keeps the profiling model stable:

- renderer reports GPU render and download timing
- CLI reports denoise, image write, host overhead, and frame timing

### 2. Scene and Buffer Lifetime

#### Scene upload reuse

The renderer currently re-uploads scene state inside each `render_radiance_profiled()` call.

For the benchmark smoke scene, this work is redundant across all cameras and all frames. The new lifecycle should:

- upload scene buffers once during initialization or explicit prepare
- reuse them until the scene changes
- rebuild only when scene content actually differs

The design does not require a general scene hashing system. A narrow explicit prepare step is acceptable for this phase.

#### Persistent pinned host staging

The profiled download path currently allocates and frees pinned host memory on every frame and camera.

This phase should change that to resolution-keyed reuse:

- allocate one staging set per renderer
- reuse staging when width and height are unchanged
- free only on renderer destruction or explicit resize

This is required because per-call pinned allocation is exactly the kind of host-side management cost the benchmark is trying to isolate.

### 3. Multi-Camera Scheduling

The current path effectively performs:

1. render camera 0
2. download camera 0
3. denoise camera 0
4. render camera 1
5. download camera 1
6. denoise camera 1
7. repeat

This phase should move to a more aggressive schedule:

1. create one renderer per active camera
2. for a frame, submit all active camera renders first
3. collect downloaded frames after submission
4. run denoise across camera results
5. write artifacts and emit reports

The exact implementation may be either:

- multi-stream launch inside one controller thread, or
- one host worker per camera that owns one renderer

The requirement is not a specific threading model. The requirement is that the new design can exploit the user's desktop GPU class and avoid trivially serial camera orchestration.

### 4. Host-Side Denoise Parallelism

After image write removal, denoise still costs roughly `45 ms` at `4` cameras.

This phase may parallelize denoise across a fixed-size host worker pool as long as:

- profiling fields remain unchanged
- per-camera denoise timing is still recorded
- output image semantics remain unchanged

The worker count should be bounded by active camera count. This phase should not introduce a wide-open thread fan-out policy.

### 5. Profiling Contract

The following profiling fields must remain unchanged:

- `frame_ms`
- `render_ms`
- `denoise_ms`
- `download_ms`
- `image_write_ms`
- `host_overhead_ms`
- per-camera timing records
- aggregate CSV/JSON structure

This is a hard requirement because the whole point of this phase is to compare against the existing baseline with the same schema.

### 6. Correctness Contract

This phase must not change image semantics.

At minimum, correctness coverage should preserve or extend:

- existing profiled/non-profiled parity checks
- existing focused renderer and CLI tests
- a multi-renderer smoke check that confirms equivalent output for `1` and `4` active cameras under the same inputs

The implementation may change lifecycle and scheduling, but not the image values that the benchmark path produces.

## Validation

Validation should use the same matrix runner and the same report artifacts as the current baseline.

### Focused verification

The existing focused build and test set must remain green, including:

- renderer correctness tests
- CLI profiling tests
- skip-write benchmark test
- pipeline and reference agreement tests

### Performance verification

Run the same benchmark matrix:

- camera counts: `1 / 2 / 4`
- profiles: `balanced / realtime`
- resolution: `640x480`

Primary comparison points are:

- `realtime-c4`
- `balanced-c4`

The key before/after metrics are:

- `host_overhead_ms.avg`
- `denoise_ms.avg`
- `frame_ms.avg`
- `fps`

## Risks and Constraints

### Concurrency risk

Multiple renderer instances and streams increase lifecycle complexity. The implementation should keep ownership explicit and avoid hidden sharing of mutable CUDA resources between renderer instances.

### Profiling interpretation risk

If multiple cameras are launched concurrently, summed per-camera stage timings may exceed frame wall-clock time. That is acceptable as long as:

- `frame_ms` remains the wall-clock truth
- per-camera fields are still clearly stage-local timings
- `host_overhead_ms` is still derived from the same frame-level accounting

### Scope control

This phase should not drift into integrator optimization merely because render time remains low. The purpose here is to remove host-side waste first.

## Completion Criteria

This phase is complete when:

1. the benchmark path reuses scene upload and pinned host staging instead of rebuilding them per camera render
2. multi-camera orchestration is no longer trivially serialized through one renderer instance
3. profiling schema remains unchanged
4. focused correctness tests remain green
5. the benchmark matrix still runs with one command
6. `host_overhead_ms.avg` shows a measurable reduction in the `4 camera` runs
7. the resulting baseline makes it clear whether denoise is the next largest cost to attack
