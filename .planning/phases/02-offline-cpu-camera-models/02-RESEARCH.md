# Phase 2: Offline CPU Camera Models - Research

**Researched:** 2026-04-20
**Domain:** Offline CPU camera-model integration across the shared-scene entrypoints, legacy `Camera` path tracer, and explicit packed-camera reference rendering [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `src/common/camera.h`, `src/realtime/camera_models.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_viewer_quality_reference.cpp`]
**Confidence:** MEDIUM [VERIFIED: relevant code paths and regression tests were inspected locally; no new implementation was executed during planning]

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Offline fisheye rendering should preserve the full rectangular output image rather than cropping to a circular view or introducing a separate mask contract in this phase. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- **D-02:** Offline `equi62_lut1d` ray generation should follow the existing shared camera math semantics, including the current out-of-range fallback behavior instead of replacing invalid regions with forced background pixels. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`, `src/realtime/camera_models.cpp`]
- **D-03:** Phase 2 validation should treat pixel-center outgoing ray agreement with the shared camera math as the primary contract for offline camera support. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- **D-04:** Phase 2 should also include end-to-end offline regression coverage that shows changing a camera between `pinhole32` and `equi62_lut1d` changes the rendered result through the same shared-scene path. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- **D-05:** `CameraSpec` / `PackedCamera` should become the single source of truth for offline camera model selection and intrinsics. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- **D-06:** Existing CPU preset pose fields such as `lookfrom`, `lookat`, and `vup` may remain as offline pose inputs, but they should no longer define a separate long-lived camera model contract independent of shared camera math. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- **D-07:** `pinhole32` offline rendering should preserve the current depth-of-field behavior through `defocus_angle` and `focus_dist`. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`, `src/common/camera.h`]
- **D-08:** `equi62_lut1d` v1 offline rendering should disable depth-of-field and use zero-distortion defaults rather than inventing a separate fisheye blur model in this phase. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]

### the agent's Discretion
- The exact internal API shape used to bridge offline ray generation from shared camera math into the existing CPU renderer. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- Whether the offline path adapts the existing `Camera` implementation or introduces a narrowly scoped shared-camera ray generator underneath it. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- The precise split between direction-level parity tests and image-level regression tests, as long as both contracts are represented. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]

### Deferred Ideas (OUT OF SCOPE)
- Explicit fisheye valid-region masking or cropped circular output is deferred beyond Phase 2. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- Dynamic calibration controls, explicit runtime intrinsics/extrinsics editing, and non-zero distortion tuning remain later-phase work. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
- Project-wide default switching to fisheye remains scoped to Phase 4 rather than being pulled forward into the offline-only phase. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CAM-02 | Offline CPU rendering uses the selected camera model for ray generation instead of assuming pinhole-only behavior. [VERIFIED: `.planning/REQUIREMENTS.md`] | The offline path currently hard-rejects any non-pinhole `PackedCamera` and reconstructs rays through the legacy `Camera` pinhole viewport (`vfov`, `lookfrom`, `lookat`, `vup`) instead of using shared camera math, so Phase 2 must replace only the ray-generation seam while preserving the existing sampling and path-tracing loop. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `src/common/camera.h`] |
</phase_requirements>

## Project Constraints

- Surface assumptions and ambiguity before implementation instead of silently picking an interpretation. [VERIFIED: user-provided `AGENTS.md` content]
- Prefer the minimum code that solves the problem; avoid speculative abstractions. [VERIFIED: user-provided `AGENTS.md` content]
- Keep edits surgical and directly traceable to offline camera-model support. [VERIFIED: user-provided `AGENTS.md` content]
- Define success through tests and explicit verification, not inferred behavior. [VERIFIED: user-provided `AGENTS.md` content]

## Summary

The safest implementation seam is not a second CPU renderer. `Camera` already owns the path-tracing loop, stratified sampling, defocus handling, and image assembly, while `realtime/camera_models.cpp` already owns the reference-backed pinhole and equidistant projection math. The current offline gap is specifically that `Camera::get_ray(...)` only knows how to emit rays from a pinhole viewport built from `vfov`, so Phase 2 should replace or extend that ray-construction seam and leave `ray_color(...)` and the render loop intact. [VERIFIED: `src/common/camera.h`, `src/realtime/camera_models.cpp`]

`render_shared_scene_from_camera(...)` is a real compatibility surface, not a dead helper. It is used by `tests/test_viewer_quality_reference.cpp` as the CPU reference path for viewer quality checks, and the design notes in `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md` explicitly preserve it as the canonical "explicit packed-camera" offline entrypoint. That means Phase 2 must update both `render_shared_scene(...)` and `render_shared_scene_from_camera(...)` together so they share the same model-aware camera contract rather than drifting into separate code paths. [VERIFIED: `tests/test_viewer_quality_reference.cpp`, `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md`, `src/core/offline_shared_scene_renderer.cpp`]

