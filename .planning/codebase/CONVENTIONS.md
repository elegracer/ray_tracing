# Coding Conventions

**Analysis Date:** 2026-04-19

## Naming Patterns

**Files:**
- Use lowercase snake_case for implementation and header files under `src/`, for example `src/realtime/camera_rig.cpp`, `src/scene/yaml_scene_loader.cpp`, and `src/realtime/viewer/scene_switch_controller.h`.
- Tests live under `tests/` and follow `test_<subject>.cpp`, for example `tests/test_camera_rig.cpp` and `tests/test_viewer_quality_controller.cpp`.
- Shared test helpers stay in `tests/test_support.h`.

**Functions:**
- Use lowercase snake_case for free functions and methods, for example `make_equi62_lut1d_params` in `src/realtime/camera_models.h`, `render_profiled_smoke_frame` in `src/realtime/realtime_pipeline.h`, and `reload_scene` in `src/scene/scene_file_catalog.h`.
- Use small file-local helper functions inside anonymous namespaces in `.cpp` files, for example `pose_translation_delta` in `src/realtime/viewer/viewer_quality_controller.cpp` and `collect_scene_files` in `src/scene/scene_file_catalog.cpp`.

**Variables:**
- Use lowercase snake_case for locals, parameters, and fields, for example `active_count`, `scene_file`, `current_scene_id_`, and `expected_beauty_size`.
- Private member fields use a trailing underscore, for example `slots_` in `src/realtime/camera_rig.h` and `generation_` in `src/scene/scene_file_catalog.h`.
- Constants use `kCamelCase` in implementation files, for example `kMaxSanitizedBeautyValue` in `src/realtime/viewer/viewer_quality_controller.cpp`.

**Types:**
- Use PascalCase for structs, classes, and enums, for example `SceneDefinition`, `PackedCameraRig`, `ViewerQualityController`, and `ViewerFrameConvention`.
- Enum values use lowercase snake_case even inside `enum class`, for example `CameraModelType::pinhole32` in `src/realtime/camera_models.h` and `ViewerFrameConvention::legacy_y_up` in `src/realtime/viewer/body_pose.h`.
- Namespaces stay short and domain-based: `rt`, `rt::scene`, and `rt::viewer`.

## Code Style

**Formatting:**
- Formatting is driven by `.clang-format`.
- Use 4-space indentation and never tabs: `.clang-format`.
- Keep lines within 100 columns: `.clang-format`.
- Keep pointer/reference style attached to the type, for example `const SceneDefinition*` and `std::string_view`: `.clang-format` with `PointerAlignment: Left`.
- Keep braces attached rather than Allman style, matching files such as `src/realtime/camera_rig.cpp` and `src/scene/scene_file_catalog.cpp`.
- Keep include order stable by author intent; include sorting is disabled with `SortIncludes: false` in `.clang-format`.

**Linting:**
- `clang-format` is the only repo-level style tool detected via `.clang-format`.
- No `.clang-tidy`, `.editorconfig`, `cpplint`, or `cppcheck` config was detected at the repository root.

## Import Organization

**Order:**
1. The matching project header first in `.cpp` files, for example `#include "realtime/camera_rig.h"` in `src/realtime/camera_rig.cpp`.
2. Additional project headers, for example `#include "scene/shared_scene_builders.h"` and `#include "scene/yaml_scene_loader.h"` in `src/scene/scene_file_catalog.cpp`.
3. Third-party headers, usually Eigen, OpenCV, YAML, TBB, or CUDA headers, for example `#include <Eigen/Core>` in `src/scene/shared_scene_ir.h`.
4. Standard library headers last, for example `<stdexcept>`, `<vector>`, and `<string_view>`.

**Path Aliases:**
- No path alias system is used.
- CMake exposes `src/` as an include root, so project includes use logical paths like `"realtime/viewer/body_pose.h"` and `"scene/scene_definition.h"` instead of relative `../` paths: `CMakeLists.txt`.

## Data Structure Patterns

**Aggregates with defaults:**
- Public data types are usually plain structs with default member initialization, for example `RenderProfile` in `src/realtime/render_profile.h`, `SceneDefinition` in `src/scene/scene_definition.h`, and `BodyPose` in `src/realtime/viewer/body_pose.h`.

**Tagged unions via `std::variant`:**
- Scene-description layers model closed sets of texture, material, and shape types with `std::variant`, for example `TextureDesc`, `MaterialDesc`, and `ShapeDesc` in `src/scene/shared_scene_ir.h`.
- Conversion code dispatches variants with visitors plus `static_assert` fallback branches, for example `src/scene/cpu_scene_adapter.cpp` and `src/scene/realtime_scene_adapter.cpp`.

