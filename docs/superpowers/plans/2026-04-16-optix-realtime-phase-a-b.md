# OptiX Realtime Phase A/B Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace placeholder GPU radiance with a real OptiX path tracing baseline for `1..4` cameras, then layer profile-driven accumulation, denoising, and timing output that can move toward `4x 640x480 @ 30fps`.

**Architecture:** Keep `SceneDescription`, `CameraRig`, and `RenderProfile` as the stable input boundary. Build a real core integrator under `src/realtime/gpu/` that owns path tracing and auxiliary buffers, then keep `RealtimePipeline` and `render_realtime` responsible for per-camera orchestration, profile selection, accumulation, denoising, and benchmark reporting.

**Tech Stack:** C++23, CUDA, OptiX 9.1, Eigen, OpenCV, TBB, CMake, CTest

---

## File Structure

- Modify: `src/realtime/gpu/launch_params.h`
  - Add GPU-facing launch and surface structs for radiance, normals, albedo, depth, packed scene data, and profile flags.
- Modify: `src/realtime/gpu/optix_renderer.h`
  - Expose the real radiance path as the primary renderer contract while keeping debug rendering intact.
- Modify: `src/realtime/gpu/optix_renderer.cpp`
  - Replace placeholder radiance buffer creation with real device allocation, upload, launch, and download.
- Modify: `src/realtime/gpu/programs.cu`
  - Add loop-based `radiance` and `shadow` path tracing kernels/programs while preserving the direction-debug path.
- Modify: `src/realtime/gpu/device_math.h`
  - Add the minimal device-side vector/math helpers needed by the real integrator.
- Modify: `src/realtime/gpu/denoiser.h`
  - Add profile-aware denoise entry points if the wrapper needs profile flags or aux-buffer assumptions.
- Modify: `src/realtime/gpu/denoiser.cpp`
  - Replace the current clamp stub with a real OptiX denoiser path, or at minimum make the wrapper honor profile gating and real aux buffers.
- Modify: `src/realtime/realtime_pipeline.h`
  - Add profile-aware frame rendering entry points and per-camera history ownership.
- Modify: `src/realtime/realtime_pipeline.cpp`
  - Route real radiance frames through accumulation/reset/denoise rather than smoke-only history counters.
- Modify: `src/realtime/render_profile.h`
  - Add explicit `quality`, `balanced`, and `realtime` constructors plus any flags needed by the integrator and pipeline.
- Modify: `utils/render_realtime.cpp`
  - Add profile selection, reproducible benchmark reporting, and per-stage timing output.
- Modify: `tests/test_optix_path_trace.cpp`
  - Turn the current non-black smoke check into a real-material / aux-buffer smoke test.
- Modify: `tests/test_reference_vs_realtime.cpp`
  - Keep CPU/GPU statistical comparison valid once placeholder output is gone.
- Modify: `tests/test_realtime_pipeline.cpp`
  - Assert profile-aware accumulation/reset behavior using real radiance frames.
- Create: `tests/test_optix_materials_aux.cpp`
  - Validate `Lambertian`, `Metal`, `Dielectric`, `DiffuseLight`, plus `beauty / normal / albedo / depth`.
- Create: `tests/test_render_profile.cpp`
  - Lock the defaults and downgrade knobs for `quality`, `balanced`, and `realtime`.
- Modify: `README.md`
  - Document the new profile-driven realtime CLI and verification commands.

## Task 1: Lock the Profile Contract

**Files:**
- Modify: `src/realtime/render_profile.h`
- Create: `tests/test_render_profile.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing profile test**

```cpp
// tests/test_render_profile.cpp
#include "realtime/render_profile.h"
#include "test_support.h"

