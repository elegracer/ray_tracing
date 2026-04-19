# External Integrations

**Analysis Date:** 2026-04-19

## APIs & External Services

**GPU / Rendering SDKs:**
- NVIDIA OptiX - Realtime ray tracing and denoiser integration used by `src/realtime/gpu/optix_renderer.h`, `src/realtime/gpu/optix_renderer.cpp`, and `src/realtime/gpu/denoiser.cpp`.
  - SDK/Client: local headers from `NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64/` plus OptiX headers such as `optix.h` and `optix_stubs.h`.
  - Auth: Not applicable.
- CUDA Toolkit / Driver API - GPU memory, kernel launch, and runtime support in `src/realtime/gpu/optix_renderer.h`, `src/realtime/gpu/programs.cu`, and `CMakeLists.txt`.
  - SDK/Client: `CUDAToolkit` via `find_package(CUDAToolkit REQUIRED)` and linked targets `CUDA::cudart` / `CUDA::cuda_driver`.
  - Auth: Not applicable.
- OpenGL + GLFW + ImGui - Interactive viewer windowing, input, and immediate-mode UI in `utils/render_realtime_viewer.cpp`.
  - SDK/Client: `glfw3`, `imgui`, and `OpenGL` from `CMakeLists.txt`.
  - Auth: Not applicable.

**Network / Cloud Services:**
- Not detected. No HTTP clients, cloud SDKs, authentication providers, or remote APIs were found in `src/`, `utils/`, `tests/`, or `CMakeLists.txt`.

## Data Storage

**Databases:**
- None.
  - Connection: Not applicable.
  - Client: Not applicable.

**File Storage:**
- Local filesystem only.
- Scene definitions are loaded from `assets/scenes/**/scene.yaml` by `src/scene/scene_file_catalog.cpp`.
- Render outputs are written under caller-supplied directories such as `build/realtime-smoke` in `README.md` and `utils/render_realtime.cpp`.

**Caching:**
- No application-level cache service detected.
- Build-time compiler caching is optional through `ccache` in `CMakeLists.txt`.

## Authentication & Identity

**Auth Provider:**
- None.
  - Implementation: Not applicable.

## Monitoring & Observability

**Error Tracking:**
- None detected.

**Logs / Metrics:**
- CLI stdout/stderr logging via `fmt` in `utils/render_scene.cpp` and `utils/render_realtime.cpp`.
- Benchmark metrics are emitted as `benchmark_frames.csv` and `benchmark_summary.json` by `src/realtime/profiling/benchmark_report.cpp`.
- Verification scripts assert those artifacts in `cmake/VerifyRenderRealtimeProfiling.cmake` and `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`.

## CI/CD & Deployment

**Hosting:**
- Not applicable. The repo builds native local binaries into `bin/` via `CMakeLists.txt`.

**CI Pipeline:**
- Not detected in repository-owned files.
- Test orchestration is local `CTest` plus custom CMake script checks from `CMakeLists.txt`.

## Environment Configuration

**Required env vars / external paths:**
- `HOME` is used to derive the default `CMAKE_TOOLCHAIN_FILE` path in `CMakeLists.txt`.
- `VCPKG_ROOT` is used in the documented configure command in `README.md`.
- A CUDA compiler path is passed explicitly in the `README.md` example (`/usr/local/cuda-13.0/bin/nvcc`).
- No application secrets or service credentials were detected.

**Secrets location:**
- Not applicable.

## File Formats & Assets

**Scene description formats:**
- YAML scene files (`scene.yaml`) are parsed by `src/scene/yaml_scene_loader.cpp`.
- File-backed scenes live under `assets/scenes/`, including `assets/scenes/final_room/scene.yaml` and `assets/scenes/imported_obj_smoke/scene.yaml`.

**Geometry formats:**
- Wavefront OBJ with MTL sidecars is imported by `src/scene/obj_mtl_importer.cpp`.
- Example geometry assets live at `assets/scenes/imported_obj_smoke/models/triangle.obj` and `assets/scenes/imported_obj_smoke/models/triangle.mtl`.

**Image formats:**
- Input textures are loaded through OpenCV in `src/common/rtw_image.h` and `src/realtime/gpu/optix_renderer.cpp`.
- The offline and realtime CLIs write PNG outputs through OpenCV in `utils/render_scene.cpp` and `utils/render_realtime.cpp`.
- `render_scene` accepts output extensions such as `jpg`, `png`, and `bmp` in `utils/render_scene.cpp`.
- Builtin scenes reference `earthmap.jpg` in `src/scene/shared_scene_builders.cpp`; the file exists at the repo root as `earthmap.jpg`.

**Benchmark/report formats:**
- CSV and JSON benchmark artifacts are written by `src/realtime/profiling/benchmark_report.cpp`.
- The matrix benchmark helper in `utils/run_realtime_benchmark_matrix.sh` organizes runs by profile and camera count.

## Runtime Integrations

**Filesystem scene discovery:**
- `src/scene/scene_file_catalog.cpp` recursively scans for `scene.yaml` files and merges them with builtin scene definitions.
- `global_scene_file_catalog()` attempts an automatic scan of `assets/scenes` at startup in `src/scene/scene_file_catalog.cpp`.

**Asset rebasing and dependency tracking:**
- `src/scene/yaml_scene_loader.cpp` rebases relative image and OBJ paths against the scene directory and records them as dependencies.
- `src/scene/obj_mtl_importer.cpp` discovers referenced `.mtl` files and rebases texture paths relative to the material file location.

**Image loading fallback behavior:**
- `src/realtime/gpu/optix_renderer.cpp` retries missing texture loads with `../` and `../../` prefixes if the original path fails.

## External Tools

**Build / package tools:**
- `vcpkg` toolchain integration from `CMakeLists.txt` and `.vscode/settings.json`.
- `ccache` optional compiler launcher in `CMakeLists.txt`.

**Developer runtime tools:**
- VS Code workspace integration in `.vscode/settings.json` and `.vscode/launch.json`.
- Bash benchmark launcher in `utils/run_realtime_benchmark_matrix.sh`.

**Testing tools:**
- `CTest` registration in `CMakeLists.txt`.
- CMake script-based CLI assertions in `cmake/VerifyRenderRealtimeProfiling.cmake` and `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`.

## Webhooks & Callbacks

**Incoming:**
- None.

**Outgoing:**
- None.

---

*Integration audit: 2026-04-19*
