# CUDA Realtime Host Overhead Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce `host_overhead_ms` in the CUDA realtime benchmark path by reusing renderer state, dispatching cameras concurrently, and parallelizing bounded host-side post-processing without changing output semantics or profiling schema.

**Architecture:** Keep `OptixRenderer` responsible for one camera executor with owned CUDA resources, add a small `RendererPool` helper in `realtime_gpu` to manage up to four renderer instances, and keep `render_realtime` as the multi-camera scheduler and profiling sink. Reuse uploaded scene state and pinned host staging inside each renderer, then overlap camera execution with one renderer per camera and bounded host-side denoise tasks.

**Tech Stack:** C++23, CUDA runtime, OptiX, `fmt`, `argparse`, `std::future`, existing CTest/CMake CLI verification.

---

## File Structure

- Modify: `src/realtime/gpu/optix_renderer.h`
  - Add explicit scene-preparation and prepared-render APIs.
  - Add owned pinned host staging state to the renderer.
- Modify: `src/realtime/gpu/optix_renderer.cpp`
  - Reuse scene/device buffers and pinned host staging across renders.
  - Keep the existing convenience render API as a wrapper for compatibility.
- Create: `src/realtime/gpu/renderer_pool.h`
  - Declare the small pool abstraction that owns `1..4` renderer instances and returns ordered per-camera results.
- Create: `src/realtime/gpu/renderer_pool.cpp`
  - Implement bounded multi-camera dispatch with one async task per active camera.
- Modify: `CMakeLists.txt`
  - Compile the new pool helper into `realtime_gpu`.
  - Add tests for prepared rendering and multi-camera scheduling.
- Create: `tests/test_optix_prepared_scene.cpp`
  - Verify explicit prepare + prepared render APIs and persistent staging do not change output semantics.
- Create: `tests/test_renderer_pool.cpp`
  - Verify concurrent pool output matches the sequential baseline for `1` and `4` cameras.
- Modify: `utils/render_realtime.cpp`
  - Replace the single-renderer loop with `RendererPool`.
  - Parallelize denoise with bounded `std::async`.
  - Preserve profiling fields and artifact schema.
- Create: `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`
  - Verify the aggressive `4` camera benchmark path still emits valid artifacts and ordered per-camera data.
- Modify: `README.md`
  - Update benchmark commands only if execution details change.

## Task 1: Add Prepared Scene and Staging Reuse to `OptixRenderer`

**Files:**
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Create: `tests/test_optix_prepared_scene.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing prepared-scene test**

```cpp
// tests/test_optix_prepared_scene.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

namespace {

void expect_vector_near(const std::vector<float>& actual, const std::vector<float>& expected, double tol,
    const std::string& label) {
    expect_true(actual.size() == expected.size(), label + " size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        expect_near(actual[i], expected[i], tol, label + " value[" + std::to_string(i) + "]");
    }
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {0.0, 0.0, -1.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-0.75, 1.25, -1.5},
        Eigen::Vector3d {1.5, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.5},
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 32, 32);

    const rt::PackedScene packed_scene = scene.pack();
    const rt::PackedCameraRig packed_rig = rig.pack();
    const rt::RenderProfile profile = rt::RenderProfile::realtime();

    rt::OptixRenderer renderer;
    renderer.prepare_scene(packed_scene);
    const rt::ProfiledRadianceFrame first = renderer.render_prepared_radiance(packed_rig, profile, 0);
    const rt::ProfiledRadianceFrame second = renderer.render_prepared_radiance(packed_rig, profile, 0);
    const rt::ProfiledRadianceFrame fallback = renderer.render_radiance_profiled(packed_scene, packed_rig, profile, 0);