int main() {
    const rt::RenderProfile quality = rt::RenderProfile::quality();
    expect_true(quality.samples_per_pixel >= 4, "quality spp");
    expect_true(quality.max_bounces >= 6, "quality bounce budget");
    expect_true(!quality.enable_denoise, "quality denoise off by default");

    const rt::RenderProfile balanced = rt::RenderProfile::balanced();
    expect_true(balanced.samples_per_pixel <= quality.samples_per_pixel, "balanced spp not above quality");
    expect_true(balanced.enable_denoise, "balanced denoise enabled");

    const rt::RenderProfile realtime = rt::RenderProfile::realtime();
    expect_true(realtime.samples_per_pixel <= balanced.samples_per_pixel, "realtime spp floor");
    expect_true(realtime.max_bounces <= balanced.max_bounces, "realtime lower bounce budget");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings --target test_render_profile -j 4`

Expected: FAIL with a message such as `No rule to make target 'test_render_profile'` or missing `RenderProfile::quality()`

- [ ] **Step 3: Add the profile factories and CTest target**

```cpp
// src/realtime/render_profile.h
struct RenderProfile {
    int samples_per_pixel = 1;
    int max_bounces = 4;
    bool enable_denoise = true;
    int rr_start_bounce = 3;
    double accumulation_reset_rotation_deg = 2.0;
    double accumulation_reset_translation = 0.05;

    static RenderProfile quality() {
        return RenderProfile{
            .samples_per_pixel = 4,
            .max_bounces = 8,
            .enable_denoise = false,
            .rr_start_bounce = 6,
            .accumulation_reset_rotation_deg = 0.5,
            .accumulation_reset_translation = 0.01,
        };
    }

    static RenderProfile balanced() {
        return RenderProfile{
            .samples_per_pixel = 2,
            .max_bounces = 4,
            .enable_denoise = true,
            .rr_start_bounce = 3,
            .accumulation_reset_rotation_deg = 1.0,
            .accumulation_reset_translation = 0.02,
        };
    }

    static RenderProfile realtime() {
        return RenderProfile{
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .enable_denoise = true,
            .rr_start_bounce = 2,
            .accumulation_reset_rotation_deg = 2.0,
            .accumulation_reset_translation = 0.05,
        };
    }

    static RenderProfile realtime_default() { return balanced(); }
};
```

```cmake
# CMakeLists.txt
add_executable(test_render_profile)
target_sources(test_render_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_profile.cpp)
target_link_libraries(test_render_profile PRIVATE core)
add_test(NAME test_render_profile COMMAND test_render_profile)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_profile -V`

Expected: PASS with `100% tests passed`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/render_profile.h tests/test_render_profile.cpp
git commit -m "test: lock realtime render profiles"
```

## Task 2: Add a Failing Real-Radiance Semantics Test

**Files:**
- Create: `tests/test_optix_materials_aux.cpp`
- Modify: `CMakeLists.txt`

This task must produce a true red test on the current placeholder renderer. If the basic
materials/aux assertions are not sufficient to fail under the current placeholder behavior,
add one focused aux-buffer semantics assertion that the placeholder path cannot satisfy.

- [ ] **Step 1: Write the failing materials/aux test**

```cpp
// tests/test_optix_materials_aux.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <cmath>
#include <vector>

namespace {

bool has_variation(const std::vector<float>& values, float epsilon) {
    if (values.empty()) {
        return false;
    }
    const float first = values.front();
    for (float value : values) {
        if (std::abs(value - first) > epsilon) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial{Eigen::Vector3d{8.0, 8.0, 8.0}});
    const int diffuse = scene.add_material(rt::LambertianMaterial{Eigen::Vector3d{0.7, 0.2, 0.2}});
    const int metal = scene.add_material(rt::MetalMaterial{Eigen::Vector3d{0.9, 0.9, 0.9}, 0.02});
    const int glass = scene.add_material(rt::DielectricMaterial{1.5});

    scene.add_quad(rt::QuadPrimitive{light,
        Eigen::Vector3d{-1.0, 1.5, -3.0},
        Eigen::Vector3d{2.0, 0.0, 0.0},
        Eigen::Vector3d{0.0, 0.0, -2.0}, false});
    scene.add_sphere(rt::SpherePrimitive{diffuse, Eigen::Vector3d{-0.8, -0.2, -3.5}, 0.5, false});
    scene.add_sphere(rt::SpherePrimitive{metal, Eigen::Vector3d{ 0.0, -0.2, -3.5}, 0.5, false});
    scene.add_sphere(rt::SpherePrimitive{glass, Eigen::Vector3d{ 0.8, -0.2, -3.5}, 0.5, false});

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params{220.0, 220.0, 48.0, 48.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 96, 96);

    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(
        scene.pack(), rig.pack(), rt::RenderProfile::quality(), 0);

    expect_true(frame.average_luminance > 0.02, "beauty is lit");
    expect_true(!frame.normal_rgba.empty(), "normal buffer present");
    expect_true(!frame.albedo_rgba.empty(), "albedo buffer present");
    expect_true(!frame.depth.empty(), "depth buffer present");
    expect_true(frame.normal_rgba != frame.beauty_rgba, "normal differs from beauty");
    expect_true(has_variation(frame.depth, 1e-6f), "depth varies across the frame");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings --target test_optix_materials_aux -j 4`

Expected: FAIL because the target does not exist yet or because the placeholder renderer still produces semantically wrong aux output, for example a uniform `depth` buffer

- [ ] **Step 3: Add the new target**

```cmake
# CMakeLists.txt
add_executable(test_optix_materials_aux)
target_sources(test_optix_materials_aux
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_materials_aux.cpp
)
target_link_libraries(test_optix_materials_aux PRIVATE realtime_gpu)
add_test(NAME test_optix_materials_aux COMMAND test_optix_materials_aux)
```

- [ ] **Step 4: Re-run and confirm it still fails for the correct reason**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_optix_materials_aux -V`

Expected: FAIL inside the test body, not at configure/build time

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/test_optix_materials_aux.cpp
git commit -m "test: add optix material aux coverage"
```

## Task 3: Expand GPU Launch Data and Host Upload Plumbing

**Files:**
- Modify: `src/realtime/gpu/launch_params.h`
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`

- [ ] **Step 1: Write the failing host-side plumbing change against the new test**

```cpp
// src/realtime/gpu/launch_params.h
struct DeviceFrameBuffers {
    float4* beauty = nullptr;
    float4* normal = nullptr;
    float4* albedo = nullptr;
    float* depth = nullptr;
};

struct DeviceSceneView {
    PackedSphere* spheres = nullptr;
    PackedQuad* quads = nullptr;
    MaterialSample* materials = nullptr;
    int sphere_count = 0;
    int quad_count = 0;
    int material_count = 0;
};

struct LaunchParams {
    DeviceFrameBuffers frame{};
    DeviceSceneView scene{};
    PackedCameraRig rig{};
    int camera_index = 0;
    int width = 0;
    int height = 0;
    int samples_per_pixel = 1;
    int max_bounces = 4;
    int rr_start_bounce = 3;
    int mode = 0;
};
```

- [ ] **Step 2: Build `test_optix_materials_aux` and observe the compile failure**

Run: `VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings --target test_optix_materials_aux -j 4`

Expected: FAIL because `OptixRenderer` still assumes placeholder `std::vector<float>` downloads and does not allocate/populate `LaunchParams::frame`

- [ ] **Step 3: Add the minimal host-side device resource ownership**

```cpp
// src/realtime/gpu/optix_renderer.h
class OptixRenderer {
   private:
    void allocate_frame_buffers(int width, int height);
    void upload_scene(const PackedScene& scene);
    void free_device_resources();

    DeviceFrameBuffers device_frame_{};
    PackedSphere* device_spheres_ = nullptr;
    PackedQuad* device_quads_ = nullptr;
    MaterialSample* device_materials_ = nullptr;
    int allocated_width_ = 0;
    int allocated_height_ = 0;
};
```

```cpp
// src/realtime/gpu/optix_renderer.cpp
void OptixRenderer::allocate_frame_buffers(int width, int height) {
    if (allocated_width_ == width && allocated_height_ == height) {
        return;
    }
    free_device_resources();
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.beauty), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.normal), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.albedo), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.depth), pixel_count * sizeof(float)));
    allocated_width_ = width;
    allocated_height_ = height;
}
```

- [ ] **Step 4: Rebuild and confirm the new failure moves into device/integrator code**

Run: `VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings --target test_optix_materials_aux -j 4`

Expected: FAIL later than before, with the remaining error now centered on `programs.cu` / missing real radiance behavior rather than missing host-side resources

- [ ] **Step 5: Commit**

```bash
git add src/realtime/gpu/launch_params.h src/realtime/gpu/optix_renderer.h src/realtime/gpu/optix_renderer.cpp
git commit -m "feat: add optix launch buffer plumbing"
```

## Task 4: Implement Loop-Based Radiance and Shadow Paths

**Files:**
- Modify: `src/realtime/gpu/device_math.h`
- Modify: `src/realtime/gpu/programs.cu`
- Modify: `src/realtime/gpu/optix_renderer.cpp`

- [ ] **Step 1: Replace the direction-only kernel assumptions with a failing real-radiance implementation stub**

```cpp
// src/realtime/gpu/programs.cu
struct PathState {
    float3 throughput;
    float3 radiance;
    Ray ray;
    bool alive;
};

