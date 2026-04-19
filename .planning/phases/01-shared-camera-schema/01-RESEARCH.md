# Phase 1: Shared Camera Schema - Research

**Researched:** 2026-04-19
**Domain:** Shared scene camera schema, YAML/builtin scene migration, and camera adapter boundaries in the local C++ renderer [VERIFIED: `src/scene/scene_definition.h`, `src/scene/yaml_scene_loader.cpp`, `src/realtime/camera_rig.h`, `src/core/offline_shared_scene_renderer.cpp`]
**Confidence:** MEDIUM [VERIFIED: repo-local code paths and targeted regression tests were inspected and the relevant baseline test slice passed via `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure`]

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Introduce a single canonical shared camera description for the project, centered on a `CameraSpec`-style schema rather than separate CPU and realtime camera config formats. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-02:** The canonical schema must carry `model`, `width`, `height`, `fx`, `fy`, `cx`, `cy`, `T_bc`, and model-specific parameter slots. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-03:** Offline, realtime, viewer, and scene-loading code should adapt from this shared schema instead of maintaining independent long-lived camera representations. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-04:** Every camera declaration in builtin scenes and YAML-backed scene files must explicitly specify `model`. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-05:** The phase must not rely on implicit camera-model defaults inside scene files, because the project default will later switch to fisheye and silent fallback would make scene behavior ambiguous. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-06:** Phase 1 schema should define stable slots for both models' distortion parameters and for body-relative `T_bc`, even though v1 will use zero distortion defaults. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-07:** The shared schema should be designed once now rather than grown incrementally with another schema break in later phases. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-08:** Repo-owned builtin scene data and YAML-backed scene files should be migrated to the new schema in this phase. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- **D-09:** Old camera declaration formats are not preserved as a supported compatibility path for repo-owned content; after migration, the new schema becomes the only project-owned representation. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

### Claude's Discretion
- Field naming, helper constructors, and exact type layout for the canonical camera schema. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- Whether compatibility shims exist temporarily inside implementation code while the new schema is wired through. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- Exact division of structs between `scene/` and `realtime/` headers as long as the canonical shared schema remains the single source of truth. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

### Deferred Ideas (OUT OF SCOPE)
- Dynamic render-preset-controlled resolution selection belongs to a later calibration-focused phase. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- Dynamic explicit intrinsics, model-specific distortion tuning, and body-relative extrinsic configuration belong to later phases even though the schema should pre-allocate their slots now. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CAM-01 | Developer can represent each camera in a rig as either `pinhole32` or `equi62_lut1d`. [VERIFIED: `.planning/REQUIREMENTS.md`] | The existing runtime side already stores per-camera model choice in `rt::CameraModelType`, `CameraRig::Slot`, and `PackedCamera`, so Phase 1 should standardize a scene-level `CameraSpec` that converts one-to-one into those runtime forms instead of inventing another enum or parameter shape. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`] |
| CAM-05 | Both supported camera models accept `fx`, `fy`, `cx`, and `cy`, with v1 distortion coefficients defaulting to zero. [VERIFIED: `.planning/REQUIREMENTS.md`] | Both runtime parameter structs already expose the four common intrinsics plus model-specific distortion slots, and both current reference-backed implementations accept zero distortion in their authored inputs. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp`, `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`] |
| DEF-01 | Builtin scene definitions and YAML-backed scene configs can declare the intended camera model for each camera. [VERIFIED: `.planning/REQUIREMENTS.md`] | Current builtin scene helpers and YAML scenes still declare cameras through `vfov`-era fields only, so Phase 1 must migrate both `src/scene/shared_scene_builders.cpp` and `assets/scenes/*/scene.yaml` to an explicit `model` field and shared intrinsics payload, then teach `yaml_scene_loader.cpp` to require it. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/shared_scene_builders.cpp`, `src/scene/yaml_scene_loader.cpp`, `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`] |
</phase_requirements>

## Project Constraints

- Surface assumptions and ambiguity before implementation instead of silently picking an interpretation. [VERIFIED: user-provided `AGENTS.md` content]
- Prefer the minimum schema and adapter changes that satisfy the phase; avoid speculative abstraction layers. [VERIFIED: user-provided `AGENTS.md` content]
- Keep edits surgical and limited to lines that directly support the shared-camera-schema migration. [VERIFIED: user-provided `AGENTS.md` content]
- Define success in terms of verifiable tests and preserved behavior, not "schema looks cleaner". [VERIFIED: user-provided `AGENTS.md` content]

## Summary

The repo already has model-aware runtime camera math and packing types, but the authored scene surface is still pinhole-era and split by backend. `rt::CameraModelType`, `rt::Pinhole32Params`, `rt::Equi62Lut1DParams`, `CameraRig::Slot`, `PackedCamera`, GPU launch params, and OptiX raygen already branch on camera model, while `CpuCameraPreset`, `RealtimeViewPreset`, builtin scene helpers, and YAML parsing still speak in `vfov`, `lookfrom/lookat`, and `use_default_viewer_intrinsics` terms. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`, `src/realtime/gpu/programs.cu`, `src/scene/shared_scene_builders.h`, `src/scene/shared_scene_builders.cpp`, `src/scene/yaml_scene_loader.cpp`]

The safest Phase 1 plan is to introduce a scene-level `CameraSpec` as the only authored camera schema, keep runtime math structs as adapter targets, and migrate every repo-owned builtin/YAML camera declaration to explicit `model` plus common intrinsics immediately. That avoids leaking renderer-space `PackedCamera` details into scene files, avoids keeping duplicate CPU/realtime camera schemas alive, and preserves later phases' freedom to change offline or realtime ray generation independently. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/realtime/camera_rig.h`, `src/realtime/camera_models.cpp`, `src/core/offline_shared_scene_renderer.cpp`]

Current baseline behavior is stable enough to preserve structurally while Phase 1 lands. The relevant scene/schema regression slice passed unchanged, including YAML loading, scene catalog overlay, camera rig packing, offline shared-scene rendering smoke coverage, and realtime scene factory coverage. [VERIFIED: `tests/test_camera_rig.cpp`, `tests/test_scene_definition.cpp`, `tests/test_yaml_scene_loader.cpp`, `tests/test_scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_realtime_scene_factory.cpp`, `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure`]