    expect_vector_near(first.frame.beauty_rgba, second.frame.beauty_rgba, 1e-6, "prepared repeat beauty");
    expect_vector_near(first.frame.normal_rgba, second.frame.normal_rgba, 1e-6, "prepared repeat normal");
    expect_vector_near(first.frame.albedo_rgba, second.frame.albedo_rgba, 1e-6, "prepared repeat albedo");
    expect_vector_near(first.frame.depth, second.frame.depth, 1e-6, "prepared repeat depth");
    expect_vector_near(first.frame.beauty_rgba, fallback.frame.beauty_rgba, 1e-6, "prepared fallback beauty");
    expect_near(first.frame.average_luminance, fallback.frame.average_luminance, 1e-9, "prepared fallback luminance");
    expect_true(first.timing.render_ms >= 0.0f, "prepared render timing non-negative");
    expect_true(first.timing.download_ms >= 0.0f, "prepared download timing non-negative");
    return 0;
}
```

- [ ] **Step 2: Wire the test into CMake and verify the red phase**

```cmake
# CMakeLists.txt
add_executable(test_optix_prepared_scene)
target_sources(test_optix_prepared_scene
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_prepared_scene.cpp
)
target_link_libraries(test_optix_prepared_scene PRIVATE realtime_gpu)
add_test(NAME test_optix_prepared_scene COMMAND test_optix_prepared_scene)
```

Run: `cmake --build build-clang-vcpkg-settings --target test_optix_prepared_scene -j 4`  
Expected: FAIL because `prepare_scene()` and `render_prepared_radiance()` do not exist yet

- [ ] **Step 3: Add the explicit prepared-render API**

```cpp
// src/realtime/gpu/optix_renderer.h
struct HostFrameStaging {
    float4* beauty = nullptr;
    float4* normal = nullptr;
    float4* albedo = nullptr;
    float* depth = nullptr;
};

class OptixRenderer {
   public:
    void prepare_scene(const PackedScene& scene);
    ProfiledRadianceFrame render_prepared_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index);

   private:
    void allocate_host_staging(int width, int height);
    void free_host_staging();
    bool scene_is_prepared_ = false;
    HostFrameStaging host_stage_{};
    int staged_width_ = 0;
    int staged_height_ = 0;
};
```

```cpp
// src/realtime/gpu/optix_renderer.cpp
void OptixRenderer::prepare_scene(const PackedScene& scene) {
    upload_scene(scene);
    build_or_refit_accels(scene);
    scene_is_prepared_ = true;
}

ProfiledRadianceFrame OptixRenderer::render_prepared_radiance(
    const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    if (!scene_is_prepared_) {
        throw std::runtime_error("render_prepared_radiance requires prepare_scene first");
    }
    validate_radiance_request(rig, camera_index);
    ProfiledRadianceFrame profiled {};
    launch_radiance(rig, profile, camera_index, &profiled.timing);
    profiled.frame = download_radiance_frame_profiled(camera_index, &profiled.timing);
    return profiled;
}

ProfiledRadianceFrame OptixRenderer::render_radiance_profiled(
    const PackedScene& scene, const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    prepare_scene(scene);
    return render_prepared_radiance(rig, profile, camera_index);
}
```

- [ ] **Step 4: Replace per-call pinned allocation with persistent staging**

```cpp
// src/realtime/gpu/optix_renderer.cpp
void OptixRenderer::allocate_host_staging(int width, int height) {
    if (staged_width_ == width && staged_height_ == height) {
        return;
    }
    free_host_staging();
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.beauty), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.normal), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.albedo), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.depth), pixel_count * sizeof(float)));
    staged_width_ = width;
    staged_height_ = height;
}

