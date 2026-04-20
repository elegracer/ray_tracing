# Phase 3: Realtime GPU And Viewer Camera Models - Research

**Researched:** 2026-04-21
**Domain:** Realtime camera-model preservation across authored scene presets, viewer rig construction, packed runtime cameras, and OptiX active-camera launches [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/gpu/programs.cu`, `src/realtime/gpu/optix_renderer.cpp`, `tests/test_realtime_scene_factory.cpp`, `tests/test_viewer_four_camera_rig.cpp`, `tests/test_optix_direction.cpp`, `tests/test_optix_equi_path_trace.cpp`]
**Confidence:** MEDIUM [VERIFIED: relevant code paths and the current regression slice were inspected locally; planning did not change code]

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Scene-authored realtime camera presets are the single source of truth for viewer default-rig model and intrinsics. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-02:** Viewer default rigs may keep current body-pose and yaw-offset behavior, but they must derive runtime cameras from scene-authored specs rather than a hardcoded pinhole camera. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-03:** Four-camera realtime/viewer rigs must allow each camera to independently choose `pinhole32` or `equi62_lut1d`. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-04:** Phase 3 must preserve the per-camera first-class type contract all the way through mixed-model rigs. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-05:** Realtime `equi62_lut1d` keeps full rectangular output; no crop or mask contract is introduced in this phase. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-06:** Realtime fisheye follows the same shared-camera fallback semantics already locked for the offline path. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`, `src/realtime/gpu/programs.cu`]
- **D-07:** Primary acceptance is contract preservation across authored preset/YAML -> runtime camera spec -> packed rig -> OptiX active camera -> viewer active camera. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- **D-08:** Phase 3 must also include end-to-end regressions proving mixed-model realtime/viewer rigs preserve selected model, intrinsics, and pose through execution. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]

### Deferred Ideas (OUT OF SCOPE)
- Project-wide default switching to fisheye remains Phase 4 work. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- Dynamic calibration controls, runtime intrinsics/extrinsics editing, and non-zero distortion tuning remain later-phase work. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
- Explicit fisheye masks or cropped circular output remain out of scope. [VERIFIED: `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CAM-03 | Realtime GPU rendering uses the selected camera model for ray generation instead of assuming pinhole-only behavior. [VERIFIED: `.planning/REQUIREMENTS.md`] | Device-side ray generation is already model-aware in `programs.cu`, but end-to-end regression still fails for equi radiance, so the phase needs to preserve packed camera data through `DeviceActiveCamera` upload and active-camera selection rather than inventing new GPU math. [VERIFIED: `src/realtime/gpu/programs.cu`, `src/realtime/gpu/optix_renderer.cpp`, `tests/test_optix_equi_path_trace.cpp`] |
| CAM-04 | Viewer camera rigs and default viewer scene setup preserve the selected camera model for each active camera. [VERIFIED: `.planning/REQUIREMENTS.md`] | The current viewer helper still hardcodes one pinhole camera and clones it four times, so Phase 3 must replace that helper with a spec-driven builder and add regressions for both authored-scene defaults and mixed-model rigs. [VERIFIED: `src/realtime/viewer/four_camera_rig.cpp`, `tests/test_viewer_four_camera_rig.cpp`, `tests/test_realtime_scene_factory.cpp`] |
</phase_requirements>

## Project Constraints

- Surface assumptions and tradeoffs before implementation; do not silently pick an interpretation. [VERIFIED: user-provided `AGENTS.md`]
- Prefer the minimum change that satisfies the phase; avoid speculative abstractions. [VERIFIED: user-provided `AGENTS.md`]
- Keep changes surgical and directly traceable to realtime/viewer camera-model preservation. [VERIFIED: user-provided `AGENTS.md`]
- Success must be explicit and testable. [VERIFIED: user-provided `AGENTS.md`]

## Summary

The GPU kernel is not the primary gap. `src/realtime/gpu/programs.cu` already branches on `CameraModelType` and contains both `unproject_pinhole32(...)` and `unproject_equi62_lut1d(...)`. `src/realtime/gpu/optix_renderer.cpp` also already copies both `pinhole` and `equi` payloads into `DeviceActiveCamera`. The bigger problem is that the viewer-side default rig still manufactures a pinhole camera independently of authored scene presets, so Phase 3 must bring that path onto the canonical `CameraSpec` contract before the end-to-end GPU/viewer chain can be considered consistent. [VERIFIED: `src/realtime/gpu/programs.cu`, `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/viewer/four_camera_rig.cpp`]

`runtime_camera_spec(...)` in `src/realtime/realtime_scene_factory.cpp` is already the correct rescaling seam for authored camera intrinsics. It copies an authored `CameraSpec`, scales `fx/fy/cx/cy` to the requested runtime resolution, and composes the authored `T_bc` with the runtime body pose. That makes it the right anchor for both scene-driven single-camera realtime rigs and any viewer helper that needs to derive runtime cameras from authored specs without drifting into another intrinsics convention. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`]

