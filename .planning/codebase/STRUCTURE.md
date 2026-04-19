# Codebase Structure

**Analysis Date:** 2026-04-19

## Directory Layout

```text
ray_tracing/
├── src/                             # Primary C++ source tree
├── utils/                           # Executable entry points and helper scripts
├── tests/                           # Standalone CTest executables
├── assets/scenes/                   # YAML scene definitions and imported scene assets
├── cmake/                           # CMake helper scripts and generated header template
├── bin/                             # Built executables and shared libraries from CMake
├── build/                           # Active CMake build tree, generated files, and test outputs
├── docs/                            # Reference docs and workflow notes
├── NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64/  # Vendored OptiX SDK tree
└── 3rdparty/                        # Additional vendored source area
```

## Directory Purposes

**`src/common/`:**
- Purpose: CPU path tracing primitives and reusable geometry/material/texture types.
- Contains: Header-only and inline-heavy types such as `src/common/camera.h`, `src/common/hittable.h`, `src/common/material.h`, and `src/common/texture.h`.
- Key files: `src/common/camera.h`, `src/common/hittable_list.h`, `src/common/bvh.h`

**`src/core/`:**
- Purpose: Thin library-facing surface for the offline renderer and version helpers.
- Contains: `render_shared_scene(...)` orchestration and project description/version helpers.
- Key files: `src/core/offline_shared_scene_renderer.cpp`, `src/core/core.cpp`

**`src/scene/`:**
- Purpose: Scene schema, scene loading, builtin scene registration, and backend adaptation.
- Contains: `SceneIR`, YAML parsing, OBJ import, scene file cataloging, and CPU/realtime adapter code.
- Key files: `src/scene/shared_scene_ir.h`, `src/scene/yaml_scene_loader.cpp`, `src/scene/scene_file_catalog.cpp`, `src/scene/cpu_scene_adapter.cpp`, `src/scene/realtime_scene_adapter.cpp`

**`src/realtime/`:**
- Purpose: Realtime-facing types that are independent of the GPU implementation details.
- Contains: camera models, camera rig packing, scene packing, render profiles, scene catalog access, and profiling helpers.
- Key files: `src/realtime/scene_description.h`, `src/realtime/camera_rig.cpp`, `src/realtime/realtime_scene_factory.cpp`, `src/realtime/render_profile.h`

**`src/realtime/gpu/`:**
- Purpose: CUDA/OptiX implementation layer.
- Contains: renderer upload/launch logic, denoiser integration, GPU frame buffer structs, and CUDA kernels.
- Key files: `src/realtime/gpu/optix_renderer.cpp`, `src/realtime/gpu/programs.cu`, `src/realtime/gpu/renderer_pool.cpp`, `src/realtime/gpu/denoiser.cpp`

**`src/realtime/viewer/`:**
- Purpose: Interactive viewer controls and scene-management helpers.
- Contains: viewer body pose math, four-camera rig generation, scene switching/reload, move speed, and quality accumulation control.
- Key files: `src/realtime/viewer/body_pose.cpp`, `src/realtime/viewer/four_camera_rig.cpp`, `src/realtime/viewer/scene_switch_controller.cpp`, `src/realtime/viewer/viewer_quality_controller.cpp`

**`utils/`:**
- Purpose: Main programs and small standalone math/demo tools.
- Contains: render CLIs (`render_scene.cpp`, `render_realtime.cpp`, `render_realtime_viewer.cpp`) plus simple integration/sampling experiments.
- Key files: `utils/render_scene.cpp`, `utils/render_realtime.cpp`, `utils/render_realtime_viewer.cpp`, `utils/run_realtime_benchmark_matrix.sh`

**`tests/`:**
- Purpose: One-source-per-executable regression and unit coverage for scene loading, adapters, viewer math, GPU rendering, and profiling.
- Contains: `test_*.cpp` files plus `tests/test_support.h`.
- Key files: `tests/test_yaml_scene_loader.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_realtime_pipeline.cpp`, `tests/test_reference_vs_realtime.cpp`

**`assets/scenes/`:**
- Purpose: File-backed scene definitions that complement or override builtin scenes.
- Contains: `scene.yaml` files, included YAML fragments, and imported OBJ/MTL assets.
- Key files: `assets/scenes/cornell_box/scene.yaml`, `assets/scenes/final_room/scene.yaml`, `assets/scenes/common/materials/common_materials.yaml`, `assets/scenes/imported_obj_smoke/models/triangle.obj`

**`cmake/`:**
- Purpose: Custom CMake support files used by the main build.
- Contains: version header template and CTest verification scripts for realtime CLI output.
- Key files: `cmake/version.h.in`, `cmake/VerifyRenderRealtimeProfiling.cmake`, `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`

## Key File Locations

**Entry Points:**
- `utils/render_scene.cpp`: Offline still-image renderer entry point.
- `utils/render_realtime.cpp`: Realtime benchmark/smoke CLI entry point.
- `utils/render_realtime_viewer.cpp`: GLFW/OpenGL/ImGui viewer entry point.

