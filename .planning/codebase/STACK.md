# Technology Stack

**Analysis Date:** 2026-04-19

## Languages

**Primary:**
- C++23 - Main application, libraries, CLIs, and tests in `src/`, `utils/`, and `tests/` as configured by `CMakeLists.txt`.

**Secondary:**
- CUDA 17 - GPU kernels and OptiX integration in `src/realtime/gpu/programs.cu`, `src/realtime/gpu/optix_renderer.cpp`, and `src/realtime/gpu/denoiser.cpp`.
- CMake - Build and test orchestration in `CMakeLists.txt` and `cmake/VerifyRenderRealtime*.cmake`.
- Bash - Benchmark matrix automation in `utils/run_realtime_benchmark_matrix.sh`.

## Runtime

**Environment:**
- Native Linux desktop/toolchain build. The repo root includes a local NVIDIA SDK directory at `NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64/`.
- CMake minimum version is `3.14` in `CMakeLists.txt`.
- CMake enables `C`, `CXX`, and `CUDA` languages in `CMakeLists.txt`, but repository-owned sources are `*.cpp`, `*.h`, and `*.cu`.

**Compiler / Toolchain:**
- C++ standard: `23` in `CMakeLists.txt`.
- CUDA standard: `17` in `CMakeLists.txt`.
- Default CUDA architectures: `86` and `89` in `CMakeLists.txt`.
- The documented local setup in `README.md` uses `clang`, `clang++`, and `/usr/local/cuda-13.0/bin/nvcc`.
- Optional compiler launcher: `ccache` via `find_program(CCACHE ccache)` in `CMakeLists.txt`.

**Package Manager:**
- `vcpkg` toolchain integration through `CMAKE_TOOLCHAIN_FILE` in `CMakeLists.txt` and `.vscode/settings.json`.
- `README.md` also exports `VCPKG_ROOT=$HOME/vcpkg_root` during configure.
- Lockfile: missing. The repo contains `vcpkg_json`, not a canonical `vcpkg.json`.

## Frameworks

**Core:**
- `CMake` - Declares the build graph, targets, options, and `CTest` registration in `CMakeLists.txt`.
- `CTest` - Test runner enabled by `enable_testing()` in `CMakeLists.txt`.

**Rendering / GPU:**
- NVIDIA OptiX - GPU ray tracing interface via `optix.h`, `optix_stubs.h`, and `optix_function_table_definition.h` in `src/realtime/gpu/optix_renderer.h` and `src/realtime/gpu/optix_renderer.cpp`.
- CUDA Toolkit - Runtime and driver linkage through `CUDA::cudart` and `CUDA::cuda_driver` in `CMakeLists.txt`.
- OpenGL + GLFW + ImGui - Optional interactive viewer stack in `utils/render_realtime_viewer.cpp` and gated by `ENABLE_GUI_VIEWER` in `CMakeLists.txt`.

**Testing:**
- Custom executable-based tests - Each `test_*` target has its own `main()` and uses helpers from `tests/test_support.h`.
- CMake verification scripts - CLI output validation in `cmake/VerifyRenderRealtimeProfiling.cmake` and `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`.

**Build/Dev:**
- VS Code CMake settings in `.vscode/settings.json`.
- Formatting configuration in `.clang-format`.

## Key Dependencies

**Critical:**
- `Eigen3` - Math types and transforms across `src/common/common.h`, `src/realtime/camera_rig.h`, and `src/scene/shared_scene_ir.h`.
- `OpenCV` - Image decode/encode and matrix operations in `src/common/rtw_image.h`, `utils/render_scene.cpp`, `utils/render_realtime.cpp`, and `src/realtime/gpu/optix_renderer.cpp`.
- `yaml-cpp` - Scene file parsing in `src/scene/yaml_scene_loader.cpp`.
- `tinyobjloader` - OBJ/MTL import in `src/scene/obj_mtl_importer.cpp`.
- `TBB` - CPU-side parallel rendering support in `src/common/camera.h`; tests also use `tbb::global_control` in `tests/test_cpu_render_smoke.cpp`.