__device__ PathState trace_primary_ray(const LaunchParams& params, int x, int y);
__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit, PathState& state);
__device__ void sample_bsdf(const LaunchParams& params, const HitInfo& hit, curandState& rng, PathState& state);
```

- [ ] **Step 2: Build and capture the expected failure before the real implementation lands**

Run: `VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings --target test_optix_materials_aux -j 4`

Expected: FAIL with undefined device functions or missing hit-path behavior

- [ ] **Step 3: Add the minimal real integrator**

```cpp
// src/realtime/gpu/programs.cu
__global__ void radiance_kernel(LaunchParams params) {
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= params.width || y >= params.height) {
        return;
    }

    const int pixel_index = y * params.width + x;
    float3 beauty = make_float3(0.0f);
    float3 normal = make_float3(0.0f);
    float3 albedo = make_float3(0.0f);
    float depth = 0.0f;

    for (int sample = 0; sample < params.samples_per_pixel; ++sample) {
        PathState state = trace_primary_ray(params, x, y);
        for (int bounce = 0; bounce < params.max_bounces && state.alive; ++bounce) {
            HitInfo hit = intersect_scene(params.scene, state.ray);
            if (!hit.hit) {
                state.radiance += state.throughput * make_float3(0.0f);
                state.alive = false;
                break;
            }
            if (bounce == 0) {
                normal = hit.shading_normal;
                albedo = hit.base_color;
                depth = hit.t;
            }
            state.radiance += state.throughput * hit.emission;
            accumulate_direct_light(params, hit, state);
            sample_bsdf(params, hit, rng_for(pixel_index, sample), state);
            if (bounce >= params.rr_start_bounce) {
                apply_russian_roulette(state, rng_for(pixel_index, sample + bounce + 1));
            }
        }
        beauty += state.radiance;
    }

    store_output(params.frame, pixel_index, beauty / params.samples_per_pixel, normal, albedo, depth);
}
```

- [ ] **Step 4: Run the materials/aux and path-trace tests**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_optix_materials_aux|test_optix_path_trace' -V`