**Configuration:**
- `CMakeLists.txt`: Root target graph, dependency discovery, and output directory definitions.
- `.clang-format`: Repository formatting rules.
- `cmake/version.h.in`: Template for the generated `build/include/core/version.h`.

**Core Logic:**
- `src/scene/shared_scene_ir.h`: Shared scene intermediate representation.
- `src/scene/scene_file_catalog.cpp`: Builtin + file-backed scene registry and hot reload.
- `src/core/offline_shared_scene_renderer.cpp`: Offline render orchestration.
- `src/realtime/realtime_scene_factory.cpp`: Realtime scene/rig construction per scene id.
- `src/realtime/gpu/optix_renderer.cpp`: OptiX scene upload and render execution.

**Testing:**
- `tests/`: Source files for test executables.
- `build/test/`: Runtime artifacts emitted by CTest helper scripts and CLI verification runs.
- `Testing/` and `build/Testing/`: CTest metadata and temporary logs.

## Naming Conventions

**Files:**
- Production source files use lowercase snake_case, for example `src/scene/scene_file_catalog.cpp` and `src/realtime/viewer/viewer_quality_controller.h`.
- Tests use `tests/test_<feature>.cpp`, for example `tests/test_renderer_pool.cpp` and `tests/test_viewer_scene_reload.cpp`.
- Scene data files use fixed `scene.yaml` names under per-scene directories such as `assets/scenes/final_room/scene.yaml`.

**Directories:**
- Source directories are organized by subsystem, not by target: `src/scene/`, `src/realtime/`, `src/realtime/gpu/`, and `src/realtime/viewer/`.
- Scene asset directories are grouped by scene id under `assets/scenes/`, matching the ids exposed through the catalog where possible.

## Where to Add New Code

**New Offline/Shared Scene Feature:**
- Primary code: `src/scene/` if the change affects scene schema, loading, or adaptation; `src/core/` only if it is specific to offline render orchestration.
- Tests: add a new `tests/test_<feature>.cpp` executable and wire it in `CMakeLists.txt`.

**New Realtime Feature:**
- Primary code: `src/realtime/` for scene packing, profiles, camera math, or profiling; `src/realtime/gpu/` for OptiX/CUDA launch or upload changes.
- Tests: `tests/test_optix_*.cpp`, `tests/test_realtime_*.cpp`, or `tests/test_reference_vs_realtime.cpp` style coverage depending on the layer.

**New Viewer Behavior:**
- Implementation: `src/realtime/viewer/`
- Entry point wiring: `utils/render_realtime_viewer.cpp` only when the behavior must be surfaced in the main loop or UI.

**New Scene Content:**
- File-backed scene: add a new directory under `assets/scenes/<scene_id>/` with `scene.yaml` and any colocated assets.
- Builtin scene: add the builder/preset registration in `src/scene/shared_scene_builders.cpp`.

**Utilities:**
- Shared helpers: prefer the nearest subsystem directory under `src/` instead of creating a generic helpers folder.
- Standalone executable/demo: place the entry point in `utils/` and register a target in `CMakeLists.txt`.

## Special Directories

**`bin/`:**
- Purpose: CMake runtime and library output directory.
- Generated: Yes
- Committed: No

**`build/`:**
- Purpose: Main build tree with `CMakeFiles/`, generated headers, `vcpkg_installed/`, and test output directories such as `build/test/`.
- Generated: Yes
- Committed: No

**`build/include/`:**
- Purpose: Generated include tree for configured headers such as `build/include/core/version.h`.
- Generated: Yes
- Committed: No

**`Testing/` and `build/Testing/`:**
- Purpose: CTest state, timestamps, and temporary logs.
- Generated: Yes
- Committed: No

**`.cache/` and `build/.cache/`:**
- Purpose: Editor and language-server caches such as `clangd` indexes.
- Generated: Yes
- Committed: No

**`NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64/`:**
- Purpose: Vendored OptiX SDK reference tree used for headers and local examples.
- Generated: No
- Committed: Yes

## Practical Placement Rules

- Put scene schema changes in `src/scene/shared_scene_ir.h` first, then update both adapters in `src/scene/cpu_scene_adapter.cpp` and `src/scene/realtime_scene_adapter.cpp` if the new primitive/material must work in both backends.
- When adding a new realtime scene entry point behavior, start from `src/realtime/realtime_scene_factory.cpp`; do not hardcode scene-specific logic directly in `utils/render_realtime.cpp` or `utils/render_realtime_viewer.cpp`.
- Keep CLI argument parsing in `utils/` and keep rendering logic below that boundary in `src/`.
- If a feature needs hot-reloadable scene data, prefer `assets/scenes/` plus `src/scene/yaml_scene_loader.cpp` over adding more hardcoded scene builders.
- There is no separate `include/` source tree. Public headers live beside implementation files under `src/`.

---

*Structure analysis: 2026-04-19*
