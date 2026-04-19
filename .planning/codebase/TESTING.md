# Testing Patterns

**Analysis Date:** 2026-04-19

## Test Framework

**Runner:**
- `CTest` driven by `enable_testing()` and `add_test(...)` in `CMakeLists.txt`.
- Test registration lives directly in `CMakeLists.txt`; there is no separate `CTestConfig.cmake` or test subdirectory CMake file.
- The configured `build/` tree currently lists 43 tests via `ctest --test-dir build -N`.

**Assertion Library:**
- No third-party assertion library is used.
- Most tests use small throw-on-failure helpers from `tests/test_support.h`: `expect_true`, `expect_near`, and `expect_vec3_near`.
- Some larger tests add file-local helpers such as `expect_throws_contains` in `tests/test_yaml_scene_loader.cpp` and `expect_throws_with_message` in `tests/test_viewer_quality_reference.cpp`.

**Run Commands:**
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
ctest --test-dir build -R 'test_yaml_scene_loader|test_scene_file_catalog' --output-on-failure
```

## Test File Organization

**Location:**
- Tests live in the dedicated `tests/` directory rather than beside source files.
- CLI verification logic lives in CMake scripts under `cmake/`, specifically `cmake/VerifyRenderRealtimeProfiling.cmake` and `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`.

**Naming:**
- Test executables and source files both use `test_<subject>`, for example `tests/test_scene_definition.cpp` mapped to `add_executable(test_scene_definition ...)` in `CMakeLists.txt`.

**Structure:**
```text
tests/
  test_support.h
  test_<subject>.cpp
cmake/
  VerifyRenderRealtimeProfiling.cmake
  VerifyRenderRealtimeFourCameraProfiling.cmake
```

## Test Structure

**Suite Organization:**
```cpp
// `tests/test_camera_rig.cpp`
int main() {
    rt::CameraRig rig;
    rig.add_pinhole(...);

    const rt::PackedCameraRig packed = rig.pack();
    expect_true(packed.cameras[0].enabled == 1, "camera 0 enabled");

    bool threw = false;
    try {
        overflow.add_pinhole(...);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect_true(threw, "fifth camera throws");
    return 0;
}
```

**Patterns:**
- Keep tests as standalone executables with `int main()` instead of macro-based test cases.
- Use anonymous-namespace helper functions for setup and repeated calculations, for example `compute_average_luminance` in `tests/test_viewer_quality_controller.cpp` and `write_text_file` in `tests/test_scene_file_catalog.cpp`.
- Split larger tests into named helper functions called from `main()`, for example `test_reload_current_scene_refreshes_realtime_preset()` in `tests/test_viewer_scene_reload.cpp` and the many `test_*` helpers inside `tests/test_yaml_scene_loader.cpp`.

## Mocking

**Framework:** Not used

**Patterns:**
```cpp
// `tests/test_scene_file_catalog.cpp`
const fs::path root = fs::temp_directory_path() / "scene_file_catalog_reload";
fs::remove_all(root);
write_text_file(scene_file, R"(format_version: 1 ...)");

rt::scene::SceneFileCatalog catalog;
catalog.scan_directory(root);
```

**What to Mock:**
- No explicit mocking convention is present.
- Tests prefer constructing real value objects, writing temporary YAML/OBJ fixtures, and invoking the real adapters, catalog, renderer, or CLI.

**What NOT to Mock:**
- Rendering-path tests exercise the real CPU or OptiX pipelines, for example `tests/test_reference_vs_realtime.cpp`, `tests/test_realtime_texture_materials.cpp`, and `tests/test_viewer_quality_reference.cpp`.
- CLI coverage invokes the real `render_realtime` binary from CTest scripts rather than faking stdout or artifact files: `cmake/VerifyRenderRealtimeProfiling.cmake` and `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`.

## Fixtures and Factories

**Test Data:**
```cpp
// `tests/test_yaml_scene_loader.cpp`
void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}
```

**Location:**
- Shared numeric assertions live in `tests/test_support.h`.
- File-system and scene fixture builders are usually local to the test file, for example `write_scene_file` in `tests/test_viewer_scene_reload.cpp`, `make_checker_fixture` in `tests/test_realtime_texture_materials.cpp`, and `make_scene_definition` in `tests/test_scene_definition.cpp`.
- Repository-owned scene assets used by integration tests live under `assets/scenes`.

## Coverage

**Requirements:** None enforced
- No coverage percentage target, `lcov`, `gcov`, or sanitizer test target was detected in `CMakeLists.txt`.
- Coverage is scenario-based: math/unit checks, parsing/integration tests, GPU smoke tests, and CLI artifact verification.

**View Coverage:**
```bash
ctest --test-dir build -N
```

## Test Types

**Unit Tests:**
- Small pure-logic checks target math and data packing code, for example `tests/test_display_transfer.cpp`, `tests/test_camera_models.cpp`, `tests/test_camera_rig.cpp`, and `tests/test_viewer_body_pose.cpp`.

**Integration Tests:**
- Scene loading, catalog reload, adapters, and report serialization are exercised with real files and real domain objects, for example `tests/test_yaml_scene_loader.cpp`, `tests/test_obj_mtl_importer.cpp`, `tests/test_scene_file_catalog.cpp`, `tests/test_cpu_scene_adapter.cpp`, and `tests/test_realtime_benchmark_report.cpp`.

**GPU/Renderer Regression Tests:**
- OptiX and realtime pipeline behavior is tested end to end through actual renderer calls, for example `tests/test_optix_path_trace.cpp`, `tests/test_renderer_pool.cpp`, `tests/test_realtime_pipeline.cpp`, `tests/test_reference_vs_realtime.cpp`, and `tests/test_viewer_quality_reference.cpp`.

**CLI Tests:**
- `render_realtime` is validated through CTest script tests that inspect stdout and generated files, for example `test_render_realtime_cli`, `test_render_realtime_profiling_cli`, and `test_render_realtime_final_room_quality_cli` in `CMakeLists.txt`.

**E2E Tests:**
- No browser/UI automation or ImGui interaction harness is present.
- Viewer coverage stops at controller/state/render-quality logic such as `tests/test_viewer_scene_switch_controller.cpp`, `tests/test_viewer_scene_reload.cpp`, and `tests/test_viewer_quality_controller.cpp`.

## Common Patterns

**Async Testing:**
```cpp
// `tests/test_reference_vs_realtime.cpp`
tbb::global_control render_threads(
    tbb::global_control::max_allowed_parallelism, 1);