The current offline preset path still has two layers of camera semantics: `CpuCameraPreset.camera` now carries canonical `CameraSpec`, but `render_shared_scene(...)` still configures the legacy `Camera` from outer preset pose/framing fields and ignores the selected camera model entirely. Phase 2 therefore needs an explicit adapter from `(CpuCameraPreset.camera + lookfrom/lookat/vup + pinhole DoF fields)` into one model-aware offline camera state. This is the boundary where `CameraSpec/PackedCamera` should become the true intrinsics/model source, while `lookfrom/lookat/vup` survive only as pose input. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/core/offline_shared_scene_renderer.cpp`]

The lowest-risk validation strategy is two-layered. First, add a direction-contract test that proves offline ray generation agrees with `unproject_pinhole32(...)` and `unproject_equi62_lut1d(...)` at representative pixels, because the user explicitly locked ray agreement as the primary contract. Second, add image-level regressions that prove switching the selected model changes offline output through both explicit packed-camera rendering and the shared-scene preset path. Existing smoke tests and viewer CPU-reference tests already provide natural integration anchors for that second layer. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`, `src/realtime/camera_models.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_viewer_quality_reference.cpp`]

**Primary recommendation:** Keep the path tracer in `src/common/camera.h`, add a model-aware offline ray-generation seam that consumes canonical camera data, route both offline entrypoints through that seam, preserve pinhole depth of field only, and lock the rollout down with one low-level ray-direction test plus shared-scene and viewer-reference regressions. [VERIFIED: `src/common/camera.h`, `src/core/offline_shared_scene_renderer.cpp`, `tests/test_viewer_quality_reference.cpp`]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Offline ray generation from camera model + intrinsics | `src/common/camera.h` or a tightly coupled offline helper | `src/realtime/camera_models.cpp` | The render loop already lives in `Camera`, but the correct ray math already lives in the shared camera-model helpers. Phase 2 should join them instead of duplicating either side. [VERIFIED: `src/common/camera.h`, `src/realtime/camera_models.cpp`] |
| Adapting shared-scene preset camera data into offline render state | `src/core/offline_shared_scene_renderer.cpp` | `src/scene/shared_scene_builders.h` | This file already resolves scene content, preset sample count overrides, and explicit packed-camera rendering. It is the narrowest place to replace the old pinhole-only camera configuration path. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `src/core/offline_shared_scene_renderer.h`] |
| Pose input for offline preset renders | `CpuCameraPreset` outer fields | shared camera schema | `lookfrom/lookat/vup` are still the existing authored pose surface for offline presets, but Phase 2 should demote them to pose-only inputs and stop using them as a substitute intrinsics/model contract. [VERIFIED: `src/scene/shared_scene_builders.h`, `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`] |
| Explicit offline CPU reference rendering for viewer comparisons | `render_shared_scene_from_camera(...)` | `tests/test_viewer_quality_reference.cpp` | This public API is already consumed by viewer-reference tests and by the scene-definition design notes, so its contract must evolve deliberately with Phase 2 rather than remain pinhole-only by accident. [VERIFIED: `src/core/offline_shared_scene_renderer.h`, `tests/test_viewer_quality_reference.cpp`, `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md`] |
| File-backed scene override coverage for preset camera model changes | `SceneFileCatalog` tests | offline render smoke tests | The repo already uses temp-root scene catalogs to prove file-backed YAML overrides work; Phase 2 can reuse that pattern to prove `render_shared_scene(...)` honors a fisheye-authored CPU preset through the same shared-scene path. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_scene_file_catalog.cpp`, `tests/test_viewer_scene_reload.cpp`] |

## Standard Stack

### Core

| Item | Role | Why Standard |
|------|------|--------------|
| `Camera` | Existing CPU path tracer and image assembly loop. [VERIFIED: `src/common/camera.h`] | It already owns sampling, recursion, and output buffers, so replacing it would create a second renderer instead of a surgical camera-model upgrade. [VERIFIED: `src/common/camera.h`] |
| `PackedCamera` | Canonical runtime camera payload for explicit offline reference renders. [VERIFIED: `src/realtime/camera_rig.h`] | The explicit offline entrypoint already accepts `PackedCamera`, and it carries `model`, dimensions, pose, and the concrete per-model params needed for ray generation. [VERIFIED: `src/core/offline_shared_scene_renderer.h`, `src/realtime/camera_rig.h`] |
| `project_*` / `unproject_*` camera-model helpers | Reference-backed ray math for both supported models. [VERIFIED: `src/realtime/camera_models.cpp`, `tests/test_camera_models.cpp`] | Reusing these functions is the simplest way to make offline and realtime honor the same intrinsics and fallback semantics. [VERIFIED: `src/realtime/camera_models.cpp`] |

### Supporting

| Item | Role | When to Use |
|------|------|-------------|
| `CpuCameraPreset` | Carries canonical `CameraSpec` plus offline-only pose and depth-of-field metadata. [VERIFIED: `src/scene/shared_scene_builders.h`] | Use it to synthesize the model-aware offline camera state for `render_shared_scene(...)` while keeping Phase 2 scoped away from scene-schema redesign. |
| `SceneFileCatalog::scan_directory(...)` | Temp-root file-backed scene override mechanism. [VERIFIED: `src/scene/scene_file_catalog.cpp`] | Use this existing pattern for tests that need a minimal fisheye-authored CPU preset without mutating repo-owned scene files during the phase. |
| `test_viewer_quality_reference.cpp` | Existing integration consumer of the CPU reference path. [VERIFIED: `tests/test_viewer_quality_reference.cpp`] | Use it to guard the explicit packed-camera contract after Phase 2 broadens offline camera support. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extend or adapt `Camera`'s ray-construction seam | Duplicate the path-tracing loop in a new offline renderer | This would fork sampling, progress reporting, and path recursion behavior for a camera-model change that only needs new ray generation. [VERIFIED: `src/common/camera.h`] |
| Reuse shared camera math directly | Re-derive fisheye projection/unprojection locally inside the offline renderer | That would create a second math implementation and break the user's consistency objective across offline and realtime. [VERIFIED: `.planning/PROJECT.md`, `src/realtime/camera_models.cpp`] |
| Drive default offline renders from `CpuCameraPreset.camera` plus pose input | Keep `render_shared_scene(...)` on the old `vfov`-style path and only update explicit packed-camera rendering | That would satisfy only half of CAM-02 and leave the shared-scene preset path pinhole-only. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `.planning/ROADMAP.md`] |

**Installation:** None. Phase 2 stays within the existing repo-local C++ targets and tests. [VERIFIED: `CMakeLists.txt` baseline was already used by Phase 1]

**Version verification:** N/A for the recommended core pieces because they are repo-local types and functions. [VERIFIED: `src/common/camera.h`, `src/realtime/camera_models.cpp`, `src/realtime/camera_rig.h`]

## Architecture Patterns

### System Architecture Diagram

```text
shared scene definition / file-backed scene
`CpuCameraPreset.camera` + pose fields
                 |
                 v
 offline preset adapter in `offline_shared_scene_renderer.cpp`
 (pose + DoF policy + canonical camera model/intrinsics)
                 |
                 v
 model-aware offline ray seam
 (consume `PackedCamera`-like data, call shared `unproject_*`)
                 |
                 v
 `Camera` render loop
 (sampling, path tracing, image assembly)
                 |
                 v
 `cv::Mat` offline output