**Value-semantic containers:**
- APIs prefer `std::vector`, `std::array`, `std::optional`, and owning `std::string` over shared mutable state, for example `std::array<PackedCamera, 4>` in `src/realtime/camera_rig.h` and `std::optional<RealtimeViewPreset>` in `src/scene/scene_definition.h`.
- View-style lookup APIs return pointers instead of references or option wrappers when absence is allowed, for example `find_scene_catalog_entry` in `src/realtime/scene_catalog.h` and `find_scene` in `src/scene/scene_file_catalog.h`.

## Error Handling

**Patterns:**
- Production code uses exceptions for invalid inputs and violated invariants, not `assert(...)`.
- Use `std::invalid_argument` for bad caller input, for example in `src/core/offline_shared_scene_renderer.cpp`, `src/realtime/realtime_scene_factory.cpp`, and `src/scene/cpu_scene_adapter.cpp`.
- Use `std::out_of_range` for invalid indices, for example in `src/scene/realtime_scene_adapter.cpp` and `src/scene/cpu_scene_adapter.cpp`.
- Use `std::runtime_error` for parse failures, IO failures, and runtime/backend failures, for example in `src/scene/yaml_scene_loader.cpp`, `src/scene/obj_mtl_importer.cpp`, `src/realtime/gpu/optix_renderer.cpp`, and `src/realtime/profiling/benchmark_report.cpp`.
- When a non-throwing surface is needed, wrap exceptions into small status structs, for example `ReloadStatus` in `src/scene/scene_file_catalog.h` and `SceneCatalogUpdateResult` in `src/realtime/viewer/scene_switch_controller.h`.

## Logging

**Framework:** None detected

**Patterns:**
- There is no centralized logging layer under `src/`.
- CLI-style failure reporting in tests uses `std::cerr` plus `EXIT_FAILURE`, for example `tests/test_cpu_render_smoke.cpp`.
- Production code prefers throwing descriptive exceptions rather than logging and continuing.

## Comments

**When to Comment:**
- Comments are sparse in new `src/realtime/` and `src/scene/` code. Add them mainly for rationale or non-obvious fallback behavior, for example the builtin fallback note in `src/scene/scene_file_catalog.cpp`.
- Legacy `src/common/` headers are more heavily commented around math and rendering formulas, for example `src/common/camera.h`, `src/common/hittable.h`, and `src/common/sphere.h`.
- Namespace closing comments are used consistently, for example `}  // namespace rt::scene`.

**JSDoc/TSDoc:**
- Not applicable in this C++ codebase.
- C++ docblock usage is not part of the active `src/` style; no Doxygen-style convention is enforced in the main source tree.

## Function Design

**Size:**
- Header APIs stay compact and declaration-heavy, while parsing/building code expands in implementation files. Large logic tends to stay contained in one translation unit, for example `src/scene/yaml_scene_loader.cpp` and `src/scene/shared_scene_builders.cpp`.

**Parameters:**
- Pass larger inputs by `const&` and lightweight identifiers by `std::string_view`, for example `begin_frame(std::string_view scene_id, const BodyPose& pose)` in `src/realtime/viewer/viewer_quality_controller.cpp`.
- Use `int` for counts that eventually feed GPU/packed structures, for example widths, heights, and camera counts in `src/realtime/camera_rig.h` and `src/realtime/realtime_pipeline.h`.

**Return Values:**
- Prefer value returns for aggregates and snapshots, for example `PackedCameraRig CameraRig::pack() const` in `src/realtime/camera_rig.cpp` and `SceneDefinition load_scene_definition(...)` in `src/scene/yaml_scene_loader.h`.
- Use designated initializers for aggregate return values and temporaries where helpful, for example `RenderProfile::quality()` in `src/realtime/render_profile.h` and `ResolvedBeautyFrameView {...}` in `src/realtime/viewer/viewer_quality_controller.cpp`.

## Module Design

**Exports:**
- Each feature is exposed through a focused header/implementation pair, for example `src/realtime/camera_models.h` with `src/realtime/camera_models.cpp` and `src/scene/scene_file_catalog.h` with `src/scene/scene_file_catalog.cpp`.
- Small inline-only utilities stay in headers when the entire API is trivial, for example `src/realtime/render_profile.h` and many files under `src/common/`.

**Barrel Files:**
- No barrel or umbrella headers were detected for the active `src/realtime/` or `src/scene/` code.
- Call sites include the concrete module they use, which keeps dependencies explicit.

---

*Convention analysis: 2026-04-19*