```
- CPU-render reference tests frequently clamp TBB to one thread for deterministic behavior, also seen in `tests/test_cpu_render_smoke.cpp` and `tests/test_viewer_quality_reference.cpp`.

**Error Testing:**
```cpp
// `tests/test_obj_mtl_importer.cpp`
std::string require_error(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        return ex.what();
    }
    throw std::runtime_error("expected exception");
}
```
- Failure-path tests usually capture exceptions and assert on message substrings, for example `tests/test_yaml_scene_loader.cpp`, `tests/test_obj_mtl_importer.cpp`, and `tests/test_scene_file_catalog.cpp`.

## Notable Gaps

- `utils/` executables such as `utils/render_scene.cpp`, `utils/estimate_pi.cpp`, and `utils/estimate_halfway.cpp` do not have dedicated tests; only `utils/render_realtime.cpp` is covered through CTest CLI runs in `CMakeLists.txt`.
- The interactive viewer executable `render_realtime_viewer` is built in `CMakeLists.txt`, but there are no automated tests for ImGui/GLFW event handling or on-screen widget behavior. Existing viewer tests cover body motion, scene switching, rig layout, and quality accumulation logic under `tests/`.
- Much of the legacy CPU path in header-only `src/common/` is exercised indirectly, but there are no per-module tests for files such as `src/common/aabb.h`, `src/common/bvh.h`, or `src/common/pdf.h`.
- There is no separate fast, mock-heavy unit-test layer for GPU-independent logic inside `src/realtime/gpu/`; coverage there depends on CUDA/OptiX-capable test runs such as `tests/test_optix_path_trace.cpp` and `tests/test_renderer_pool.cpp`.

---

*Testing analysis: 2026-04-19*