Expected: both PASS

- [ ] **Step 5: Commit**

```bash
git add src/realtime/gpu/device_math.h src/realtime/gpu/programs.cu src/realtime/gpu/optix_renderer.cpp tests/test_optix_path_trace.cpp
git commit -m "feat: add optix radiance and shadow paths"
```

## Task 5: Download Real Aux Buffers and Keep the CPU/GPU Baseline Honest

**Files:**
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Modify: `tests/test_reference_vs_realtime.cpp`

- [ ] **Step 1: Tighten the failing CPU/GPU comparison around real radiance output**

```cpp
// tests/test_reference_vs_realtime.cpp
expect_true(gpu.average_luminance > 0.02, "gpu frame is lit");
expect_true(!gpu.normal_rgba.empty(), "gpu normal buffer present");
expect_true(!gpu.albedo_rgba.empty(), "gpu albedo buffer present");
expect_true(!gpu.depth.empty(), "gpu depth buffer present");
expect_near(gpu.average_luminance, cpu_mean_luminance, 0.08, "mean luminance agreement");
```

- [ ] **Step 2: Run the comparison test and verify it fails**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_reference_vs_realtime -V`

Expected: FAIL until the host download path copies real device data instead of synthetic vectors

- [ ] **Step 3: Replace placeholder downloads with real device copies**

```cpp
// src/realtime/gpu/optix_renderer.cpp
std::vector<float> OptixRenderer::download_beauty(int camera_index) const {
    (void)camera_index;
    std::vector<float4> host_rgba(static_cast<std::size_t>(last_width_ * last_height_));
    RT_CUDA_CHECK(cudaMemcpy(host_rgba.data(), device_frame_.beauty,
        host_rgba.size() * sizeof(float4), cudaMemcpyDeviceToHost));
    std::vector<float> out(host_rgba.size() * 4U, 0.0f);
    for (std::size_t i = 0; i < host_rgba.size(); ++i) {
        out[4 * i + 0] = host_rgba[i].x;
        out[4 * i + 1] = host_rgba[i].y;
        out[4 * i + 2] = host_rgba[i].z;
        out[4 * i + 3] = host_rgba[i].w;
    }
    return out;
}
```

- [ ] **Step 4: Run the baseline tests**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_optix_materials_aux|test_reference_vs_realtime' -V`

