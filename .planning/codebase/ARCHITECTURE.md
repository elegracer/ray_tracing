# Architecture

**Analysis Date:** 2026-04-19

## Pattern Overview

**Overall:** Shared-scene core with dual render backends.

**Key Characteristics:**
- Scene content is normalized into a backend-agnostic `scene::SceneIR` in `src/scene/shared_scene_ir.h` before any CPU or GPU rendering work happens.
- Runtime entry points live in `utils/` and stay thin; most orchestration is delegated into `core`, `scene`, `realtime`, and `realtime/gpu` code referenced from `CMakeLists.txt`.
- The codebase supports both builtin scenes from `src/scene/shared_scene_builders.cpp` and file-backed scenes from `assets/scenes/**/scene.yaml` through the same catalog interfaces in `src/scene/scene_file_catalog.h` and `src/realtime/scene_catalog.h`.

## Layers

**Build/Target Assembly:**
- Purpose: Defines the link-time boundaries that shape the runtime architecture.
- Location: `CMakeLists.txt`
- Contains: `core` shared library, `realtime_gpu` static library, CLI executables in `utils/`, and standalone test executables in `tests/`.
- Depends on: CMake packages such as CUDA, OptiX headers, OpenCV, yaml-cpp, tinyobjloader, GLFW, OpenGL, and ImGui.
- Used by: All binaries under `bin/`.

**Common CPU Ray Tracer Primitives:**
- Purpose: Supplies hittables, materials, textures, BVH, and the CPU camera used by offline rendering.
- Location: `src/common/`
- Contains: `Camera`, `Hittable`, `Material`, `Texture`, `Sphere`, `Quad`, `Triangle`, `ConstantMedium`, and support math/types from files such as `src/common/camera.h`, `src/common/hittable.h`, and `src/common/material.h`.
- Depends on: Internal math/types only.
- Used by: `src/scene/cpu_scene_adapter.cpp` and `src/core/offline_shared_scene_renderer.cpp`.

**Scene Definition and Catalog Layer:**
- Purpose: Owns scene metadata, YAML loading, OBJ/MTL imports, builtin scene registration, and hot-reloadable scene lookup.
- Location: `src/scene/`
- Contains: `SceneDefinition`, `SceneIR`, YAML parsing, OBJ import, scene catalogs, and adapters.
- Depends on: `src/common/` for CPU adaptation targets, `src/realtime/` types for realtime adaptation targets, yaml-cpp, tinyobjloader, and Eigen.
- Used by: Both offline and realtime render paths.

**Offline CPU Rendering Layer:**
- Purpose: Converts `SceneIR` into CPU hittables and renders still images through the book-style path tracer.
- Location: `src/scene/cpu_scene_adapter.cpp`, `src/core/offline_shared_scene_renderer.cpp`
- Contains: `adapt_to_cpu(...)`, camera preset resolution, packed-camera to offline-camera conversion, and `render_shared_scene(...)`.
- Depends on: `src/common/`, `src/scene/shared_scene_builders.h`, and `src/realtime/camera_rig.h`.
- Used by: `utils/render_scene.cpp` and tests such as `tests/test_offline_shared_scene_renderer.cpp`.

**Realtime Scene Preparation Layer:**
- Purpose: Converts `SceneIR` into a GPU-friendly scene description and derives camera rigs/view presets for realtime rendering.
- Location: `src/scene/realtime_scene_adapter.cpp`, `src/realtime/realtime_scene_factory.cpp`, `src/realtime/camera_rig.cpp`
- Contains: `SceneDescription`, `PackedScene`, `CameraRig`, `PackedCameraRig`, frame convention transforms, and per-scene default view presets.
- Depends on: `src/scene/`, Eigen, and realtime camera/viewer support code.
- Used by: `utils/render_realtime.cpp`, `utils/render_realtime_viewer.cpp`, and GPU tests.

**Realtime GPU Execution Layer:**
- Purpose: Uploads packed scenes, launches CUDA/OptiX kernels, downloads auxiliary buffers, and optionally denoises the result.
- Location: `src/realtime/gpu/`
- Contains: `OptixRenderer`, `RendererPool`, OptiX launch params, frame buffer types, and CUDA programs in `src/realtime/gpu/programs.cu`.
- Depends on: CUDA, OptiX, `src/realtime/scene_description.h`, `src/realtime/camera_rig.h`, and render profiles.
- Used by: `utils/render_realtime.cpp`, `utils/render_realtime_viewer.cpp`, and tests such as `tests/test_optix_path_trace.cpp`.