**Primary recommendation:** Add a new `scene::CameraSpec` in `src/scene/`, embed it into both preset types, keep `PackedCamera` and GPU structs as derived runtime formats only, and migrate every repo-owned camera declaration to explicit `model` with no implicit fallback. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/shared_scene_builders.h`, `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Authored camera schema and YAML shape | Scene/schema layer | Builtin/YAML loader layer | `SceneDefinition`, `shared_scene_builders`, and `yaml_scene_loader` are where camera data is authored today, so Phase 1 should land the canonical schema there first. [VERIFIED: `src/scene/scene_definition.h`, `src/scene/shared_scene_builders.h`, `src/scene/shared_scene_builders.cpp`, `src/scene/yaml_scene_loader.cpp`] |
| Conversion from authored schema to runtime rig slots | Adapter layer | Realtime scene factory | `CameraRig::Slot` stores `model`, `T_bc`, `width`, `height`, and a `variant` of the two parameter structs, which is the closest existing runtime boundary to a canonical authored `CameraSpec`. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`] |
| Packed renderer format | Runtime packing layer | GPU device layer | `PackedCamera` stores renderer-space `T_rc` and both concrete runtime parameter structs, and GPU code copies that into `DeviceActiveCamera`, so it should stay derived rather than become the authored schema. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/launch_params.h`] |
| Offline ray-generation behavior | Offline renderer layer | Shared-scene adapter layer | `offline_shared_scene_renderer.cpp` still rejects non-pinhole packed cameras and reconstructs `Camera` from pinhole-specific assumptions, so Phase 1 should preserve that boundary and avoid bundling offline raygen changes into the schema migration. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] |
| Realtime scene startup behavior | Realtime factory/viewer layer | Scene preset layer | `default_camera_rig_for_scene` and `viewer::make_default_viewer_rig` still manufacture pinhole rigs from preset-specific `vfov` or hard-coded intrinsics, so they are the first downstream consumers that must adapt from `CameraSpec` without changing GPU ray math yet. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/realtime/viewer/four_camera_rig.cpp`] |
| Repo-owned content migration | Content layer | Scene catalog overlay layer | Builtin scene definitions come from `src/scene/shared_scene_builders.cpp`, and YAML scenes overlay builtin records by `scene_id` in `SceneFileCatalog`, so Phase 1 must migrate both surfaces together. [VERIFIED: `src/scene/shared_scene_builders.cpp`, `src/scene/scene_file_catalog.cpp`, `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`] |

## Standard Stack

### Core

| Item | Role | Why Standard |
|------|------|--------------|
| `scene::CameraSpec` (new recommended repo-local type) | Single authored camera schema shared by builtin scenes, YAML scenes, CPU presets, and realtime presets. [VERIFIED: recommendation synthesized from `src/scene/shared_scene_builders.h`, `src/scene/yaml_scene_loader.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | The current split between `CpuCameraPreset` and `RealtimeViewPreset` is exactly where pinhole assumptions still live, so Phase 1 needs one schema above both of them. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/yaml_scene_loader.cpp`] |
| `rt::CameraModelType` | Canonical model enum for authored and runtime camera selection. [VERIFIED: `src/realtime/camera_models.h`] | The enum is already used by `CameraRig`, `PackedCamera`, GPU launch params, and OptiX raygen branching, so reusing it prevents duplicate model taxonomies. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`, `src/realtime/gpu/programs.cu`] |
| `rt::Pinhole32Params` and `rt::Equi62Lut1DParams` | Concrete runtime math targets that authored schema should convert into. [VERIFIED: `src/realtime/camera_models.h`] | The repo's tested projection/unprojection behavior and LUT generation already anchor on these types and on the reference headers in `docs/reference/src-cam/`. [VERIFIED: `src/realtime/camera_models.cpp`, `tests/test_camera_models.cpp`, `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`] |

### Supporting

| Item | Role | When to Use |
|------|------|-------------|
| `CameraRig::Slot` | Intermediate runtime-owned representation of one camera before packing to `PackedCamera`. [VERIFIED: `src/realtime/camera_rig.h`] | Use it as the primary adapter target from `CameraSpec` because it still stores `T_bc` instead of the derived renderer transform. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`] |
| `make_equi62_lut1d_params(...)` | Generates the LUT-backed fisheye runtime params from common intrinsics plus distortion. [VERIFIED: `src/realtime/camera_models.cpp`] | Use it whenever a `CameraSpec` with `model == equi62_lut1d` becomes runtime data; do not serialize or author LUT entries directly. [VERIFIED: `src/realtime/camera_models.cpp`] |
| `SceneFileCatalog` | Single overlay point that merges builtin scene definitions with YAML scene files by `scene_id`. [VERIFIED: `src/scene/scene_file_catalog.cpp`] | Use it to validate that migrated YAML files and builtin scene definitions resolve to the same `SceneDefinition` shape after the schema change. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`] |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `scene::CameraSpec` | Reuse `PackedCamera` directly as the authored schema. [VERIFIED: `src/realtime/camera_rig.h`] | `PackedCamera` is the wrong abstraction boundary because it stores derived renderer-space `T_rc` and carries the full `Equi62Lut1DParams` LUT payload that should remain runtime-generated. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_models.cpp`] |
| Shared camera block inside both presets | Keep `CpuCameraPreset` and `RealtimeViewPreset` as unrelated long-lived schemas. [VERIFIED: `src/scene/shared_scene_builders.h`] | That would preserve the current duplication of `vfov`-based pinhole assumptions and directly contradict D-01 and D-03. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/shared_scene_builders.h`] |
| Authored common intrinsics + distortion slots | Store full `Equi62Lut1DParams` in scene/YAML content. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp`] | Full `Equi62Lut1DParams` includes derived LUT state and duplicates top-level width/height/intrinsics, which would make scene data larger and easier to desynchronize. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp`] |

**Installation:** None. Phase 1 can stay inside the existing `core`/`realtime_gpu` targets and test executables. [VERIFIED: `CMakeLists.txt`]