void OptixRenderer::free_host_staging() {
    free_host_ptr(host_stage_.beauty);
    free_host_ptr(host_stage_.normal);
    free_host_ptr(host_stage_.albedo);
    free_host_ptr(host_stage_.depth);
    host_stage_ = HostFrameStaging {};
    staged_width_ = 0;
    staged_height_ = 0;
}
```

```cpp
// src/realtime/gpu/optix_renderer.cpp inside download_radiance_frame_profiled()
allocate_host_staging(frame.width, frame.height);
RT_CUDA_CHECK(cudaMemcpyAsync(
    host_stage_.beauty, device_frame_.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
RT_CUDA_CHECK(cudaMemcpyAsync(
    host_stage_.normal, device_frame_.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
RT_CUDA_CHECK(cudaMemcpyAsync(
    host_stage_.albedo, device_frame_.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
RT_CUDA_CHECK(cudaMemcpyAsync(
    host_stage_.depth, device_frame_.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost, stream_));

frame.beauty_rgba = unpack_rgba_from_float4(host_stage_.beauty, pixel_count);
frame.normal_rgba = unpack_rgba_from_float4(host_stage_.normal, pixel_count);
frame.albedo_rgba = unpack_rgba_from_float4(host_stage_.albedo, pixel_count);
frame.depth.assign(host_stage_.depth, host_stage_.depth + pixel_count);
```

- [ ] **Step 5: Free staging on destruction and verify green**

```cpp
// src/realtime/gpu/optix_renderer.cpp
OptixRenderer::~OptixRenderer() {
    free_host_staging();
    free_device_resources();
    if (optix_context_ != nullptr) {
        optixDeviceContextDestroy(optix_context_);
    }
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
    }
}
```

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_optix_prepared_scene|test_optix_profiled_render' -V`  
Expected: PASS with prepared and fallback paths agreeing

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt \
  src/realtime/gpu/optix_renderer.h \
  src/realtime/gpu/optix_renderer.cpp \
  tests/test_optix_prepared_scene.cpp
git commit -m "feat: reuse prepared optix scene state"
```

## Task 2: Add a Concurrent Renderer Pool

**Files:**
- Create: `src/realtime/gpu/renderer_pool.h`
- Create: `src/realtime/gpu/renderer_pool.cpp`
- Create: `tests/test_renderer_pool.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing renderer-pool test**

```cpp
// tests/test_renderer_pool.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/gpu/renderer_pool.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

namespace {

void expect_vector_near(const std::vector<float>& actual, const std::vector<float>& expected, double tol,
    const std::string& label) {
    expect_true(actual.size() == expected.size(), label + " size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        expect_near(actual[i], expected[i], tol, label + " value[" + std::to_string(i) + "]");
    }
}

rt::SceneDescription make_scene() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {0.0, 0.0, -1.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-0.75, 1.25, -1.5},
        Eigen::Vector3d {1.5, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.5},
        false,
    });
    return scene;
}

rt::CameraRig make_rig(int camera_count) {
    rt::CameraRig rig;
    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = Eigen::Vector3d {0.03 * static_cast<double>(i), 0.0, 0.0};
        rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
            T_bc, 32, 32);
    }
    return rig;
}

}  // namespace

int main() {
    const rt::PackedScene packed_scene = make_scene().pack();
    const rt::PackedCameraRig packed_rig = make_rig(4).pack();
    const rt::RenderProfile profile = rt::RenderProfile::realtime();

    rt::RendererPool pool(4);
    pool.prepare_scene(packed_scene);
    const std::vector<rt::CameraRenderResult> pooled = pool.render_frame(packed_rig, profile, 4);

    expect_true(pooled.size() == 4, "pooled camera count");
    for (int i = 0; i < 4; ++i) {
        expect_true(pooled[static_cast<std::size_t>(i)].camera_index == i, "pooled camera ordering");
        rt::OptixRenderer baseline;
        baseline.prepare_scene(packed_scene);
        const rt::ProfiledRadianceFrame expected = baseline.render_prepared_radiance(packed_rig, profile, i);
        expect_vector_near(pooled[static_cast<std::size_t>(i)].profiled.frame.beauty_rgba,
            expected.frame.beauty_rgba, 1e-6, "pool beauty parity " + std::to_string(i));
        expect_near(pooled[static_cast<std::size_t>(i)].profiled.frame.average_luminance,
            expected.frame.average_luminance, 1e-9, "pool luminance parity " + std::to_string(i));
    }
    return 0;
}
```

- [ ] **Step 2: Wire the test into CMake and verify the red phase**

```cmake
# CMakeLists.txt
target_sources(realtime_gpu
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/gpu/renderer_pool.cpp
)

add_executable(test_renderer_pool)
target_sources(test_renderer_pool
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_renderer_pool.cpp
)
target_link_libraries(test_renderer_pool PRIVATE realtime_gpu)
add_test(NAME test_renderer_pool COMMAND test_renderer_pool)
```

Run: `cmake --build build-clang-vcpkg-settings --target test_renderer_pool -j 4`  
Expected: FAIL because `RendererPool` does not exist yet

- [ ] **Step 3: Add the pool interface**

```cpp
// src/realtime/gpu/renderer_pool.h
#pragma once

#include "realtime/gpu/optix_renderer.h"

#include <vector>

namespace rt {

struct CameraRenderResult {
    int camera_index = 0;
    ProfiledRadianceFrame profiled;
};

class RendererPool {
   public:
    explicit RendererPool(int renderer_count);

    void prepare_scene(const PackedScene& scene);
    std::vector<CameraRenderResult> render_frame(const PackedCameraRig& rig, const RenderProfile& profile, int active_cameras);

   private:
    std::vector<OptixRenderer> renderers_;
};

}  // namespace rt
```

- [ ] **Step 4: Implement bounded async dispatch**

```cpp
// src/realtime/gpu/renderer_pool.cpp
#include "realtime/gpu/renderer_pool.h"

#include <future>
#include <stdexcept>

namespace rt {

RendererPool::RendererPool(int renderer_count) {
    if (renderer_count < 1 || renderer_count > 4) {
        throw std::runtime_error("RendererPool requires renderer_count in [1, 4]");
    }
    renderers_.resize(static_cast<std::size_t>(renderer_count));
}

void RendererPool::prepare_scene(const PackedScene& scene) {
    for (OptixRenderer& renderer : renderers_) {
        renderer.prepare_scene(scene);
    }
}

std::vector<CameraRenderResult> RendererPool::render_frame(
    const PackedCameraRig& rig, const RenderProfile& profile, int active_cameras) {
    if (active_cameras < 1 || active_cameras > static_cast<int>(renderers_.size())) {
        throw std::runtime_error("RendererPool active_cameras out of range");
    }

    std::vector<std::future<CameraRenderResult>> futures;
    futures.reserve(static_cast<std::size_t>(active_cameras));
    for (int camera_index = 0; camera_index < active_cameras; ++camera_index) {
        futures.push_back(std::async(std::launch::async, [this, &rig, &profile, camera_index]() {
            return CameraRenderResult {
                .camera_index = camera_index,
                .profiled = renderers_[static_cast<std::size_t>(camera_index)]
                                .render_prepared_radiance(rig, profile, camera_index),
            };
        }));
    }

    std::vector<CameraRenderResult> results;
    results.reserve(static_cast<std::size_t>(active_cameras));
    for (std::future<CameraRenderResult>& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

}  // namespace rt
```

- [ ] **Step 5: Verify green**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_renderer_pool|test_optix_prepared_scene' -V`  
Expected: PASS with pooled and sequential results matching

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt \
  src/realtime/gpu/renderer_pool.h \
  src/realtime/gpu/renderer_pool.cpp \
  tests/test_renderer_pool.cpp
git commit -m "feat: add concurrent renderer pool"
```

## Task 3: Integrate `RendererPool` into `render_realtime`

**Files:**
- Modify: `utils/render_realtime.cpp`
- Create: `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the failing 4-camera profiling verification**

```cmake
# cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
if(NOT DEFINED RENDER_REALTIME_EXE)
    message(FATAL_ERROR "RENDER_REALTIME_EXE is required")
endif()
if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${RENDER_REALTIME_EXE}"
        --camera-count 4
        --frames 2
        --profile realtime
        --skip-image-write
        --output-dir "${OUTPUT_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "render_realtime 4-camera run failed: ${run_stderr}")
endif()
if(NOT run_stdout MATCHES "cameras=4")
    message(FATAL_ERROR "stdout missing 4-camera frame line:\n${run_stdout}")
endif()

file(READ "${OUTPUT_DIR}/benchmark_summary.json" json_text)
if(NOT json_text MATCHES "\"camera_count\": 4")
    message(FATAL_ERROR "json missing 4-camera metadata:\n${json_text}")
endif()
if(NOT json_text MATCHES "\"camera_index\": 3")
    message(FATAL_ERROR "json missing camera 3 record:\n${json_text}")
endif()
```

```cmake
# CMakeLists.txt
add_test(NAME test_render_realtime_four_camera_profiling_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-four-camera
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
)
```

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_four_camera_profiling_cli -V`  
Expected: FAIL because the current CLI still uses one renderer and does not yet schedule through `RendererPool`

- [ ] **Step 2: Replace the single renderer with the pool**

```cpp
// utils/render_realtime.cpp
#include "realtime/gpu/renderer_pool.h"

// ...
    rt::RendererPool renderer_pool(camera_count);
    renderer_pool.prepare_scene(packed_scene);
    rt::OptixDenoiserWrapper denoiser;

    for (int frame_index = 0; frame_index < frames; ++frame_index) {
        const auto frame_begin = std::chrono::steady_clock::now();
        auto camera_results = renderer_pool.render_frame(packed_rig, profile, camera_count);
        rt::profiling::FrameStageSample frame_record {};
        frame_record.cameras.reserve(static_cast<std::size_t>(camera_count));

        for (rt::CameraRenderResult& result : camera_results) {
            rt::RadianceFrame frame = std::move(result.profiled.frame);
            frame_record.render_ms += static_cast<double>(result.profiled.timing.render_ms);
            frame_record.download_ms += static_cast<double>(result.profiled.timing.download_ms);
            // denoise and write remain in later steps
        }
    }
```

- [ ] **Step 3: Preserve report ordering and schema**

```cpp
// utils/render_realtime.cpp inside the per-camera loop
std::sort(camera_results.begin(), camera_results.end(), [](const rt::CameraRenderResult& lhs, const rt::CameraRenderResult& rhs) {
    return lhs.camera_index < rhs.camera_index;
});

for (rt::CameraRenderResult& result : camera_results) {
    frame_record.cameras.push_back(rt::profiling::CameraStageSample {
        .camera_index = result.camera_index,
        .render_ms = static_cast<double>(result.profiled.timing.render_ms),
        .denoise_ms = denoise_ms,
        .download_ms = static_cast<double>(result.profiled.timing.download_ms),
        .average_luminance = frame.average_luminance,
    });
}
```

- [ ] **Step 4: Verify green**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_render_realtime_profiling_cli|test_render_realtime_four_camera_profiling_cli|test_renderer_pool' -V`  
Expected: PASS with the same profiling fields and ordered camera records

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt \
  cmake/VerifyRenderRealtimeFourCameraProfiling.cmake \
  utils/render_realtime.cpp
git commit -m "feat: schedule realtime cameras with renderer pool"
```

## Task 4: Parallelize Host-Side Denoise with Bounded Async Tasks

**Files:**
- Modify: `utils/render_realtime.cpp`
- Modify: `tests/test_renderer_pool.cpp` only if a denoise-related regression check is needed
- Modify: `README.md`

- [ ] **Step 1: Add a failing CLI regression for bounded post-processing**

Use the existing `test_render_realtime_four_camera_profiling_cli` as the red driver by extending it to require `denoise_ms=` and stable per-camera records after the concurrent render path is in place.

```cmake
# cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
if(NOT run_stdout MATCHES "denoise_ms=")
    message(FATAL_ERROR "stdout missing denoise_ms field:\n${run_stdout}")
endif()
```

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_four_camera_profiling_cli -V`  
Expected: FAIL only if the scheduler refactor dropped denoise accounting while staging the next change

- [ ] **Step 2: Parallelize denoise with one async task per active camera**

```cpp
// utils/render_realtime.cpp
struct PostprocessResult {
    int camera_index = 0;
    rt::RadianceFrame frame;
    double denoise_ms = 0.0;
};

std::vector<std::future<PostprocessResult>> postprocess_futures;
postprocess_futures.reserve(camera_results.size());
for (rt::CameraRenderResult& result : camera_results) {
    postprocess_futures.push_back(std::async(std::launch::async,
        [&denoiser, &profile, result = std::move(result)]() mutable {
            PostprocessResult out {};
            out.camera_index = result.camera_index;
            out.frame = std::move(result.profiled.frame);
            if (profile.enable_denoise) {
                const auto denoise_begin = std::chrono::steady_clock::now();
                denoiser.run(out.frame);
                const auto denoise_end = std::chrono::steady_clock::now();
                out.denoise_ms = std::chrono::duration<double, std::milli>(denoise_end - denoise_begin).count();
            }
            return out;
        }));
}
```

- [ ] **Step 3: Fold post-processed results back into the existing reporting path**

```cpp
// utils/render_realtime.cpp
std::vector<PostprocessResult> postprocessed;
postprocessed.reserve(postprocess_futures.size());
for (std::future<PostprocessResult>& future : postprocess_futures) {
    postprocessed.push_back(future.get());
}
std::sort(postprocessed.begin(), postprocessed.end(), [](const PostprocessResult& lhs, const PostprocessResult& rhs) {
    return lhs.camera_index < rhs.camera_index;
});

for (PostprocessResult& item : postprocessed) {
    frame_record.denoise_ms += item.denoise_ms;
    frame_luminance_sum += item.frame.average_luminance;
    if (!skip_image_write) {
        const auto image_write_begin = std::chrono::steady_clock::now();
        write_frame_image(output_path, frame_index, item.camera_index, item.frame);
        const auto image_write_end = std::chrono::steady_clock::now();
        frame_record.image_write_ms += std::chrono::duration<double, std::milli>(image_write_end - image_write_begin).count();
    }
}
```

- [ ] **Step 4: Update the benchmark docs**

```md
<!-- README.md -->
The benchmark path now uses one renderer per active camera and bounded host-side post-processing. The matrix runner remains the supported way to capture comparable `1 / 2 / 4` camera baselines.
```

- [ ] **Step 5: Verify green**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_render_realtime_cli_skip_write|test_render_realtime_four_camera_profiling_cli|test_render_realtime_profiling_cli' -V`  
Expected: PASS with unchanged profiling schema and valid 4-camera output

- [ ] **Step 6: Commit**

```bash
git add README.md utils/render_realtime.cpp cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
git commit -m "feat: parallelize realtime denoise path"
```

## Task 5: Final Verification and Baseline Capture

**Files:**
- Modify: `README.md` only if a command changed while implementing

- [ ] **Step 1: Build the focused target set**

Run:

```bash
VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings \
  --target render_realtime test_render_profile test_realtime_benchmark_report \
           test_optix_direction test_optix_profiled_render test_optix_prepared_scene \
           test_optix_path_trace test_optix_materials_aux test_optix_equi_path_trace \
           test_renderer_pool test_realtime_pipeline test_reference_vs_realtime -j 4
```

Expected: all targets build successfully

- [ ] **Step 2: Run the focused test suite**

Run:

```bash
ctest --test-dir build-clang-vcpkg-settings -R 'test_render_profile|test_realtime_benchmark_report|test_optix_direction|test_optix_profiled_render|test_optix_prepared_scene|test_optix_path_trace|test_optix_materials_aux|test_optix_equi_path_trace|test_renderer_pool|test_render_realtime_cli|test_render_realtime_profiling_cli|test_render_realtime_four_camera_profiling_cli|test_render_realtime_cli_skip_write|test_realtime_pipeline|test_reference_vs_realtime' -V
```

Expected: `100% tests passed`

- [ ] **Step 3: Run the benchmark matrix**

Run:

```bash
bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix-host-overhead 3
```

Expected:

- `build/realtime-matrix-host-overhead/realtime-c4/benchmark_summary.json` exists
- `build/realtime-matrix-host-overhead/balanced-c4/benchmark_summary.json` exists

- [ ] **Step 4: Record the before/after host-overhead comparison**

Run:

```bash
sed -n '1,120p' build/realtime-matrix-final/realtime-c4/benchmark_summary.json
sed -n '1,120p' build/realtime-matrix-host-overhead/realtime-c4/benchmark_summary.json
sed -n '1,120p' build/realtime-matrix-host-overhead/balanced-c4/benchmark_summary.json
```

Expected:

- `host_overhead_ms.avg` is lower than the `111.206 ms` `realtime-c4` baseline
- the JSON schema remains unchanged
- `image_write_ms.avg` remains `0` in the matrix runs

- [ ] **Step 5: Check the worktree**

Run: `git status --short`  
Expected: clean working tree

---

## Self-Review

Spec coverage:

- prepared scene reuse and pinned staging: Task 1
- multi-renderer and multi-stream style scheduling: Tasks 2 and 3
- bounded host-side denoise parallelism: Task 4
- unchanged profiling schema and matrix validation: Tasks 3 and 5
- focused correctness preservation: Tasks 1, 2, and 5

Placeholder scan:

- all tasks name exact files
- red/green commands are explicit
- commit points are defined

Type consistency:

- `prepare_scene()` and `render_prepared_radiance()` are introduced before pool usage
- `RendererPool` returns `CameraRenderResult`, which is then used by the CLI tasks