**Viewer Control Layer:**
- Purpose: Maintains interactive camera pose, scene switching/reload, and quality-mode accumulation on top of the realtime renderer.
- Location: `src/realtime/viewer/`
- Contains: `BodyPose`, `make_default_viewer_rig(...)`, `ViewerQualityController`, `SceneSwitchController`, `MoveSpeedState`, and default viewer helpers.
- Depends on: `src/realtime/` scene and rig APIs plus `src/scene/scene_file_catalog.h` for reload/rescan.
- Used by: `utils/render_realtime_viewer.cpp`.

**Profiling and Reporting Layer:**
- Purpose: Captures per-frame/per-camera timing and writes benchmark artifacts for realtime runs.
- Location: `src/realtime/profiling/benchmark_report.h`, `src/realtime/profiling/benchmark_report.cpp`
- Contains: run/frame/camera timing structures and CSV/JSON writers.
- Depends on: Standard library and realtime CLI timing data.
- Used by: `utils/render_realtime.cpp`.

## Data Flow

**Scene Catalog and Load Path:**

1. `src/scene/scene_file_catalog.cpp` seeds the global catalog with builtin scenes from `src/scene/shared_scene_builders.cpp`.
2. The same catalog scans `assets/scenes/` for `scene.yaml` files and replaces builtin records when a file-backed scene reuses the same scene id.
3. `src/scene/yaml_scene_loader.cpp` loads YAML, follows `includes`, merges imported OBJ/MTL data through `src/scene/obj_mtl_importer.cpp`, and produces a `SceneDefinition`.
4. `src/scene/shared_scene_builders.cpp` exposes the loaded scene as `SceneIR`, `SceneMetadata`, CPU presets, background color, and realtime presets.
5. `src/realtime/scene_catalog.cpp` publishes a small `SceneCatalogEntry` view for CLIs and the viewer UI.

**Offline CPU Render Pipeline:**

1. `utils/render_scene.cpp` validates `--scene` against `find_scene_catalog_entry(...)`.
2. `src/core/offline_shared_scene_renderer.cpp` fetches the default CPU preset from `src/scene/shared_scene_builders.cpp`.
3. `src/scene/cpu_scene_adapter.cpp` turns `SceneIR` textures/materials/shapes/instances/media into `pro::proxy<Hittable>` and optional light lists.
4. `src/common/camera.h` renders the adapted world and stores the image in `Camera::img`.
5. `utils/render_scene.cpp` writes the `cv::Mat` to `<scene>.<format>` via OpenCV.

**Realtime CLI Render Pipeline:**

1. `utils/render_realtime.cpp` resolves a `RenderProfile`, creates an output directory, and builds a `PackedScene` via `make_realtime_scene(scene_id).pack()`.
2. `src/realtime/realtime_scene_factory.cpp` adapts `SceneIR` into `SceneDescription`, copies scene background, and builds the default `CameraRig` for `1..4` cameras.
3. `src/realtime/gpu/renderer_pool.cpp` prepares the scene in each `OptixRenderer` instance and launches one async render per active camera.
4. `src/realtime/gpu/optix_renderer.cpp` uploads textures/materials/primitives, builds or refits acceleration structures, launches the OptiX radiance pipeline, and downloads beauty/normal/albedo/depth buffers.
5. `utils/render_realtime.cpp` optionally denoises each camera frame, writes PNGs, and emits `benchmark_frames.csv` plus `benchmark_summary.json`.

**Realtime Viewer Pipeline:**

1. `utils/render_realtime_viewer.cpp` creates the GLFW/OpenGL/ImGui shell and prepares a `RendererPool` with the current realtime scene.
2. Each frame, `src/realtime/viewer/body_pose.cpp` and `src/realtime/viewer/move_speed.cpp` update the body pose from mouse, WASD, QE, and scroll input.
3. `src/realtime/viewer/four_camera_rig.cpp` converts the body pose into a four-camera surround `CameraRig`.
4. `src/realtime/viewer/viewer_quality_controller.cpp` chooses preview vs converge profile and manages accumulation history resets when the pose or scene changes.
5. The viewer uploads each resolved beauty frame into OpenGL textures and draws a fixed `2x2` panel layout, while `src/realtime/viewer/scene_switch_controller.cpp` handles scene switching, reload, and directory rescans.

**State Management:**
- Scene state is centralized in the singleton returned by `src/scene/scene_file_catalog.cpp::global_scene_file_catalog()`.
- Viewer interaction state is local to `utils/render_realtime_viewer.cpp` and helper classes in `src/realtime/viewer/`.
- GPU renderer state is per-instance inside `src/realtime/gpu/optix_renderer.h`; multi-camera parallelism is handled by `src/realtime/gpu/renderer_pool.cpp`.