**Version verification:** N/A for the recommended schema pieces because they are repo-local C++ types, not external packages. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`, `src/scene/shared_scene_builders.h`]

## Architecture Patterns

### System Architecture Diagram

```text
repo-owned builtin scene tables         repo-owned YAML scene files
`src/scene/shared_scene_builders.cpp`   `assets/scenes/*/scene.yaml`
                |                                  |
                v                                  v
        scene::CameraSpec creation  <----  yaml_scene_loader parses explicit `model`
                |                                  |
                +---------------> `SceneDefinition`  <---------------+
                                   (single authored truth)            |
                                                                      |
                                          `SceneFileCatalog` overlays builtin/file-backed scenes
                                                                      |
                                  +-----------------------------------+----------------------------------+
                                  |                                                                      |
                                  v                                                                      v
                    CPU preset adapter / compatibility shim                                  Realtime preset adapter / rig builder
                    keeps current pose/framing fields for v1                                  keeps current spawn/move fields for v1
                                  |                                                                      |
                                  v                                                                      v
                      `Pinhole32Params` for current CPU path                         `CameraRig::Slot` -> `PackedCamera` -> GPU device camera
                                  |                                                                      |
                                  v                                                                      v
                `offline_shared_scene_renderer.cpp` keeps current                         OptiX raygen already branches by `CameraModelType`
                 pinhole-only behavior until Phase 2                                      without further schema changes
```

The critical boundary is that authored camera data should stop at `CameraSpec`, while `CameraRig::Slot`, `PackedCamera`, and GPU device structs stay derived runtime formats. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`]

### Recommended Project Structure

```text
src/
├── scene/
│   ├── camera_spec.h        # canonical authored camera schema
│   ├── scene_definition.h   # SceneDefinition owns presets that embed CameraSpec
│   ├── yaml_scene_loader.cpp# YAML <-> CameraSpec parsing and validation
│   └── shared_scene_builders.cpp # builtin scene migration to CameraSpec
├── realtime/
│   ├── camera_models.h      # runtime math targets and conversion helpers
│   ├── camera_rig.h         # CameraSpec -> Slot/PackedCamera adapter boundary
│   └── realtime_scene_factory.cpp # preset CameraSpec -> default runtime rig
└── core/
    └── offline_shared_scene_renderer.cpp # temporary pinhole-only compatibility shim
```

This keeps the new schema in `src/scene/` because that is where authored scene data already lives, and it keeps runtime-only transforms/LUTs out of the schema header. [VERIFIED: `src/scene/scene_definition.h`, `src/scene/yaml_scene_loader.cpp`, `src/realtime/camera_rig.h`, `src/realtime/camera_models.cpp`]

### Pattern 1: Scene-Level `CameraSpec` With Runtime Conversion Helpers

**What:** Introduce one authored camera type that stores common intrinsics once, keeps `T_bc` in body space, and gives each supported model its own distortion slot. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/realtime/camera_rig.h`, `src/realtime/camera_models.h`]

**When to use:** Use it anywhere the repo currently stores long-lived camera configuration in scene data, preset data, or YAML. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/yaml_scene_loader.cpp`]

**Example:** [VERIFIED: synthesized from `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

```cpp
// Source: synthesized from `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`,
// and Phase 1 locked decisions in `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`.
namespace rt::scene {

struct CameraSpec {
    CameraModelType model = CameraModelType::pinhole32;
    int width = 0;
    int height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();

    struct Pinhole32Slot {
        double k1 = 0.0;
        double k2 = 0.0;
        double k3 = 0.0;
        double p1 = 0.0;
        double p2 = 0.0;
    } pinhole {};

    struct Equi62Lut1DSlot {
        std::array<double, 6> radial {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        Eigen::Vector2d tangential = Eigen::Vector2d::Zero();
    } equi {};
};

Pinhole32Params to_pinhole32_params(const CameraSpec& spec);
Equi62Lut1DParams to_equi62_lut1d_params(const CameraSpec& spec);

}  // namespace rt::scene
```

### Pattern 2: Preserve Backend-Specific Outer Presets, Replace Their Camera Payload

**What:** Keep `CpuCameraPreset` and `RealtimeViewPreset` for the fields that are truly backend-specific in Phase 1, but replace their camera-specific content with `CameraSpec`. [VERIFIED: `src/scene/shared_scene_builders.h`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

**When to use:** Use this pattern because Phase 1 is explicitly not changing offline CPU or realtime GPU ray generation behavior yet. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/realtime_scene_factory.cpp`]

**Example:** [VERIFIED: synthesized from `src/scene/shared_scene_builders.h`, `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/realtime_scene_factory.cpp`]

```cpp
// Source: synthesized from existing preset ownership in `src/scene/shared_scene_builders.h`.
struct CpuCameraPreset {
    double aspect_ratio = 16.0 / 9.0;
    int image_width = 1280;
    int max_depth = 50;
    Eigen::Vector3d lookfrom = Eigen::Vector3d::Zero();
    Eigen::Vector3d lookat = Eigen::Vector3d::Zero();
    Eigen::Vector3d vup = Eigen::Vector3d::UnitY();
    double defocus_angle = 0.0;
    double focus_dist = 10.0;
    CameraSpec camera {};
};

struct RealtimeViewPreset {
    viewer::BodyPose initial_body_pose {};
    viewer::ViewerFrameConvention frame_convention = viewer::ViewerFrameConvention::world_z_up;
    CameraSpec camera {};
    double base_move_speed = 1.8;
};
```

### Pattern 3: Explicit `model` In Every Repo-Owned YAML Camera Block

**What:** Migrate file-backed scene cameras from `vfov`-only declarations to explicit model declarations with common intrinsics. [VERIFIED: `src/scene/yaml_scene_loader.cpp`, `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`]

**When to use:** Use this for every repo-owned YAML scene in this phase because the locked decision is to remove implicit camera-model defaults immediately. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

**Example:** [VERIFIED: synthesized from existing YAML structure in `assets/scenes/*/scene.yaml` and Phase 1 decisions]

```yaml
cpu_presets:
  default:
    samples_per_pixel: 500
    camera:
      model: pinhole32
      width: 1280
      height: 720
      fx: 1758.2028
      fy: 1758.2028
      cx: 640.0
      cy: 360.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
      pinhole32:
        k1: 0.0
        k2: 0.0
        k3: 0.0
        p1: 0.0
        p2: 0.0
      lookfrom: [13.0, 2.0, 3.0]
      lookat: [0.0, 0.0, 0.0]
      vup: [0.0, 1.0, 0.0]
      defocus_angle: 0.0
      focus_dist: 10.0
```

### Anti-Patterns to Avoid

- **Using `PackedCamera` as the schema:** `PackedCamera` stores `T_rc`, not `T_bc`, and includes model payloads that are already adapted for runtime consumption. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`]
- **Serializing fisheye LUTs into scene files:** `Equi62Lut1DParams::lut` and `lut_step` are derived by `make_equi62_lut1d_params(...)`, so storing them in YAML or builtin tables would duplicate derived state. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_models.cpp`]
- **Keeping `use_default_viewer_intrinsics` alongside explicit intrinsics:** once `CameraSpec` owns `fx/fy/cx/cy`, the boolean becomes an obsolete second source of truth. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/realtime/realtime_scene_factory.cpp`] 
- **Letting `model` remain optional in YAML:** current loader defaults by omission because it only parses legacy fields; Phase 1 must turn omission into a validation error for project-owned content. [VERIFIED: `src/scene/yaml_scene_loader.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] 

