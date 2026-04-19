# Phase 2: Offline CPU Camera Models - Context

**Gathered:** 2026-04-20
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase makes the offline CPU renderer honor the selected per-camera model from shared scene inputs so the same shared camera contract can drive offline ray generation for both `pinhole32` and `equi62_lut1d`. It covers offline ray generation behavior, offline camera contract alignment with shared camera math, and regression expectations for switching models in the offline path. It does not yet switch the project default to fisheye, add dynamic calibration inputs, or introduce explicit fisheye validity masks.

</domain>

<decisions>
## Implementation Decisions

### Offline fisheye output contract
- **D-01:** Offline fisheye rendering should preserve the full rectangular output image rather than cropping to a circular view or introducing a separate mask contract in this phase.
- **D-02:** Offline `equi62_lut1d` ray generation should follow the existing shared camera math semantics, including the current out-of-range fallback behavior instead of replacing invalid regions with forced background pixels.

### Offline/realtime consistency target
- **D-03:** Phase 2 validation should treat pixel-center outgoing ray agreement with the shared camera math as the primary contract for offline camera support.
- **D-04:** In addition to direction-level checks, Phase 2 should include end-to-end offline regression coverage that shows changing a camera between `pinhole32` and `equi62_lut1d` changes the rendered result through the same shared-scene path.

### Offline camera source of truth
- **D-05:** `CameraSpec` / `PackedCamera` should become the single source of truth for offline camera model selection and intrinsics.
- **D-06:** Existing CPU preset pose fields such as `lookfrom`, `lookat`, and `vup` may remain as offline pose inputs, but they should no longer define a separate long-lived camera model contract independent of shared camera math.
- **D-07:** `pinhole32` offline rendering should preserve the current depth-of-field behavior through `defocus_angle` and `focus_dist`.
- **D-08:** `equi62_lut1d` v1 offline rendering should disable depth-of-field and use zero-distortion defaults rather than inventing a separate fisheye blur model in this phase.

### the agent's Discretion
- The exact internal API shape used to bridge offline ray generation from shared camera math into the existing CPU renderer.
- Whether the offline path adapts the existing `Camera` implementation or introduces a narrowly scoped shared-camera ray generator underneath it.
- The precise split between direction-level parity tests and image-level regression tests, as long as both contracts are represented.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Camera model math
- `docs/reference/src-cam/cam_pinhole32.h` — Reference parameterization and projection behavior for `pinhole32`.
- `docs/reference/src-cam/cam_equi62_lut1d.h` — Reference parameterization and projection behavior for `equi62_lut1d`.

### Shared camera contract
- `.planning/phases/01-shared-camera-schema/01-CONTEXT.md` — Locked decisions from Phase 1 about the canonical shared camera schema and explicit model declarations.

### Offline renderer baseline
- `src/common/camera.h` — Current offline CPU camera implementation and pinhole viewport semantics that Phase 2 must either adapt or bypass.
- `src/core/offline_shared_scene_renderer.cpp` — Current shared-scene offline render entrypoints and pinhole-only camera configuration path.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/realtime/camera_models.cpp`: Already provides `project_*` and `unproject_*` math for both supported camera models, which should anchor offline ray generation instead of re-deriving fisheye behavior from scratch.
- `src/realtime/camera_rig.cpp`: Already packs shared camera model choice, intrinsics, and pose into `PackedCamera`, giving the offline path a canonical authored/runtime camera description to consume.
- `tests/test_camera_models.cpp`: Existing math-level tests can be extended or mirrored to prove offline ray directions match shared camera model behavior.

### Established Patterns
- `src/core/offline_shared_scene_renderer.cpp` currently hard-rejects non-pinhole cameras and derives rays indirectly through `Camera`'s pinhole `vfov/lookfrom/lookat/vup` semantics.
- `src/scene/shared_scene_builders.h` and scene YAML still preserve legacy offline pose-style preset fields (`lookfrom`, `lookat`, `vup`, `defocus_angle`, `focus_dist`) alongside the canonical shared camera spec.
- Current offline regression tests mostly validate preset preservation and smoke rendering, not model-switch behavior or shared-camera direction parity.

### Integration Points
- `render_shared_scene_from_camera(...)` in `src/core/offline_shared_scene_renderer.cpp` is the narrowest existing seam for plugging canonical `PackedCamera`-driven offline rendering into shared scenes.
- `src/common/camera.h` is the main integration risk because it currently assumes pinhole viewport construction from `vfov` and camera-frame basis vectors.
- `tests/test_offline_shared_scene_renderer.cpp` and related shared-scene regression coverage are the natural homes for model-switch and contract checks once the offline path supports fisheye.

</code_context>

<specifics>
## Specific Ideas

- Offline fisheye should remain a rectangular render target in v1 rather than introducing a circular crop or explicit valid-pixel mask.
- The same camera-selection change in scene or rig data should flow through the offline path without needing a separate scene format or separate offline-only camera declaration.
- Pinhole depth of field stays intact, while fisheye v1 should remain a no-defocus model until a later calibration/rendering phase defines something more explicit.

</specifics>

<deferred>
## Deferred Ideas

- Explicit fisheye valid-region masking or cropped circular output is deferred beyond Phase 2.
- Dynamic calibration controls, explicit runtime intrinsics/extrinsics editing, and non-zero distortion tuning remain later-phase work.
- Project-wide default switching to fisheye remains scoped to Phase 4 rather than being pulled forward into the offline-only phase.

</deferred>

---
*Phase: 02-offline-cpu-camera-models*
*Context gathered: 2026-04-20*