## Key Abstractions

**`scene::SceneIR`:**
- Purpose: Canonical intermediate representation for scene content before backend-specific adaptation.
- Examples: `src/scene/shared_scene_ir.h`, `src/scene/shared_scene_builders.cpp`, `src/scene/yaml_scene_loader.cpp`
- Pattern: Textures, materials, shapes, surface instances, and media are stored separately and referenced by integer indices.

**`SceneDefinition` and `SceneFileCatalog`:**
- Purpose: Bundle scene metadata, presets, dependencies, and IR; expose catalog lookup and reload.
- Examples: `src/scene/scene_definition.h`, `src/scene/scene_file_catalog.h`
- Pattern: File-backed scenes and builtin scenes share the same record shape and lookup surface.

**`SceneDescription` / `PackedScene`:**
- Purpose: Realtime-side representation for textures, materials, explicit primitives, and homogeneous media.
- Examples: `src/realtime/scene_description.h`, `src/scene/realtime_scene_adapter.cpp`
- Pattern: CPU adapters keep higher-level hittables; GPU adapters flatten boxes into quads and triangle meshes into explicit triangles.

**`CameraRig` / `PackedCameraRig`:**
- Purpose: Normalize up to four active cameras into a fixed upload shape for the GPU renderer.
- Examples: `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp`, `src/realtime/viewer/four_camera_rig.cpp`
- Pattern: Builder-style rig assembly followed by `.pack()` into a four-slot struct consumed by OptiX kernels.

**`RenderProfile`:**
- Purpose: Captures quality/performance tradeoffs for realtime rendering and accumulation reset thresholds.
- Examples: `src/realtime/render_profile.h`, `src/realtime/viewer/viewer_quality_controller.cpp`, `utils/render_realtime.cpp`
- Pattern: Value object with named presets (`quality`, `balanced`, `realtime`) reused by both CLI and viewer.

## Entry Points

**Offline Still Renderer:**
- Location: `utils/render_scene.cpp`
- Triggers: `bin/render_scene`
- Responsibilities: Parse CLI flags, validate scene support, invoke `render_shared_scene(...)`, and write a single image.

**Realtime Benchmark/Smoke Renderer:**
- Location: `utils/render_realtime.cpp`
- Triggers: `bin/render_realtime`
- Responsibilities: Parse CLI flags, prepare scene/rig/profile, render `N` frames across `1..4` cameras, optionally denoise/write PNGs, and emit benchmark artifacts.

**Realtime GUI Viewer:**
- Location: `utils/render_realtime_viewer.cpp`
- Triggers: `bin/render_realtime_viewer`
- Responsibilities: Run the interactive loop, update body pose, render a four-camera view, and expose scene switch/reload controls.

**Library Boundary for Offline Rendering:**
- Location: `src/core/offline_shared_scene_renderer.cpp`
- Triggers: Called from `utils/render_scene.cpp` and tests.
- Responsibilities: Resolve scene presets, adapt to CPU hittables, and produce `cv::Mat` results.

## Error Handling

**Strategy:** Throw exceptions in library and adapter layers, then convert them to CLI stderr messages or UI status strings at the outermost entry point.

**Patterns:**
- Input and index validation use explicit checks with `std::invalid_argument`, `std::runtime_error`, and `std::out_of_range` in files such as `src/scene/yaml_scene_loader.cpp`, `src/scene/cpu_scene_adapter.cpp`, and `src/realtime/gpu/renderer_pool.cpp`.
- File-backed scene loading prepends source file paths to exceptions in `src/scene/yaml_scene_loader.cpp` and `src/scene/obj_mtl_importer.cpp`, which makes catalog reload failures actionable.
- UI-facing reload/switch operations return small status structs instead of throwing across the main loop in `src/realtime/viewer/scene_switch_controller.h`.

## Cross-Cutting Concerns

**Logging:** Minimal and local. CLI tools and the viewer print directly with `fmt::print(...)` in `utils/render_scene.cpp`, `utils/render_realtime.cpp`, and `utils/render_realtime_viewer.cpp`. There is no centralized logging layer.

**Validation:** Strong local validation exists at scene-load, adapter, rig-pack, and render-request boundaries in `src/scene/yaml_scene_loader.cpp`, `src/scene/cpu_scene_adapter.cpp`, `src/realtime/camera_rig.cpp`, `src/realtime/realtime_scene_factory.cpp`, and `src/realtime/gpu/optix_renderer.cpp`.

**Authentication:** Not applicable. The repository is a local renderer with no service authentication layer.

---

*Architecture analysis: 2026-04-19*