Expected: PASS with `0 tests failed`

- [ ] **Step 5: Commit**

```bash
git add src/realtime/gpu/optix_renderer.cpp tests/test_reference_vs_realtime.cpp
git commit -m "test: validate real optix radiance against cpu baseline"
```

## Task 6: Route Real Frames Through the Realtime Pipeline

**Files:**
- Modify: `src/realtime/realtime_pipeline.h`
- Modify: `src/realtime/realtime_pipeline.cpp`
- Modify: `src/realtime/gpu/denoiser.h`
- Modify: `src/realtime/gpu/denoiser.cpp`
- Modify: `tests/test_realtime_pipeline.cpp`

- [ ] **Step 1: Write the failing profile-aware pipeline test**

```cpp
// tests/test_realtime_pipeline.cpp
int main() {
    rt::RealtimePipeline pipeline;
    const rt::RealtimeFrameSet first = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_true(first.frames[0].history_length == 1, "camera 0 history starts");
    expect_true(first.frames[0].radiance.average_luminance > 0.01, "camera 0 real radiance");

    const rt::RealtimeFrameSet second = pipeline.render_profiled_smoke_frame(2, rt::RenderProfile::balanced());
    expect_true(second.frames[0].history_length == 2, "camera 0 accumulates");

    const rt::RealtimeFrameSet reset = pipeline.render_profiled_smoke_frame_with_pose_jump(2, rt::RenderProfile::balanced());
    expect_true(reset.frames[0].history_length == 1, "camera 0 reset");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_realtime_pipeline -V`

Expected: FAIL because the pipeline currently only returns history counters and not real radiance frames

- [ ] **Step 3: Add the minimal profile-aware pipeline and denoiser gating**

```cpp
// src/realtime/realtime_pipeline.h
class RealtimePipeline {
   public:
    RealtimeFrameSet render_profiled_smoke_frame(int active_cameras, const RenderProfile& profile);
    RealtimeFrameSet render_profiled_smoke_frame_with_pose_jump(int active_cameras, const RenderProfile& profile);
};
```

```cpp
// src/realtime/realtime_pipeline.cpp
RealtimeFrameSet RealtimePipeline::render_profiled_smoke_frame(int active_cameras, const RenderProfile& profile) {
    validate_active_cameras(active_cameras);
    RealtimeFrameSet out{};
    out.frames.resize(static_cast<std::size_t>(active_cameras));
    const PackedScene scene = build_smoke_scene().pack();
    const PackedCameraRig rig = build_smoke_rig(active_cameras).pack();
    for (int i = 0; i < active_cameras; ++i) {
        history_lengths_[i] += 1;
        out.frames[static_cast<std::size_t>(i)].history_length = history_lengths_[i];
        out.frames[static_cast<std::size_t>(i)].radiance = renderer_.render_radiance(scene, rig, profile, i);
        if (profile.enable_denoise) {
            denoiser_.run(out.frames[static_cast<std::size_t>(i)].radiance);
        }
    }
    return out;
}
```