```

The important boundary is that model selection and intrinsics come from canonical shared camera data, while the legacy offline camera fields contribute only pose and pinhole-only depth-of-field policy. [VERIFIED: `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`, `src/scene/shared_scene_builders.h`, `src/core/offline_shared_scene_renderer.cpp`]

### Recommended Project Structure

```text
src/
├── common/
│   └── camera.h                     # existing CPU tracer; gain a model-aware ray seam
├── core/
│   ├── offline_shared_scene_renderer.h
│   ├── offline_shared_scene_renderer.cpp
│   └── offline_camera_ray_generator.h   # optional narrow helper if the seam should stay out of Camera
├── realtime/
│   ├── camera_models.cpp            # shared pinhole/equi ray math
│   └── camera_rig.h                 # PackedCamera contract already used by explicit offline entrypoint
└── tests/
    ├── test_offline_shared_scene_renderer.cpp
    ├── test_viewer_quality_reference.cpp
    └── test_offline_camera_ray_generator.cpp   # recommended low-level parity target
```

This structure keeps the implementation narrow: one ray seam, one adapter layer, and tests at both the math-contract and shared-scene integration levels. [VERIFIED: `src/common/camera.h`, `src/core/offline_shared_scene_renderer.cpp`, `tests/test_viewer_quality_reference.cpp`]

### Pattern 1: Replace only the ray-generation seam

**What:** Keep `Camera::render(...)` and `ray_color(...)` intact while swapping the logic that converts pixel coordinates into a primary ray. [VERIFIED: `src/common/camera.h`]

**When to use:** Use this because Phase 2 is an offline camera-model upgrade, not a new path tracer. [VERIFIED: `.planning/ROADMAP.md`, `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md`]

**Example:** A `PackedCamera`-driven path where pinhole calls `unproject_pinhole32(...)`, fisheye calls `unproject_equi62_lut1d(...)`, and pinhole-only defocus remains a separate origin perturbation step. [VERIFIED: `src/realtime/camera_models.cpp`, `src/common/camera.h`]

### Pattern 2: One adapter for both offline entrypoints

**What:** Build one canonical offline camera state from either `CpuCameraPreset` or explicit `PackedCamera`, then feed both `render_shared_scene(...)` and `render_shared_scene_from_camera(...)` through the same model-aware render path. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `src/core/offline_shared_scene_renderer.h`]

**When to use:** Use this because the design notes already preserve both public entrypoints, and maintaining two independent camera paths would immediately reintroduce drift. [VERIFIED: `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md`]

### Pattern 3: Temp catalog roots for preset-path verification

**What:** Create temporary scene directories with `scene.yaml`, scan them into a local or global `SceneFileCatalog`, and then assert render or preset behavior on the resulting scene id. [VERIFIED: `tests/test_scene_file_catalog.cpp`, `tests/test_viewer_scene_reload.cpp`]

**When to use:** Use this to prove `render_shared_scene(...)` honors a fisheye-authored CPU preset without mutating repo-owned assets during Phase 2. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_scene_file_catalog.cpp`]

---
## RESEARCH COMPLETE