## Migration Strategy

1. Add `scene::CameraSpec` and conversion helpers first, without changing runtime render math. That isolates the schema migration from later phases that touch offline/realtime ray generation. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/gpu/programs.cu`]
2. Change `CpuCameraPreset` and `RealtimeViewPreset` so their camera payload comes from `CameraSpec`, but keep their current outer pose/sampling fields for v1 compatibility. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/realtime/realtime_scene_factory.cpp`, `src/core/offline_shared_scene_renderer.cpp`]
3. Migrate builtin scene tables in `src/scene/shared_scene_builders.cpp` by replacing `make_cpu_camera(vfov, lookfrom, lookat, ...)` and `make_realtime_view_preset(...)` camera intrinsics with explicit `CameraSpec` constructors. [VERIFIED: `src/scene/shared_scene_builders.cpp`] 
4. Migrate all repo-owned YAML camera blocks under `assets/scenes/` to explicit `model`, `width`, `height`, `fx`, `fy`, `cx`, `cy`, and zeroed model-specific parameters. [VERIFIED: `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`] 
5. Update `yaml_scene_loader.cpp` to require `model` and parse both supported model payloads, then remove any project-owned fallback that assumes pinhole by omission. [VERIFIED: `src/scene/yaml_scene_loader.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] 
6. Keep temporary compatibility shims only inside implementation code where current consumers still want legacy pose or sizing fields. Do not preserve the old YAML or builtin declaration format as a supported repo-owned format. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] 

The repo has four versioned YAML scene files today and a builtin registry in `src/scene/shared_scene_builders.cpp`, so the migration surface is finite and fully repo-owned. `SceneFileCatalog` replaces builtin records with YAML records when `scene_id` matches, which means YAML overlays must be migrated in the same phase or they will mask migrated builtin camera data. [VERIFIED: `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`, `src/scene/scene_file_catalog.cpp`] 

## Invariants To Preserve

- Existing canonical scene ids and scene overlay behavior must stay unchanged while camera schema changes land. `SceneFileCatalog` intentionally overlays builtin records with file-backed scenes by matching `metadata.id`, and current regressions depend on that. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`] 
- The mixed-model runtime rig contract must stay intact. `test_camera_rig` already proves that one rig can pack both `pinhole32` and `equi62_lut1d` cameras, and Phase 1 should preserve that as the adapter target. [VERIFIED: `tests/test_camera_rig.cpp`] 
- Offline shared-scene rendering must keep current structural behavior for existing pinhole presets. The offline path still validates centered, distortion-free pinhole cameras and throws on non-pinhole packed cameras. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `tests/test_offline_shared_scene_renderer.cpp`] 
- Realtime GPU raygen must remain untouched in this phase. Device code already switches by `CameraModelType`, so the schema migration should feed it better data rather than editing projection math now. [VERIFIED: `src/realtime/gpu/programs.cu`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] 
- v1 distortion defaults must remain zero for repo-owned migrated content, because later calibration phases are explicitly out of scope. [VERIFIED: `.planning/REQUIREMENTS.md`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] 
- Authored transforms should stay body-relative as `T_bc`; only `CameraRig::pack()` should derive renderer-space `T_rc`. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/realtime/camera_rig.cpp`] 

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Authored camera storage | A scene schema built on `PackedCamera` or GPU `DeviceActiveCamera`. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`] | A scene-level `CameraSpec` with runtime conversion helpers. [VERIFIED: recommendation synthesized from `src/realtime/camera_rig.h`, `src/scene/shared_scene_builders.h`] | Runtime structs carry derived transforms and device-friendly copies that should not become authoring truth. [VERIFIED: `src/realtime/camera_rig.cpp`, `src/realtime/gpu/optix_renderer.cpp`] |
| Fisheye LUT authoring | Manual YAML fields or custom LUT generation code in scene loaders. [VERIFIED: `src/realtime/camera_models.cpp`] | `make_equi62_lut1d_params(...)`. [VERIFIED: `src/realtime/camera_models.cpp`] | The helper already derives `lut` and `lut_step` from common intrinsics plus distortion and is the tested path in the repo. [VERIFIED: `src/realtime/camera_models.cpp`, `tests/test_camera_models.cpp`] |
| YAML camera defaults | Implicit missing-`model` fallback to pinhole. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/yaml_scene_loader.cpp`] | Explicit `model` required in every repo-owned camera block. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | Silent fallback becomes ambiguous as soon as project defaults switch to fisheye in a later phase. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `.planning/REQUIREMENTS.md`] |
| Parallel preset schemas | Separate long-lived CPU and realtime camera formats that each own their own model/intrinsics rules. [VERIFIED: `src/scene/shared_scene_builders.h`] | One shared camera payload embedded in both preset wrappers. [VERIFIED: recommendation synthesized from `src/scene/shared_scene_builders.h`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | That is the smallest change that removes duplication now without forcing Phase 2 or Phase 3 runtime rewrites into this phase. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/realtime_scene_factory.cpp`] |