- [ ] **Step 4: Run the pipeline and path-trace regression tests**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_realtime_pipeline|test_optix_path_trace|test_optix_materials_aux' -V`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/realtime/realtime_pipeline.h src/realtime/realtime_pipeline.cpp src/realtime/gpu/denoiser.h src/realtime/gpu/denoiser.cpp tests/test_realtime_pipeline.cpp
git commit -m "feat: route real radiance through realtime pipeline"
```

## Task 7: Add Profile-Aware CLI Timing and Benchmark Output

**Files:**
- Modify: `utils/render_realtime.cpp`
- Modify: `README.md`

- [ ] **Step 1: Write the failing CLI benchmark expectation as a manual smoke target**

```bash
./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke
```

Expected before implementation: FAIL with an argparse error such as `unknown argument --profile`

- [ ] **Step 2: Run the command and verify it fails**

Run: `./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke`

Expected: non-zero exit due to missing `--profile`

- [ ] **Step 3: Add profile selection and stage timing output**

```cpp
// utils/render_realtime.cpp
std::string profile_name = "balanced";
program.add_argument("--profile")
    .help("quality, balanced, or realtime")
    .choices("quality", "balanced", "realtime")
    .default_value(profile_name)
    .store_into(profile_name);

const rt::RenderProfile profile = [&]() {
    if (profile_name == "quality") return rt::RenderProfile::quality();
    if (profile_name == "realtime") return rt::RenderProfile::realtime();
    return rt::RenderProfile::balanced();
}();

fmt::print("summary profile={} frames={} cameras={} resolution={}x{} avg_frame_ms={:.3f} fps={:.2f} output_dir={}\n",
    profile_name, frames, camera_count, kDefaultWidth, kDefaultHeight, avg_frame_ms, fps, output_path.string());
```

```md
<!-- README.md -->
./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke
```

- [ ] **Step 4: Re-run the CLI smoke**

Run: `./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke`

Expected: exit `0`, per-frame timing lines, and a summary line containing `profile=realtime`

- [ ] **Step 5: Commit**

```bash
git add utils/render_realtime.cpp README.md
git commit -m "feat: add realtime profile benchmark output"
```

## Task 8: Final Verification Pass

**Files:**
- Modify: `README.md` if the verification commands changed while implementing

- [ ] **Step 1: Build the full target set**

Run:

```bash
VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings \
  --target render_realtime test_render_profile test_optix_direction test_optix_path_trace \
           test_optix_materials_aux test_realtime_pipeline test_reference_vs_realtime -j 4
```

Expected: all targets build successfully

- [ ] **Step 2: Run the full focused test suite**

Run:

```bash
ctest --test-dir build-clang-vcpkg-settings -R 'test_render_profile|test_optix_direction|test_optix_path_trace|test_optix_materials_aux|test_realtime_pipeline|test_reference_vs_realtime' -V
```

Expected: `100% tests passed`

- [ ] **Step 3: Run the realtime CLI benchmark smoke**

Run:

```bash
./bin/render_realtime --camera-count 4 --frames 2 --profile balanced --output-dir build/realtime-smoke
./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke
```

Expected: both commands exit `0`, write PNGs, and print `summary profile=...`

- [ ] **Step 4: Check the worktree state**

Run: `git status --short`

Expected: clean working tree

- [ ] **Step 5: Verify the smoke outputs were written**

Run:

```bash
find build/realtime-smoke -maxdepth 1 -type f | sort
```

Expected: per-frame, per-camera PNG files such as `build/realtime-smoke/frame_0000_cam_0.png`

## Self-Review

- Spec coverage:
  - real OptiX radiance baseline: Tasks 2-5
  - `1..4` camera support: Tasks 4-6
  - profile-driven quality/balanced/realtime modes: Tasks 1, 6, 7
  - CPU/GPU statistical comparison: Task 5
  - benchmark/timing output: Tasks 7-8
- Placeholder scan:
  - no red-flag placeholders remain
  - every test/build step has an exact command and expected failure/pass mode
- Type consistency:
  - `RenderProfile::quality/balanced/realtime` is used consistently
  - `render_profiled_smoke_frame` and `render_profiled_smoke_frame_with_pose_jump` are the pipeline names used throughout
