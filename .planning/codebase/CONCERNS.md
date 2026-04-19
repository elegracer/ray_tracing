# Codebase Concerns

**Analysis Date:** 2026-04-19

## Tech Debt

**Realtime denoise path is a placeholder, not a real OptiX denoiser:**
- Issue: `rt::OptixDenoiserWrapper` only clamps beauty values to `<= 1.0f` and recomputes luminance; it ignores albedo, normal, width, and height in `src/realtime/gpu/denoiser.cpp`.
- Files: `src/realtime/gpu/denoiser.cpp`, `utils/render_realtime.cpp`, `tests/test_realtime_pipeline.cpp`, `CMakeLists.txt`
- Impact: `denoise_ms` and `denoise_enabled` are reported as if denoising is a meaningful stage, but the path does not reduce noise or use OptiX denoiser features. Quality/perf results are easy to misread.
- Fix approach: either wire a real OptiX denoiser into `realtime_gpu` and keep the profiling fields, or rename/remove the feature so reports and profiles stop implying a capability that is not present.

**Build and test setup is pinned to one local CUDA/OptiX environment:**
- Issue: the default toolchain path is `$HOME/vcpkg_root`, default GPU architectures are `86 89`, OptiX headers are resolved from the vendored `NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64`, README commands hard-code `/usr/local/cuda-13.0/bin/nvcc`, and the renderer always calls `cudaSetDevice(0)`.
- Files: `CMakeLists.txt`, `README.md`, `src/realtime/gpu/optix_renderer.cpp`
- Impact: portability is low, CI setup is costly, multi-GPU systems cannot select a device, and non-`sm_86`/`sm_89` targets require local edits instead of configuration.
- Fix approach: expose toolchain, OptiX include root, CUDA arch list, and CUDA device as cache vars or CLI/env settings; gate GPU-only tests when the environment is unavailable.

**Scene registration is maintained in parallel tables and a large hand-built source file:**
- Issue: builtin scene metadata, CPU presets, realtime presets, and scene constructors are spread across `kSceneRegistry`, `kCpuPresetRegistry`, and `kRealtimePresetRegistry` inside a 760-line `src/scene/shared_scene_builders.cpp`.
- Files: `src/scene/shared_scene_builders.cpp`, `src/scene/scene_file_catalog.cpp`, `assets/scenes/`
- Impact: adding or changing a scene is easy to do partially. CPU/realtime support, labels, presets, and builder logic can drift because they are updated in separate places.
- Fix approach: consolidate builtin scene definitions into one data source or generate the registries from a single `SceneDefinition` representation.

## Known Bugs

**Invalid or missing realtime image textures fail silently:**
- Symptoms: image textures become empty/black instead of surfacing an error.
- Files: `src/realtime/gpu/optix_renderer.cpp`, `tests/test_yaml_scene_loader.cpp`
- Trigger: `cv::imread()` cannot decode the texture, or the working-directory fallback search still misses the file.
- Workaround: ensure texture files are readable from the current working directory or its parent directories; there is no explicit runtime error today.

**Startup scene-catalog failures are hidden behind builtin fallback:**
- Symptoms: broken file-backed scenes under `assets/scenes/` can disappear from the runtime catalog without a startup failure.
- Files: `src/scene/scene_file_catalog.cpp`, `tests/test_scene_file_catalog.cpp`
- Trigger: any exception while `global_scene_file_catalog()` scans `assets/scenes` during first use.
- Workaround: call `SceneFileCatalog::scan_directory()` directly or use viewer reload flows that surface errors instead of relying on the singleton startup path.

## Security Considerations

**Scene files can request arbitrary local file reads:**
- Risk: YAML scene definitions can load arbitrary local paths through `includes`, image texture `path`, and OBJ imports.
- Files: `src/scene/yaml_scene_loader.cpp`, `src/scene/obj_mtl_importer.cpp`, `src/scene/scene_file_catalog.cpp`
- Current mitigation: the project uses local files only; no remote fetch or credential handling is present.
- Recommendations: if scene files ever become untrusted input, restrict loads to an allowlisted root and reject absolute paths or `..` traversal before calling `YAML::LoadFile`, `cv::imread`, or OBJ import helpers.

## Performance Bottlenecks

**Each camera launch allocates and frees device-side launch params:**
- Problem: `launch_radiance_kernel()` does `cudaMalloc`, `cudaMemcpyAsync`, and `cudaFree` for `LaunchParams` on every camera render.
- Files: `src/realtime/gpu/programs.cu`
- Cause: launch parameters are not persisted in device memory across launches.
- Improvement path: keep one reusable device-side `LaunchParams` buffer per renderer/stream and update it in place.

**Host-side concurrency uses fresh `std::async` tasks every frame:**
- Problem: per-frame render and optional denoise stages both spin up `std::async` work items.
- Files: `src/realtime/gpu/renderer_pool.cpp`, `utils/render_realtime.cpp`
- Cause: neither render dispatch nor denoise postprocessing uses a persistent worker pool.
- Improvement path: replace per-frame `std::async` creation with long-lived worker threads or a bounded task executor tied to the fixed camera-count limit.