The viewer helper is the main contract mismatch. `make_default_viewer_rig(...)` currently hardcodes a local `Pinhole32Params` block and then uses current yaw-offset/body-pose logic to add four pinhole cameras. This means viewer defaults cannot preserve authored equi scenes, and they also cannot express mixed-model rigs even though `CameraRig` and `PackedCameraRig` already can. Phase 3 therefore needs a spec-driven viewer rig builder that preserves the current pose/yaw semantics while deriving model and intrinsics from `CameraSpec` inputs. [VERIFIED: `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/camera_rig.cpp`, `tests/test_camera_rig.cpp`]

The current regression slice confirms that the missing behavior is real and not hypothetical. `test_realtime_scene_factory`, `test_viewer_four_camera_rig`, and `test_optix_direction` pass, but `test_optix_equi_path_trace` currently fails with `expect_true failed: equi radiance should be non-black`. That makes the safest plan sequence: first fix the viewer/runtime contract mismatch, then lock the GPU active-camera contract and equi radiance path, and only after that add broader mixed-model/file-backed regressions. [VERIFIED: local run `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace)$' --output-on-failure` on 2026-04-21]

**Primary recommendation:** Plan Phase 3 in three steps: (1) replace the viewer's hardcoded pinhole rig with a spec-driven builder aligned to authored realtime presets, (2) fix and regress the OptiX active-camera contract so equi and mixed-model rigs render through the existing GPU math, and (3) close with file-backed and four-camera regressions that prove the realtime/viewer chain preserves model, intrinsics, and pose end to end. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/gpu/programs.cu`, `tests/test_optix_equi_path_trace.cpp`]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Authored camera rescaling to runtime resolution | `src/realtime/realtime_scene_factory.cpp` | `scene::CameraSpec` | `runtime_camera_spec(...)` already defines the canonical scale-from-authored-calibration rule and should stay the single truth for runtime intrinsics. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`] |
| Viewer rig construction from pose + camera spec | `src/realtime/viewer/four_camera_rig.cpp` | `src/realtime/camera_rig.cpp` | Viewer logic owns yaw offsets and frame-convention pose math, while `CameraRig::add_camera(...)` already knows how to materialize either supported model from `CameraSpec`. [VERIFIED: `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/camera_rig.cpp`] |
| GPU active-camera upload | `src/realtime/gpu/optix_renderer.cpp` | `src/realtime/gpu/launch_params.h` | Device upload is where packed runtime camera data becomes the OptiX launch contract. [VERIFIED: `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/launch_params.h`] |
| Device-side ray generation | `src/realtime/gpu/programs.cu` | `CameraModelType` / `DeviceActiveCamera` | The model branch already exists here; Phase 3 should preserve and prove it rather than replace it. [VERIFIED: `src/realtime/gpu/programs.cu`] |
| File-backed realtime preset preservation | `tests/test_realtime_scene_factory.cpp` / `tests/test_viewer_scene_reload.cpp` | `src/scene/yaml_scene_loader.cpp` | Temp YAML scene roots and reload coverage are the established pattern for proving file-backed scene behavior without mutating repo fixtures. [VERIFIED: `tests/test_viewer_scene_reload.cpp`, `src/scene/yaml_scene_loader.cpp`] |

## Standard Stack

### Core

