# CUDA Realtime Profiling and Low-Risk Optimization Design

## Goal

Establish a repeatable profiling workflow for the current CUDA realtime renderer, then use that evidence to land one low-risk optimization that is directly justified by measured cost.

This design is intentionally narrower than a general performance sprint. It does not try to solve the full `4x 640x480 @ 30fps` target in one pass. It exists to make the next optimization phase evidence-driven.

## Current State

The current branch already has:

- real OptiX radiance output for `1..4` cameras
- `pinhole32` and `equi62_lut1d` ray generation on the GPU
- realtime pipeline integration with profile-aware denoise gating
- a `render_realtime` CLI with profile selection and coarse timing output
- focused correctness and smoke coverage for renderer, pipeline, and CLI paths

The latest measured CLI smoke at `4x 640x480` is still around `4.9 fps`, far below the eventual target. The current timing output is useful but too coarse to answer the next performance questions with confidence.

## Scope

This sub-project includes:

1. richer host-side frame and per-camera timing in `render_realtime`
2. CUDA event timing for key GPU stages
3. structured benchmark output in both human-readable and machine-readable forms
4. a fixed benchmark matrix for `1 / 2 / 4` cameras and `balanced / realtime` profiles
5. one low-risk optimization selected from the profiling results

This sub-project does not include:

- replacing the current denoiser stub with a real OptiX denoiser
- broad integrator refactors
- camera model math changes
- aggressive shader-side algorithm changes
- multi-camera shared rendering strategies

## Primary Questions to Answer

The profiling output must make it straightforward to answer:

1. what is the most expensive stage at `4x 640x480`
2. where cost grows when scaling from `1` camera to `2` and `4`
3. why `balanced` and `realtime` currently do not separate enough in wall-clock performance

These questions are more important than adding many timing fields with unclear interpretation.

## Recommended Approach

### Option A: Host-only profiling

- extend the CLI with more wall-clock timers
- keep output simple

Pros:

- smallest code change
- very low implementation risk

Cons:

- poor visibility into GPU stage boundaries
- cannot reliably separate render, denoise, and synchronization effects

### Option B: Host profiling plus CUDA event timing

- keep wall-clock frame and per-camera timers
- add CUDA event timing for key GPU stages
- export text and structured benchmark artifacts

Pros:

- enough fidelity to guide the next optimization step
- still fits naturally into the existing repo and CLI workflow
- low risk compared with external-profiler-driven redesign

Cons:

- requires some timing plumbing in the CLI and renderer-facing paths

### Option C: NVTX-first workflow

- instrument with ranges
- rely primarily on Nsight capture workflows

Pros:

- deepest eventual visibility

Cons:

- too heavy for day-to-day benchmark regression
- poor fit for a first repeatable in-repo baseline

## Chosen Direction

Use Option B.

The codebase needs an internal, repeatable benchmark baseline first. External profilers remain useful later, but the immediate need is a stable profiling path that can run inside normal development and CI-like smoke flows.

## Design

### 1. Benchmark Entry Point

`render_realtime` remains the single profiling entry point for this phase.

The CLI should continue to render actual frames and emit images unless the benchmark mode explicitly disables or separates image writes later. This preserves the current smoke behavior and avoids profiling a path that users never execute.

The CLI becomes responsible for:

- collecting host wall-clock timings
- collecting CUDA event timings for GPU-visible stages
- emitting structured benchmark artifacts
- recording enough metadata to compare runs across profiles and camera counts

It should not become a second pipeline abstraction. It remains a benchmark-oriented executable layered on existing renderer infrastructure.

### 2. Timing Model

Profiling data is split into frame-level, camera-level, and aggregate records.

#### Frame-level fields

- `frame_index`
- `camera_count`
- `profile`
- `width`
- `height`
- `samples_per_pixel`
- `max_bounces`
- `denoise_enabled`
- `frame_ms`
- `fps`

#### Stage-level fields

- `render_ms`
- `denoise_ms`
- `download_ms`
- `image_write_ms`
- `host_overhead_ms`

`host_overhead_ms` is derived rather than directly timed. Its purpose is to expose orchestration and synchronization cost that is not otherwise attributed.

#### Camera-level fields

For each active camera in a frame:

- `camera_index`
- `render_ms`
- `denoise_ms`
- `download_ms`
- `average_luminance`

This is needed because multi-camera regressions may not be evenly distributed across all views.

#### Aggregate fields

At run end, compute:

- `avg`
- `p50`
- `p95`
- `max`

for at least `frame_ms`, `render_ms`, `denoise_ms`, and `download_ms`.

### 3. Timing Sources

Use two timing sources.

#### Host wall-clock

Use steady-clock timing for:

- full frame time
- denoise stage in the current branch, because the denoiser is still a CPU-side stub
- image write time
- high-level orchestration boundaries

#### CUDA events

Use CUDA events for:

- render stage
- explicit device-to-host download stage

When a real GPU denoiser replaces the current stub, `denoise_ms` should move to CUDA-event timing as well. Until then, the important property is that the reported source of each stage timing is explicit and stable across runs.

If a stage is skipped, record zero rather than inventing a synthetic value.

The design does not require exact sub-kernel attribution yet. It is enough to separate the current top-level GPU phases.

### 4. Output Formats

The profiling run emits:

1. terminal summary for direct human use
2. CSV for simple diffing and spreadsheet use
3. JSON for structured archival and scripts

#### Terminal output

Keep one concise line per frame plus a final aggregate summary.

The summary must expose enough information that a single pasted block answers:

- which profile was used
- how many cameras were active
- resolution
- core profile knobs
- average and percentile frame time
- denoise cost

#### CSV

Use one row per frame.

The CSV should be easy to compare across runs without nested parsing.

#### JSON

The JSON should contain:

- run metadata
- per-frame records
- per-camera records
- aggregate statistics

This file is the canonical artifact for later scripts and plotting.

### 5. Benchmark Matrix

The first fixed matrix is:

- camera counts: `1`, `2`, `4`
- profiles: `balanced`, `realtime`
- resolution: `640x480`

Use a small frame count that keeps iteration practical while still allowing percentile calculations.

The matrix does not need to include `quality` in this phase because the immediate target is realtime-oriented behavior and scaling.

### 6. Low-Risk Optimization Selection Rule

This design intentionally does not pre-commit to a specific optimization before profiling.

After the profiling data is available, select exactly one optimization that satisfies all of the following:

1. it targets one of the highest measured costs in the `4 camera` matrix
2. it is bounded to CLI, host orchestration, launch plumbing, or download behavior
3. it does not change image formation semantics
4. it can be validated with the same benchmark matrix before and after the change

Priority order for candidate classes:

1. host orchestration cost reduction
2. GPU/host boundary cost reduction
3. benchmark-path write isolation or reduction

This is deliberately conservative. The point is to prove the profiling loop works before taking on riskier renderer-side changes.

## Validation Strategy

Validation has three layers.

### 1. Correctness preservation

The existing focused build, renderer tests, pipeline tests, and CLI smoke must stay green.

This profiling work must not change rendering semantics.

### 2. Profiling artifact validation

Verify that:

- terminal output includes the expected timing fields
- CSV and JSON files are both produced
- aggregate statistics are internally consistent
- skipped stages record zero rather than junk values

### 3. Optimization proof

For the selected low-risk optimization:

- run the same benchmark matrix before and after
- compare the targeted stage cost
- confirm no existing focused tests regress

The optimization is only accepted if the benchmark demonstrates a measurable improvement in the targeted metric.

## Risks and Constraints

### Timing distortion

Profiling itself can perturb runtime. The design accepts this and prefers stable, comparable instrumentation over chasing absolute truth.

### Denoiser interpretation

The current denoiser path is still a stub. Any `denoise_ms` reported in this phase reflects the current implementation, not the eventual real OptiX denoiser cost. This is acceptable as long as the output is described honestly.

### Benchmark pollution by image writes

PNG writing can dominate or hide rendering differences. This phase therefore records image write time explicitly so it can be separated from rendering cost even if writes remain enabled.

## Acceptance Criteria

This profiling phase is complete when:

1. `render_realtime` emits richer host and CUDA-event timing data
2. terminal, CSV, and JSON outputs are all available
3. a reproducible `1 / 2 / 4 camera × balanced / realtime` benchmark matrix can be run
4. the resulting report makes the top stage cost and scaling loss obvious
5. exactly one low-risk optimization is selected from the measured data and validated by before/after benchmark results
6. the existing focused verification suite still passes

## Next Step

Once the spec is approved, the implementation plan should break this work into:

1. profiling data model and CLI output changes
2. CUDA event instrumentation
3. structured CSV/JSON artifact writing
4. benchmark matrix execution and baseline capture
5. low-risk optimization selection and validation