**Infrastructure:**
- `fmt` - Console output and filename formatting in `utils/render_scene.cpp`, `utils/render_realtime.cpp`, and `src/common/camera.h`.
- `argparse` - CLI argument parsing in `utils/render_scene.cpp`, `utils/render_realtime.cpp`, and `utils/render_realtime_viewer.cpp`.
- `range-v3` - Linked by `core` in `CMakeLists.txt`.
- `msft_proxy4` / `proxy` - Type-erased polymorphism in `src/common/traits.h` and `src/common/hittable*.h`.
- `indicators` - Terminal progress bar support in `src/common/camera.h`.
- `icecream-cpp` - Debug include used in `src/common/camera.h`; include path is discovered with `find_path(ICECREAM_CPP_INCLUDE_DIRS ...)` in `CMakeLists.txt`.
- `imgui`, `glfw3`, `OpenGL` - Viewer-only dependencies behind `ENABLE_GUI_VIEWER` in `CMakeLists.txt`.

**Dependency Source Files:**
- `vcpkg_json` lists `eigen3`, `fmt`, `argparse`, `range-v3`, `proxy`, `opencv`, `icecream-cpp`, `tbb`, `indicators`, and `imgui`.
- `CMakeLists.txt` additionally requires `yaml-cpp`, `tinyobjloader`, `CUDAToolkit`, and the local OptiX include path.

## Notable Targets

**Libraries:**
- `core` - Shared library for scene loading, CPU rendering, and shared scene abstractions in `CMakeLists.txt`.
- `realtime_gpu` - Static library for OptiX/CUDA rendering in `CMakeLists.txt`.

**Primary executables:**
- `render_scene` - Offline CPU renderer in `utils/render_scene.cpp`.
- `render_realtime` - Realtime/benchmark CLI in `utils/render_realtime.cpp`.
- `render_realtime_viewer` - Optional GUI viewer in `utils/render_realtime_viewer.cpp`.

**Utility executables:**
- `estimate_halfway`, `estimate_pi`, `integrate_x_sq`, `sphere_importance`, `cos_cubed`, `cos_density` in `utils/`.

**Test executables:**
- Many `test_*` binaries are declared directly in `CMakeLists.txt`, including CPU, scene-loading, viewer, OptiX, and CLI verification coverage.

## Configuration

**Environment:**
- `CMAKE_TOOLCHAIN_FILE` defaults to `$ENV{HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake` in `CMakeLists.txt`.
- `README.md` expects `VCPKG_ROOT` and a CUDA compiler path during configure.
- No `.env`-style application configuration was detected in repository-owned code.

**Build:**
- Top-level build definition: `CMakeLists.txt`.
- Generated version header source: `cmake/version.h.in`, output to the build tree as `include/core/version.h`.
- Optional GUI toggle: `ENABLE_GUI_VIEWER` in `CMakeLists.txt`.
- IDE configuration: `.vscode/settings.json` and `.vscode/launch.json`.
- Formatting rules: `.clang-format`.

## Platform Requirements

**Development:**
- A CMake-capable native toolchain with CUDA support.
- Local access to a vcpkg installation rooted at `$HOME/vcpkg_root` unless `CMAKE_TOOLCHAIN_FILE` is overridden.
- Local OptiX headers reachable under `NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64/include` or another path supplied to `OPTIX_INCLUDE_DIR`.

**Production / Execution:**
- Output binaries are written to `bin/` by `CMAKE_LIBRARY_OUTPUT_DIRECTORY` and `CMAKE_RUNTIME_OUTPUT_DIRECTORY` in `CMakeLists.txt`.
- GPU executables depend on CUDA runtime/driver libraries and OptiX at runtime.
- GUI execution additionally depends on an OpenGL-capable desktop environment for `render_realtime_viewer`.

---

*Stack analysis: 2026-04-19*