## Fragile Areas

**Large multi-responsibility files dominate critical paths:**
- Files: `src/realtime/gpu/programs.cu` (1060 lines), `src/realtime/gpu/optix_renderer.cpp` (850), `src/scene/shared_scene_builders.cpp` (760), `src/scene/yaml_scene_loader.cpp` (606), `utils/render_realtime_viewer.cpp` (620)
- Why fragile: rendering, scene IO, registry logic, and viewer UI each live in a single dense file, so small changes require understanding many unrelated responsibilities at once.
- Safe modification: split by responsibility first, then change behavior behind existing tests instead of editing these files monolithically.
- Test coverage: there are many smoke/regression tests, but they do not isolate every branch inside these files.

**The interactive viewer depends on deprecated fixed-function OpenGL:**
- Files: `utils/render_realtime_viewer.cpp`
- Why fragile: the viewer uses OpenGL 2.1, display lists, and immediate-mode drawing (`glBegin`, `glEnd`, `glGenLists`, `glCallList`), which is brittle on core-profile-only systems and hard to modernize incrementally.
- Safe modification: treat the viewer render path as replace-in-whole code; partial shader-era updates will fight the current immediate-mode assumptions.
- Test coverage: no automated test opens a GLFW window or validates the actual on-screen render loop.

## Scaling Limits

**Camera count is hard-limited to four across the stack:**
- Current capacity: `1..4` cameras.
- Limit: the limit is enforced in `src/realtime/gpu/renderer_pool.cpp`, `src/realtime/realtime_pipeline.cpp`, `src/realtime/realtime_scene_factory.cpp`, `utils/render_realtime.cpp`, and the fixed 2x2 viewer layout in `utils/render_realtime_viewer.cpp`.
- Scaling path: replace fixed-size camera assumptions with dynamic containers and decouple viewer layout from camera count before attempting `>4` cameras.

**Repository-local GPU dependencies and output directories do not scale well:**
- Current capacity: the vendored `NVIDIA-OptiX-SDK-9.1.0-linux64-x86_64` directory is about `83M`, and `CMAKE_RUNTIME_OUTPUT_DIRECTORY` / `CMAKE_LIBRARY_OUTPUT_DIRECTORY` write into repo-local `bin/`, which is `374M` in the current workspace.
- Limit: clones, worktrees, and CI caches get heavier, and local builds dirty a tracked directory layout.
- Scaling path: resolve OptiX from an external install/cache and move build outputs back under the active build directory instead of the repo root.

## Dependencies at Risk

**Vendored OptiX SDK 9.1.0:**
- Risk: the project depends on a pinned local SDK tree instead of a system/package-managed dependency.
- Impact: CUDA/driver/OptiX upgrades are harder to test incrementally, and the repo carries a large third-party payload even when only headers are needed.
- Migration plan: resolve OptiX through a configurable install root or package mechanism and keep only the minimal project-owned wrappers in the repository.

## Missing Critical Features

**There is no production-grade realtime denoiser despite denoise-facing profiles and reporting:**
- Problem: the code exposes denoise toggles and denoise timing/reporting, but the implementation is still a clamp stub.
- Blocks: trustworthy quality comparisons between `balanced` / `realtime` profiles and any attempt to use denoise-related benchmark numbers for renderer decisions.

## Test Coverage Gaps

**Realtime invalid-texture behavior is not asserted:**
- What's not tested: a realtime render with a missing or undecodable image texture.
- Files: `src/realtime/gpu/optix_renderer.cpp`, `tests/test_yaml_scene_loader.cpp`, `tests/test_realtime_texture_materials.cpp`
- Risk: broken textures can silently ship as black/default-looking output.
- Priority: High

**The singleton startup fallback path is not covered:**
- What's not tested: `global_scene_file_catalog()` swallowing a startup scan failure and returning builtin-only data.
- Files: `src/scene/scene_file_catalog.cpp`, `tests/test_scene_file_catalog.cpp`
- Risk: file-backed scene regressions can be masked during normal startup.
- Priority: Medium

**GPU correctness checks stay at smoke-test depth:**
- What's not tested: higher-resolution or per-pixel parity across the broader built-in scene catalog.
- Files: `tests/test_reference_vs_realtime.cpp`, `tests/test_optix_profiled_render.cpp`, `tests/test_realtime_texture_materials.cpp`
- Risk: scene-specific lighting, material, and precision regressions can pass while simple `32x32` and `64x64` fixtures still look acceptable.
- Priority: Medium

**The GLFW/ImGui viewer loop has no automated end-to-end coverage:**
- What's not tested: window creation, OpenGL upload/draw, keyboard/mouse interaction, and frame presentation in `render_realtime_viewer`.
- Files: `utils/render_realtime_viewer.cpp`, `tests/test_viewer_scene_reload.cpp`, `tests/test_viewer_scene_switch_controller.cpp`, `tests/test_viewer_quality_controller.cpp`
- Risk: UI/render-loop regressions are caught only by manual runs on a machine with a compatible graphics stack.
- Priority: Medium

---

*Concerns audit: 2026-04-19*