**Key insight:** The scene/schema layer should own common intrinsics and explicit model choice, while all runtime-packed transforms and LUT-heavy payloads remain derived adapter products. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/realtime/camera_rig.h`, `src/realtime/camera_models.cpp`]

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None outside repo-owned source files. Camera declarations currently live in versioned YAML scene files under `assets/scenes/` and builtin C++ tables in `src/scene/shared_scene_builders.cpp`, and no SQLite/DB files were found under the repo root. [VERIFIED: `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, `assets/scenes/simple_light/scene.yaml`, `src/scene/shared_scene_builders.cpp`, `find . -maxdepth 3 \( -name '*.db' -o -name '*.sqlite' -o -name '*.sqlite3' -o -name '*.json' -o -name '*.yaml' -o -name '*.yml' \)`] | Code edit only for versioned builtin/YAML content; no external data migration step is required. [VERIFIED: repo scan above] |
| Live service config | None. No deployment/service descriptors were found under the repo root. [VERIFIED: `find . -maxdepth 3 \( -name '*.service' -o -name '*.plist' -o -name 'ecosystem.config*' -o -name '*.timer' \)`] | None. [VERIFIED: repo scan above] |
| OS-registered state | None found in the repo. This project does not register scene schemas with system services as part of the checked-in workflow. [VERIFIED: `find . -maxdepth 3 \( -name '*.service' -o -name '*.plist' -o -name 'ecosystem.config*' -o -name '*.timer' \)`] | None. [VERIFIED: repo scan above] |
| Secrets/env vars | No camera-schema-related secrets or env-var names were found. The only env usage surfaced in this scan is the vcpkg toolchain path in `CMakeLists.txt`. [VERIFIED: `CMakeLists.txt`, `rg -n "DATABASE_URL|POSTGRES|MYSQL|SQLITE|REDIS|MONGO|dotenv|getenv\\(|std::getenv|ENV\\{|secret|token|api_key|password|SOPS|CI" . -g '!build' -g '!NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64'`] | None for the phase schema migration. [VERIFIED: repo scan above] |
| Build artifacts | `build/CMakeCache.txt`, `build/libcore.a`, `build/librealtime_gpu.a`, and `build/compile_commands.json` already exist and will become stale after header/schema changes. [VERIFIED: `find build -maxdepth 2 \( -name '*.o' -o -name '*.a' -o -name 'CMakeCache.txt' -o -name 'compile_commands.json' \)`] | Rebuild and rerun the relevant CTest slice after implementation; no manual artifact migration is needed beyond recompilation. [VERIFIED: build layout and existing CTest setup in `build/CTestTestfile.cmake`] |

## Common Pitfalls

### Pitfall 1: Treating `PackedCamera` As The Canonical Schema

**What goes wrong:** Scene data starts storing renderer-space `T_rc` and full runtime payloads instead of authored `T_bc` and minimal model parameters. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`]  
**Why it happens:** `PackedCamera` already looks "complete", so it is tempting to reuse it instead of introducing a scene-level type. [VERIFIED: `src/realtime/camera_rig.h`]  
**How to avoid:** Keep `CameraSpec` in `src/scene/` and only convert to `CameraRig::Slot`/`PackedCamera` at adapter boundaries. [VERIFIED: recommendation synthesized from `src/realtime/camera_rig.h`, `src/scene/scene_definition.h`]  
**Warning signs:** YAML or builtin scene code starts talking about `T_rc`, `lut_step`, or raw LUT arrays. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_models.h`]  

### Pitfall 2: Duplicating Common Intrinsics In Multiple Places

**What goes wrong:** `width`, `height`, `fx`, `fy`, `cx`, and `cy` drift between top-level schema fields and model payloads. [VERIFIED: risk derived from `src/realtime/camera_models.h`, where `Equi62Lut1DParams` already duplicates width/height/intrinsics]  
**Why it happens:** Existing runtime structs already embed common intrinsics, so a naive schema copy can duplicate them accidentally. [VERIFIED: `src/realtime/camera_models.h`]  
**How to avoid:** Make common intrinsics top-level `CameraSpec` fields and treat runtime parameter structs as conversion outputs, not authored storage. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/realtime/camera_models.h`]  
**Warning signs:** New tests need to assert the same numeric value in two authored places before conversion happens. [VERIFIED: recommendation synthesized from the schema design risk above]  

### Pitfall 3: Forgetting That YAML Overlays Builtin Scene Definitions

**What goes wrong:** Builtin camera schema is migrated, but a same-id YAML scene still overlays it and keeps the old camera declaration path alive. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`]  
**Why it happens:** `SceneFileCatalog::scan_directory()` starts from builtin records and then replaces builtin entries with file-backed ones when ids match. [VERIFIED: `src/scene/scene_file_catalog.cpp`]  
**How to avoid:** Migrate builtin tables and repo-owned YAML scenes in the same phase, then verify the overlay with `test_shared_scene_regression` and `test_scene_file_catalog`. [VERIFIED: `tests/test_shared_scene_regression.cpp`, `tests/test_scene_file_catalog.cpp`]  
**Warning signs:** A migrated builtin scene still presents legacy camera fields after catalog load or reload. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_viewer_scene_reload.cpp`]  

### Pitfall 4: Smuggling Runtime Behavior Changes Into Phase 1

**What goes wrong:** Offline or realtime render behavior changes are bundled into the schema migration and make the phase harder to verify. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]  
**Why it happens:** `default_camera_rig_for_scene` and `offline_shared_scene_renderer` currently derive camera behavior from legacy fields, so it is tempting to "finish the job" in one pass. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/core/offline_shared_scene_renderer.cpp`]  
**How to avoid:** Land the shared schema and data migration first, then keep compatibility shims in the adapters until later phases change ray generation. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]  
**Warning signs:** The plan starts editing GPU projection code or removing the offline pinhole validation branch in this phase. [VERIFIED: `src/realtime/gpu/programs.cu`, `src/core/offline_shared_scene_renderer.cpp`]  

### Pitfall 5: Leaving Realtime Resolution Semantics Ambiguous