| Item | Role | Why Standard |
|------|------|--------------|
| `scene::CameraSpec` | Canonical authored camera contract. [VERIFIED: `src/scene/camera_spec.h`] | It already carries `model`, size, intrinsics, `T_bc`, and both model parameter slots; Phase 3 should consume it directly. |
| `CameraRig::add_camera(...)` | Runtime rig materialization from canonical camera data. [VERIFIED: `src/realtime/camera_rig.cpp`] | It already converts `CameraSpec` to `PackedCamera` for both supported models. |
| `runtime_camera_spec(...)` | Canonical authored-to-runtime resize rule. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`] | It prevents viewer/factory code from inventing a second intrinsics scaling convention. |
| `DeviceActiveCamera` + `programs.cu` | GPU launch contract and model-aware ray generation. [VERIFIED: `src/realtime/gpu/launch_params.h`, `src/realtime/gpu/programs.cu`] | They already model the right behavior; the missing work is preserving that behavior end to end. |

### Supporting

| Item | Role | When to Use |
|------|------|-------------|
| `tests/test_realtime_scene_factory.cpp` | Scene-preset/runtime-rig regression home. [VERIFIED: `tests/test_realtime_scene_factory.cpp`] | Use it for authored preset preservation, resize scaling, and file-backed realtime preset coverage. |
| `tests/test_viewer_four_camera_rig.cpp` | Viewer rig pose/model regression home. [VERIFIED: `tests/test_viewer_four_camera_rig.cpp`] | Use it for spec-driven viewer rig construction and mixed-model four-camera expectations. |
| `tests/test_optix_direction.cpp` | Lightweight GPU direction smoke test. [VERIFIED: `tests/test_optix_direction.cpp`] | Use it for model-aware active-camera upload smoke without full path tracing cost. |
| `tests/test_optix_equi_path_trace.cpp` | Existing GPU equi radiance regression. [VERIFIED: `tests/test_optix_equi_path_trace.cpp`] | Use it as the main evidence that the realtime equi path is actually producing image content rather than just compiling. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Derive viewer runtime cameras from `CameraSpec` | Keep the current hardcoded viewer pinhole helper and only fix scene factory | That would leave CAM-04 unsatisfied because the viewer would still have its own camera truth. |
| Preserve the existing GPU camera upload path and test it harder | Rebuild GPU camera structs directly from scene YAML or viewer inputs | That would bypass `PackedCamera` and create another camera contract instead of preserving the existing one. |
| Use temp file-backed scenes for equi coverage | Mutate repo-owned scene YAML to make one built-in scene fisheye during Phase 3 | That would conflate Phase 3 behavior preservation with Phase 4 default switching and make regressions harder to reason about. |

## Architecture Patterns

### Pattern 1: Canonical authored spec -> runtime spec -> rig

Use `CameraSpec` as the only authored camera contract, rescale it with `runtime_camera_spec(...)`, then materialize the runtime camera via `CameraRig::add_camera(...)`. This is already the pattern for scene-driven realtime rigs and should become the pattern for viewer default rigs too. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/realtime/camera_rig.cpp`]

### Pattern 2: Viewer-specific pose logic, shared camera payload

Keep viewer-specific concerns limited to body pose, yaw offsets, and frame-convention math. Do not let viewer code manufacture new camera intrinsics or hardcode pinhole params. The reusable camera payload should come from `CameraSpec`. [VERIFIED: `src/realtime/viewer/four_camera_rig.cpp`, `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md`]

### Pattern 3: PackedCamera is the GPU truth

Once a rig is packed, `PackedCamera` should be the only contract the GPU side sees. Device upload and kernel ray generation should consume `PackedCamera.model`, `PackedCamera.pinhole`, `PackedCamera.equi`, and `PackedCamera.T_rc` as-is rather than reconstructing model behavior from external scene or viewer state. [VERIFIED: `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/programs.cu`]

### Pattern 4: Temp YAML scene roots for file-backed realtime coverage

When Phase 3 needs a realtime equi preset in tests, create a temp `scene.yaml`, scan or reload it through the existing scene catalog path, and assert the factory/viewer chain preserves the authored camera payload. This is already the established brownfield testing pattern for file-backed scenes. [VERIFIED: `tests/test_viewer_scene_reload.cpp`, `tests/test_scene_file_catalog.cpp`]

---
## RESEARCH COMPLETE
