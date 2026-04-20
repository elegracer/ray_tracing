# Phase 3: Realtime GPU And Viewer Camera Models - Context

**Gathered:** 2026-04-20
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase makes the realtime GPU renderer and the viewer path preserve the selected per-camera model from shared scene data through runtime rig creation, packed cameras, OptiX launches, and active viewer cameras. It covers realtime camera-model honoring, viewer default-rig alignment with authored scene presets, and mixed-model multi-camera behavior. It does not yet switch project-wide defaults to fisheye or introduce new calibration controls, masks, or crop semantics.

</domain>

<decisions>
## Implementation Decisions

### Viewer default rig source of truth
- **D-01:** Scene-authored realtime camera presets should become the single source of truth for the viewer default rig's camera model and intrinsics.
- **D-02:** Viewer default rig generation may keep its existing body-pose and yaw-offset behavior, but it should derive each runtime camera from the scene preset rather than from a separate hardcoded pinhole camera definition.

### Mixed-model four-camera behavior
- **D-03:** A four-camera realtime/viewer rig must allow each camera to independently be `pinhole32` or `equi62_lut1d`.
- **D-04:** Phase 3 should preserve the per-camera first-class type contract all the way through mixed-model rigs rather than forcing one model for the whole rig.

### Realtime fisheye output contract
- **D-05:** Realtime `equi62_lut1d` rendering should keep the full rectangular output image and must not introduce a circular crop or explicit valid-pixel mask in this phase.
- **D-06:** Realtime fisheye behavior should follow the same shared-camera fallback semantics already used by the current camera math and Phase 2 offline contract, including out-of-range LUT fallback behavior.

### Realtime/viewer consistency target
- **D-07:** Phase 3 acceptance should primarily verify contract preservation across the chain `scene preset/YAML -> runtime camera spec -> packed rig -> OptiX active camera -> viewer active camera`.
- **D-08:** In addition to contract-level checks, Phase 3 should include end-to-end regressions proving mixed-model realtime/viewer rigs keep the selected model, intrinsics, and pose through execution.

### the agent's Discretion
- The exact internal adapter shape used to replace viewer-side hardcoded pinhole rig construction with scene-derived camera specs.
- Whether mixed-model regression coverage lives mostly in realtime factory tests, viewer rig tests, OptiX tests, or a split across them.
- The exact helper boundaries between realtime scene factory code and viewer rig code, as long as the authored scene preset remains the camera truth.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Camera model math
- `docs/reference/src-cam/cam_pinhole32.h` — Reference parameterization and projection behavior for `pinhole32`.
- `docs/reference/src-cam/cam_equi62_lut1d.h` — Reference parameterization and projection behavior for `equi62_lut1d`.

### Prior locked decisions
- `.planning/phases/01-shared-camera-schema/01-CONTEXT.md` — Canonical shared camera schema and explicit model declaration rules.
- `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md` — Locked rectangular fisheye contract and shared-camera consistency target already established for the offline path.

### Realtime and viewer baseline
- `src/realtime/realtime_scene_factory.cpp` — Current runtime camera-spec scaling and scene-preset rig construction path.
- `src/realtime/camera_rig.h` — Packed realtime camera contract consumed by GPU and viewer tests.
- `src/realtime/gpu/programs.cu` — Current device-side pinhole/equi ray-generation branch.
- `src/realtime/viewer/four_camera_rig.cpp` — Current viewer-side hardcoded pinhole rig construction that must be brought onto the shared camera contract.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/realtime/realtime_scene_factory.cpp`: Already rescales authored `CameraSpec` intrinsics to runtime resolution and builds a `CameraRig` from scene presets, which should anchor the Phase 3 runtime path.
- `src/realtime/gpu/programs.cu`: Already contains device-side `unproject_pinhole32(...)` and `unproject_equi62_lut1d(...)` branches keyed by `CameraModelType`, so GPU math support exists and needs end-to-end wiring rather than reinvention.
- `src/realtime/camera_rig.cpp`: Already packs per-camera model choice, intrinsics, and `Sophus::SE3d` pose into `PackedCamera`, making it the natural preserved contract across realtime and viewer code.

### Established Patterns
- `src/realtime/viewer/four_camera_rig.cpp` currently hardcodes a pinhole-only viewer rig, which is now the main contract mismatch with the shared camera model rollout.
- `tests/test_realtime_scene_factory.cpp` already checks preset-driven spawn pose, resized intrinsics, and default-viewer alignment for pinhole scenes, so it is the natural regression home for shared preset truth.
- `tests/test_viewer_four_camera_rig.cpp` already exercises four-camera pose semantics and is the natural place to extend mixed-model viewer rig coverage.

### Integration Points
- `default_camera_rig_for_scene(...)` in `src/realtime/realtime_scene_factory.cpp` is the narrowest existing seam for preserving authored scene camera model choice into runtime rigs.
- `make_default_viewer_rig(...)` in `src/realtime/viewer/four_camera_rig.cpp` is the primary integration risk because it still constructs pinhole params independently of scene presets.
- `DeviceActiveCamera` packing in `src/realtime/gpu/optix_renderer.cpp` and ray generation in `src/realtime/gpu/programs.cu` are the key realtime execution points that must preserve packed per-camera model data.

</code_context>

<specifics>
## Specific Ideas

- Viewer default rigs should stop being a separate pinhole-only truth and instead reflect the authored realtime preset camera contract.
- Mixed-model rigs are part of the requested contract, not an optional later enhancement.
- CPU and GPU fisheye should keep the same rectangular-output semantics in v1 rather than diverging into different invalid-region behavior.

</specifics>

<deferred>
## Deferred Ideas

- Project-wide default switching to fisheye remains Phase 4 work.
- Dynamic calibration controls, explicit runtime intrinsics/extrinsics editing, and non-zero distortion tuning remain later-phase work.
- Explicit fisheye masks, cropped circular output, or other new validity-region semantics remain out of scope for this phase.

</deferred>

---
*Phase: 03-realtime-gpu-and-viewer-camera-models*
*Context gathered: 2026-04-20*