**What goes wrong:** `CameraSpec` stores width/height, but `default_camera_rig_for_scene(scene_id, camera_count, width, height)` still takes requested output size, so intrinsics silently mismatch the actual render target. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]  
**Why it happens:** Current realtime presets do not own width/height at all; the factory derives pinhole intrinsics from caller-supplied dimensions. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/realtime/realtime_scene_factory.cpp`]  
**How to avoid:** Decide explicitly whether Phase 1 stores authored reference resolution and rescales intrinsics on demand, or whether it pins runtime default rig generation to schema width/height and treats caller dimensions as output-only. [VERIFIED: recommendation derived from current factory/API mismatch]  
**Warning signs:** Scene presets start carrying explicit width/height, but tests for `default_camera_rig_for_scene(..., 640, 480)` are left unchanged without any rescale rule. [VERIFIED: `tests/test_realtime_scene_factory.cpp`]  

## Code Examples

Verified project patterns and recommended usage:

### Convert `CameraSpec` Into Existing Runtime Types

This preserves the repo's tested runtime math and keeps authored data separate from packed runtime state. [VERIFIED: `src/realtime/camera_models.cpp`, `src/realtime/camera_rig.cpp`]

```cpp
// Source: synthesized from `src/realtime/camera_models.cpp` and `src/realtime/camera_rig.cpp`.
rt::CameraRig::Slot to_slot(const rt::scene::CameraSpec& spec) {
    rt::CameraRig::Slot slot;
    slot.model = spec.model;
    slot.T_bc = spec.T_bc;
    slot.width = spec.width;
    slot.height = spec.height;

    if (spec.model == rt::CameraModelType::pinhole32) {
        slot.params = rt::Pinhole32Params {
            spec.fx, spec.fy, spec.cx, spec.cy,
            spec.pinhole.k1, spec.pinhole.k2, spec.pinhole.k3, spec.pinhole.p1, spec.pinhole.p2,
        };
    } else {
        slot.params = rt::make_equi62_lut1d_params(
            spec.width, spec.height, spec.fx, spec.fy, spec.cx, spec.cy,
            spec.equi.radial, spec.equi.tangential);
    }
    return slot;
}
```

### Reject Missing Or Unsupported YAML Camera Models

This is the key loader-side behavior change for repo-owned scene files. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/yaml_scene_loader.cpp`]

```cpp
// Source: recommended loader behavior based on current YAML validation style in `src/scene/yaml_scene_loader.cpp`.
CameraModelType parse_camera_model(const YAML::Node& node) {
    if (!node) {
        throw std::runtime_error("camera.model is required");
    }
    const std::string value = node.as<std::string>();
    if (value == "pinhole32") {
        return CameraModelType::pinhole32;
    }
    if (value == "equi62_lut1d") {
        return CameraModelType::equi62_lut1d;
    }
    throw std::runtime_error("unsupported camera model: " + value);
}
```

### Preserve Current Offline Compatibility As A Shim

This keeps Phase 1 schema work separate from later offline ray-generation changes. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

```cpp
// Source: recommended compatibility boundary based on `src/core/offline_shared_scene_renderer.cpp`.
void configure_offline_camera(const scene::CpuCameraPreset& preset, Camera& cam) {
    // v1 Phase 1: continue using legacy pose/framing fields, but source model/intrinsics from `preset.camera`.
    if (preset.camera.model != CameraModelType::pinhole32) {
        throw std::invalid_argument("Phase 1 offline path still expects pinhole-compatible camera specs");
    }
    cam.aspect_ratio = preset.aspect_ratio;
    cam.image_width = preset.image_width;
    cam.max_depth = preset.max_depth;
    cam.lookfrom = preset.lookfrom;
    cam.lookat = preset.lookat;
    cam.vup = preset.vup;
    cam.defocus_angle = preset.defocus_angle;
    cam.focus_dist = preset.focus_dist;
    cam.vfov = 2.0 * std::atan(0.5 * static_cast<double>(preset.camera.height) / preset.camera.fy) * 180.0 / std::numbers::pi;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| CPU scene presets describe camera framing through `vfov`, `lookfrom`, `lookat`, and `vup` with no explicit camera model. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/yaml_scene_loader.cpp`] | Phase 1 should embed an explicit `CameraSpec` with `model` and common intrinsics inside the CPU preset wrapper. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | Planned in Phase 1 on 2026-04-19. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | Removes implicit pinhole assumptions from scene data while preserving current offline pose fields for now. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`] |
| Realtime presets describe startup view through `initial_body_pose`, `vfov_deg`, and `use_default_viewer_intrinsics`. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/yaml_scene_loader.cpp`] | Phase 1 should replace `vfov_deg`/`use_default_viewer_intrinsics` with explicit `CameraSpec` intrinsics and model choice. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | Planned in Phase 1 on 2026-04-19. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] | Makes startup camera data numerically explicit and removes the last realtime preset-only pinhole shortcut. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`] |
| `PackedCamera` and GPU `DeviceActiveCamera` are the richest concrete camera types in the repo, so they can look like natural schema candidates. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/gpu/launch_params.h`] | The current repo boundary is to treat them as runtime products derived from authored scene data. [VERIFIED: recommendation synthesized from `src/realtime/camera_rig.cpp`, `src/realtime/gpu/optix_renderer.cpp`] | Already true in current code and should stay true after Phase 1. [VERIFIED: `src/realtime/camera_rig.cpp`, `src/realtime/gpu/optix_renderer.cpp`] | Prevents runtime-only transform/LUT details from leaking into scene/YAML representation. [VERIFIED: `src/realtime/camera_models.cpp`, `src/realtime/camera_rig.cpp`] |

**Deprecated/outdated:**

- `vfov_deg` plus `use_default_viewer_intrinsics` as the long-lived realtime camera declaration is outdated for this milestone because it cannot represent explicit model choice or shared intrinsics. [VERIFIED: `src/scene/shared_scene_builders.h`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
- Missing `model` in repo-owned scene files is outdated and must become a validation error rather than a pinhole assumption. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/yaml_scene_loader.cpp`]
- Treating `PackedCamera` as a scene-schema shortcut is outdated because it hardcodes renderer-space concerns into authored data. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`] 

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| None | All material claims in this document were verified against repo files or the executed CTest slice rather than relying on unverified training knowledge. [VERIFIED: sources throughout this document and `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure`] | N/A | N/A |

## Open Questions (RESOLVED)

1. **Should Phase 1 introduce a scene-level `CameraRigSpec` container now, or only a per-preset `CameraSpec`?**
   - Decision: Phase 1 introduces only a per-preset `CameraSpec`; it does not add a separate authored `CameraRigSpec` container yet. [VERIFIED: matches the locked phase boundary in `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
   - Rationale: current authored scene surfaces still describe one logical camera per CPU/realtime preset, so a container-level schema would add surface area without serving a concrete Phase 1 requirement. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/scene_definition.h`, `.planning/REQUIREMENTS.md`]
   - Planning implication: use container-friendly field names and adapter APIs so later multi-camera authoring can grow without another YAML key rewrite. [VERIFIED: aligned with D-07 in `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]

2. **How should realtime requested output size interact with schema-owned `width` and `height`?**
   - Decision: `CameraSpec.width` and `CameraSpec.height` are authored calibration resolution. When `default_camera_rig_for_scene(scene_id, camera_count, width, height)` is called with a different output size, realtime adapters should rescale `fx`, `fy`, `cx`, and `cy` proportionally from the authored calibration resolution to the requested runtime resolution. [VERIFIED: decision derived from the existing factory signature in `src/realtime/realtime_scene_factory.cpp` and locked D-02 in `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`]
   - Rationale: the current factory API already takes caller-provided output dimensions, and preserving that API with explicit rescaling is lower risk than redefining those parameters as ignored output-only values. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `tests/test_realtime_scene_factory.cpp`]
   - Planning implication: the Phase 1 plan must state this rescaling rule explicitly and add `tests/test_realtime_scene_factory.cpp` coverage that proves authored intrinsics are preserved under same-size calls and proportionally rescaled under size changes. [VERIFIED: current factory/test contract in `src/realtime/realtime_scene_factory.cpp`, `tests/test_realtime_scene_factory.cpp`]

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | CMake + CTest with standalone C++ test executables. [VERIFIED: `CMakeLists.txt`, `build/CTestTestfile.cmake`] |
| Config file | `CMakeLists.txt` with generated `build/CTestTestfile.cmake`. [VERIFIED: `CMakeLists.txt`, `build/CTestTestfile.cmake`] |
| Quick run command | `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure` [VERIFIED: executed successfully during this research] |
| Full suite command | `ctest --test-dir build --output-on-failure` [VERIFIED: `build/CTestTestfile.cmake`, `ctest --version`] |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CAM-01 | Shared scene schema can represent `pinhole32` and `equi62_lut1d` per camera and convert to runtime rig data. [VERIFIED: `.planning/REQUIREMENTS.md`] | unit + integration [VERIFIED: existing coverage pattern in `tests/test_camera_rig.cpp`] | `ctest --test-dir build -R 'test_camera_rig|test_scene_definition' --output-on-failure` [VERIFIED: targets exist in `CMakeLists.txt`] | ✅ extend existing [VERIFIED: `tests/test_camera_rig.cpp`, `tests/test_scene_definition.cpp`] |
| CAM-05 | Shared schema carries `fx`, `fy`, `cx`, `cy`, with v1 zero distortion defaults. [VERIFIED: `.planning/REQUIREMENTS.md`] | unit [VERIFIED: existing math tests in `tests/test_camera_models.cpp`] | `ctest --test-dir build -R 'test_camera_models|test_yaml_scene_loader' --output-on-failure` [VERIFIED: targets exist in `CMakeLists.txt`] | ✅ extend existing [VERIFIED: `tests/test_camera_models.cpp`, `tests/test_yaml_scene_loader.cpp`] |
| DEF-01 | Builtin scene definitions and YAML scenes declare camera `model` explicitly and load through the same schema. [VERIFIED: `.planning/REQUIREMENTS.md`] | integration [VERIFIED: scene loader/catalog/regression tests already cover these surfaces] | `ctest --test-dir build -R 'test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_realtime_scene_factory' --output-on-failure` [VERIFIED: targets exist in `CMakeLists.txt`] | ✅ extend existing [VERIFIED: `tests/test_yaml_scene_loader.cpp`, `tests/test_scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_realtime_scene_factory.cpp`] |

### Sampling Rate

- **Per task commit:** `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure` [VERIFIED: executed successfully during this research]
- **Per wave merge:** `ctest --test-dir build -R 'test_camera_models|test_camera_rig|test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory' --output-on-failure` [VERIFIED: test targets exist in `CMakeLists.txt`]
- **Phase gate:** `ctest --test-dir build --output-on-failure` [VERIFIED: `build/CTestTestfile.cmake`, `ctest --version`]

### Wave 0 Gaps

- [ ] Extend `tests/test_scene_definition.cpp` so the new `CameraSpec` fields are owned and copied correctly, not just preset ids. [VERIFIED: `tests/test_scene_definition.cpp` currently only checks metadata and preset string ownership]
- [ ] Extend `tests/test_yaml_scene_loader.cpp` with negative coverage for missing `camera.model` and unsupported model names, plus positive coverage for explicit `pinhole32` and `equi62_lut1d` parsing. [VERIFIED: `tests/test_yaml_scene_loader.cpp` currently asserts legacy `vfov` and realtime preset parsing only]
- [ ] Extend `tests/test_shared_scene_regression.cpp` to assert migrated builtin/YAML camera models and common intrinsics, not just legacy `lookfrom` or spawn positions. [VERIFIED: `tests/test_shared_scene_regression.cpp` currently checks preset counts, spp, `lookfrom`, and spawn pose]
- [ ] Extend `tests/test_realtime_scene_factory.cpp` to assert that default rig creation preserves migrated schema intrinsics/model instead of only reproducing legacy `vfov`-derived pinhole behavior. [VERIFIED: `tests/test_realtime_scene_factory.cpp` currently checks pinhole `fy` from legacy `vfov`]
- [ ] Keep `tests/test_offline_shared_scene_renderer.cpp` as a structural compatibility check for Phase 1 and add explicit assertions around the pinhole-only shim if `CameraSpec` reaches the offline path. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`, `src/core/offline_shared_scene_renderer.cpp`] 

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no [VERIFIED: this phase changes local scene schema and loader code, not identity or login flows] | Not applicable in the current repo surface. [VERIFIED: `src/scene/*`, `src/realtime/*`, `src/core/*`] |
| V3 Session Management | no [VERIFIED: this repo has no session-management subsystem in the inspected camera/schema paths] | Not applicable in the current repo surface. [VERIFIED: `src/scene/*`, `src/realtime/*`, `src/core/*`] |
| V4 Access Control | no [VERIFIED: Phase 1 changes do not introduce authorization decisions] | Not applicable in the current repo surface. [VERIFIED: inspected phase paths above] |
| V5 Input Validation | yes [VERIFIED: `yaml_scene_loader.cpp` is a parser/validator for untrusted file input] | Extend existing loader checks so `camera.model` is required, enum values are validated, and camera numeric fields are sanity-checked before adapter conversion. [VERIFIED: `src/scene/yaml_scene_loader.cpp`, `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`] |
| V6 Cryptography | no [VERIFIED: no cryptographic operations are involved in the phase scope] | No crypto path; do not invent one. [VERIFIED: inspected phase paths and env scan] |

### Known Threat Patterns for YAML Scene Schema

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Missing or unknown `camera.model` silently changing semantics | Tampering | Reject missing/unsupported model values in `yaml_scene_loader.cpp`; do not keep implicit defaults. [VERIFIED: `.planning/phases/01-shared-camera-schema/01-CONTEXT.md`, `src/scene/yaml_scene_loader.cpp`] |
| Width/height or focal-length values that produce impossible runtime cameras | Tampering | Validate positive dimensions and focal lengths once in schema parsing/conversion before calling runtime helpers. Existing runtime code already rejects invalid packed dimensions/focal lengths in the offline path. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/realtime_scene_factory.cpp`] |
| File-backed scene overlay unexpectedly masking migrated builtin camera data | Tampering | Keep catalog overlay tests and ensure migrated YAML and builtin definitions agree on schema shape. [VERIFIED: `src/scene/scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`] |
| Derived LUT or renderer transform stored as authored data | Tampering | Keep authored schema limited to `T_bc`, common intrinsics, and model-specific distortion slots; derive LUT and `T_rc` only in adapters. [VERIFIED: `src/realtime/camera_models.cpp`, `src/realtime/camera_rig.cpp`] |

## Sources

### Primary (HIGH confidence)

- `src/realtime/camera_models.h` - current runtime camera enum and parameter structs. [VERIFIED: `src/realtime/camera_models.h`]
- `src/realtime/camera_models.cpp` - LUT generation and projection/unprojection helpers. [VERIFIED: `src/realtime/camera_models.cpp`]
- `src/realtime/camera_rig.h` and `src/realtime/camera_rig.cpp` - current rig slot, pack, `T_bc`, and `PackedCamera` boundary. [VERIFIED: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`]
- `src/scene/shared_scene_builders.h` and `src/scene/shared_scene_builders.cpp` - current builtin CPU/realtime preset shapes and scene registry tables. [VERIFIED: `src/scene/shared_scene_builders.h`, `src/scene/shared_scene_builders.cpp`]
- `src/scene/scene_definition.h` - current owning scene definition type. [VERIFIED: `src/scene/scene_definition.h`]
- `src/scene/yaml_scene_loader.cpp` - current YAML camera parsing and validation style. [VERIFIED: `src/scene/yaml_scene_loader.cpp`]
- `src/scene/scene_file_catalog.cpp` - builtin/YAML overlay behavior. [VERIFIED: `src/scene/scene_file_catalog.cpp`]
- `src/core/offline_shared_scene_renderer.cpp` - current offline pinhole-only compatibility boundary. [VERIFIED: `src/core/offline_shared_scene_renderer.cpp`]
- `src/realtime/realtime_scene_factory.cpp` and `src/realtime/viewer/four_camera_rig.cpp` - current realtime pinhole rig construction paths. [VERIFIED: `src/realtime/realtime_scene_factory.cpp`, `src/realtime/viewer/four_camera_rig.cpp`]
- `src/realtime/gpu/launch_params.h`, `src/realtime/gpu/optix_renderer.cpp`, and `src/realtime/gpu/programs.cu` - current device-side model-aware runtime handling. [VERIFIED: `src/realtime/gpu/launch_params.h`, `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/programs.cu`]
- `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/imported_obj_smoke/scene.yaml`, and `assets/scenes/simple_light/scene.yaml` - current file-backed camera declaration format. [VERIFIED: those four YAML files]
- `tests/test_camera_rig.cpp`, `tests/test_scene_definition.cpp`, `tests/test_yaml_scene_loader.cpp`, `tests/test_scene_file_catalog.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, and `tests/test_realtime_scene_factory.cpp` - current regression surface for this phase. [VERIFIED: those test files]
- `docs/reference/src-cam/cam_pinhole32.h` and `docs/reference/src-cam/cam_equi62_lut1d.h` - reference parameter shapes behind the runtime math types. [VERIFIED: those reference headers]
- `ctest --test-dir build -R 'test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory|test_camera_rig' --output-on-failure` - executed baseline verification. [VERIFIED: command output during this research]

### Secondary (MEDIUM confidence)

- `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md` - prior scene/preset split decisions that explain why current preset wrappers exist, even though Phase 1 now supersedes the old "separate camera configs" direction. [VERIFIED: `docs/superpowers/specs/2026-04-19-scene-definition-and-render-preset-design.md`] 
- `docs/superpowers/specs/2026-04-18-shared-scene-ir-for-cpu-and-realtime-design.md` - prior shared-scene ownership boundary showing that authored semantics belong in `scene/`, not runtime-only packing types. [VERIFIED: `docs/superpowers/specs/2026-04-18-shared-scene-ir-for-cpu-and-realtime-design.md`] 

### Tertiary (LOW confidence)

- None. [VERIFIED: no web-only or unverified claims were used in this document]

## Metadata

**Confidence breakdown:**

- Standard stack: HIGH - the recommended stack is almost entirely repo-local and directly verified in the inspected source files. [VERIFIED: `src/realtime/camera_models.h`, `src/realtime/camera_rig.h`, `src/scene/shared_scene_builders.h`]
- Architecture: MEDIUM - current boundaries are clear, but the exact `CameraSpec` container shape and realtime resolution-rescale rule still require an implementation decision in planning. [VERIFIED: open questions above]
- Pitfalls: HIGH - the biggest risks are visible in current code structure and test coverage gaps, not inferred from outside information. [VERIFIED: `src/scene/yaml_scene_loader.cpp`, `src/scene/scene_file_catalog.cpp`, `src/realtime/realtime_scene_factory.cpp`, `src/core/offline_shared_scene_renderer.cpp`]

**Research date:** 2026-04-19
**Valid until:** 2026-05-19 for repo-internal code mapping; revisit sooner if Phase 1 plan decides to introduce a `CameraRigSpec` container immediately. [VERIFIED: current repo state and open questions in this document]
