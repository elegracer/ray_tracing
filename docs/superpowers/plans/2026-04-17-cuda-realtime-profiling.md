# CUDA Realtime Profiling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add repeatable host + CUDA-event profiling to the realtime CLI, emit CSV/JSON benchmark artifacts, run the `1 / 2 / 4 camera × balanced / realtime` matrix, then land one low-risk optimization justified by the measured bottleneck.

**Architecture:** Keep `render_realtime` as the single benchmark entry point, but split reusable reporting code into a small `src/realtime/profiling/` helper so the CLI does not absorb all aggregation and serialization logic. Add a profiled render path in `OptixRenderer` that can report GPU render and download timings without changing image formation semantics, then use those records in the CLI, matrix runner, and optimization comparison workflow.

**Tech Stack:** C++23, CUDA runtime events, OptiX 9.1, fmt, argparse, OpenCV, CMake, CTest, POSIX shell

---

## File Structure

- Create: `src/realtime/profiling/benchmark_report.h`
  - Own the profiling data model shared by the CLI and tests.
- Create: `src/realtime/profiling/benchmark_report.cpp`
  - Own aggregate statistics and CSV/JSON serialization.
- Modify: `src/realtime/gpu/optix_renderer.h`
  - Expose a profiled radiance API that returns both the frame and timing data.
- Modify: `src/realtime/gpu/optix_renderer.cpp`
  - Add CUDA-event timing for render and explicit device-to-host download plus any pinned host staging needed for measured download copies.
- Modify: `utils/render_realtime.cpp`
  - Collect per-frame and per-camera timing, print richer summaries, write benchmark artifacts, and later add the chosen low-risk optimization flag.
- Create: `cmake/VerifyRenderRealtimeProfiling.cmake`
  - Run the CLI smoke in CTest and validate stdout plus artifact files.
- Create: `tests/test_realtime_benchmark_report.cpp`
  - Lock aggregate math and CSV/JSON field presence.
- Create: `tests/test_optix_profiled_render.cpp`
  - Lock the new profiled renderer API contract.
- Create: `utils/run_realtime_benchmark_matrix.sh`
  - Run the fixed `1 / 2 / 4 × balanced / realtime` benchmark matrix reproducibly.
- Modify: `CMakeLists.txt`
  - Wire the new helper sources, tests, and CLI verification script.
- Modify: `README.md`
  - Document profiling outputs, matrix execution, and the later benchmark optimization flag.

## Task 1: Add Benchmark Report Model and Serialization

**Files:**
- Create: `src/realtime/profiling/benchmark_report.h`
- Create: `src/realtime/profiling/benchmark_report.cpp`
- Create: `tests/test_realtime_benchmark_report.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing benchmark report test**

```cpp
// tests/test_realtime_benchmark_report.cpp
#include "realtime/profiling/benchmark_report.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace profiling = rt::profiling;

    profiling::RunReport report {};
    report.profile = "realtime";
    report.camera_count = 2;
    report.width = 640;
    report.height = 480;
    report.frames_requested = 2;
    report.samples_per_pixel = 1;
    report.max_bounces = 2;
    report.denoise_enabled = true;
    report.frames = {
        profiling::FrameStageSample {
            .frame_index = 0,
            .camera_count = 2,
            .profile = "realtime",
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 10.0,
            .render_ms = 6.0,
            .denoise_ms = 2.0,
            .download_ms = 1.0,
            .image_write_ms = 0.5,
            .host_overhead_ms = 0.5,
            .fps = 100.0,
            .cameras = {
                profiling::CameraStageSample {.camera_index = 0, .render_ms = 3.0, .denoise_ms = 1.0, .download_ms = 0.5, .average_luminance = 0.12},
                profiling::CameraStageSample {.camera_index = 1, .render_ms = 3.0, .denoise_ms = 1.0, .download_ms = 0.5, .average_luminance = 0.13},
            },
        },
        profiling::FrameStageSample {
            .frame_index = 1,
            .camera_count = 2,
            .profile = "realtime",
            .width = 640,
            .height = 480,
            .samples_per_pixel = 1,
            .max_bounces = 2,
            .denoise_enabled = true,
            .frame_ms = 14.0,
            .render_ms = 8.0,
            .denoise_ms = 3.0,
            .download_ms = 1.5,
            .image_write_ms = 1.0,
            .host_overhead_ms = 0.5,
            .fps = 71.428571,
            .cameras = {
                profiling::CameraStageSample {.camera_index = 0, .render_ms = 4.0, .denoise_ms = 1.5, .download_ms = 0.75, .average_luminance = 0.14},
                profiling::CameraStageSample {.camera_index = 1, .render_ms = 4.0, .denoise_ms = 1.5, .download_ms = 0.75, .average_luminance = 0.15},
            },
        },
    };

    report.aggregate = profiling::compute_aggregate(report.frames);
    expect_near(report.aggregate.frame_ms.avg, 12.0, 1e-12, "frame avg");
    expect_near(report.aggregate.frame_ms.p50, 10.0, 1e-12, "frame p50");
    expect_near(report.aggregate.frame_ms.p95, 14.0, 1e-12, "frame p95");
    expect_near(report.aggregate.denoise_ms.max, 3.0, 1e-12, "denoise max");

    const std::filesystem::path out_dir = std::filesystem::temp_directory_path() / "rt-benchmark-report-test";
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path csv_path = out_dir / "benchmark_frames.csv";
    const std::filesystem::path json_path = out_dir / "benchmark_summary.json";

    profiling::write_csv(report, csv_path);
    profiling::write_json(report, json_path);

    std::ifstream csv(csv_path);
    std::string csv_text((std::istreambuf_iterator<char>(csv)), std::istreambuf_iterator<char>());
    expect_true(csv_text.find("frame_index,camera_count,profile,width,height") != std::string::npos, "csv header");
    expect_true(csv_text.find("0,2,realtime,640,480") != std::string::npos, "csv first row");

    std::ifstream json(json_path);
    std::string json_text((std::istreambuf_iterator<char>(json)), std::istreambuf_iterator<char>());
    expect_true(json_text.find("\"profile\": \"realtime\"") != std::string::npos, "json metadata");
    expect_true(json_text.find("\"frame_ms\"") != std::string::npos, "json aggregate");
    expect_true(json_text.find("\"camera_index\": 1") != std::string::npos, "json per camera");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target test_realtime_benchmark_report -j 4`

Expected: FAIL with `No rule to make target 'test_realtime_benchmark_report'` or missing `realtime/profiling/benchmark_report.h`

- [ ] **Step 3: Add the benchmark report helper and test target**

```cpp
// src/realtime/profiling/benchmark_report.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rt::profiling {

struct CameraStageSample {
    int camera_index = 0;
    double render_ms = 0.0;
    double denoise_ms = 0.0;
    double download_ms = 0.0;
    double average_luminance = 0.0;
};

struct FrameStageSample {
    int frame_index = 0;
    int camera_count = 0;
    std::string profile;
    int width = 0;
    int height = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    double frame_ms = 0.0;
    double render_ms = 0.0;
    double denoise_ms = 0.0;
    double download_ms = 0.0;
    double image_write_ms = 0.0;
    double host_overhead_ms = 0.0;
    double fps = 0.0;
    std::vector<CameraStageSample> cameras;
};

struct AggregateStats {
    double avg = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double max = 0.0;
};

struct RunAggregate {
    AggregateStats frame_ms;
    AggregateStats render_ms;
    AggregateStats denoise_ms;
    AggregateStats download_ms;
    AggregateStats image_write_ms;
    AggregateStats host_overhead_ms;
};

struct RunReport {
    std::string profile;
    int camera_count = 0;
    int width = 0;
    int height = 0;
    int frames_requested = 0;
    int samples_per_pixel = 0;
    int max_bounces = 0;
    bool denoise_enabled = false;
    std::vector<FrameStageSample> frames;
    RunAggregate aggregate;
};

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames);
void write_csv(const RunReport& report, const std::filesystem::path& path);
void write_json(const RunReport& report, const std::filesystem::path& path);

}  // namespace rt::profiling
```

```cpp
// src/realtime/profiling/benchmark_report.cpp
#include "realtime/profiling/benchmark_report.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace rt::profiling {
namespace {

AggregateStats compute_stats(std::vector<double> values) {
    AggregateStats stats {};
    if (values.empty()) {
        return stats;
    }
    std::sort(values.begin(), values.end());
    const auto percentile_index = [&](double p) -> std::size_t {
        return static_cast<std::size_t>(p * static_cast<double>(values.size() - 1));
    };
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    stats.avg = sum / static_cast<double>(values.size());
    stats.p50 = values[percentile_index(0.50)];
    stats.p95 = values[percentile_index(0.95)];
    stats.max = values.back();
    return stats;
}

std::string bool_text(bool value) {
    return value ? "true" : "false";
}

}  // namespace

RunAggregate compute_aggregate(const std::vector<FrameStageSample>& frames) {
    std::vector<double> frame_ms;
    std::vector<double> render_ms;
    std::vector<double> denoise_ms;
    std::vector<double> download_ms;
    std::vector<double> image_write_ms;
    std::vector<double> host_overhead_ms;
    for (const FrameStageSample& frame : frames) {
        frame_ms.push_back(frame.frame_ms);
        render_ms.push_back(frame.render_ms);
        denoise_ms.push_back(frame.denoise_ms);
        download_ms.push_back(frame.download_ms);
        image_write_ms.push_back(frame.image_write_ms);
        host_overhead_ms.push_back(frame.host_overhead_ms);
    }
    return RunAggregate {
        .frame_ms = compute_stats(frame_ms),
        .render_ms = compute_stats(render_ms),
        .denoise_ms = compute_stats(denoise_ms),
        .download_ms = compute_stats(download_ms),
        .image_write_ms = compute_stats(image_write_ms),
        .host_overhead_ms = compute_stats(host_overhead_ms),
    };
}

void write_csv(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "frame_index,camera_count,profile,width,height,samples_per_pixel,max_bounces,denoise_enabled,"
           "frame_ms,render_ms,denoise_ms,download_ms,image_write_ms,host_overhead_ms,fps\n";
    for (const FrameStageSample& frame : report.frames) {
        out << frame.frame_index << ',' << frame.camera_count << ',' << frame.profile << ','
            << frame.width << ',' << frame.height << ',' << frame.samples_per_pixel << ','
            << frame.max_bounces << ',' << bool_text(frame.denoise_enabled) << ','
            << frame.frame_ms << ',' << frame.render_ms << ',' << frame.denoise_ms << ','
            << frame.download_ms << ',' << frame.image_write_ms << ','
            << frame.host_overhead_ms << ',' << frame.fps << '\n';
    }
}

void write_json(const RunReport& report, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "{\n"
        << "  \"profile\": \"" << report.profile << "\",\n"
        << "  \"camera_count\": " << report.camera_count << ",\n"
        << "  \"width\": " << report.width << ",\n"
        << "  \"height\": " << report.height << ",\n"
        << "  \"frames_requested\": " << report.frames_requested << ",\n"
        << "  \"samples_per_pixel\": " << report.samples_per_pixel << ",\n"
        << "  \"max_bounces\": " << report.max_bounces << ",\n"
        << "  \"denoise_enabled\": " << bool_text(report.denoise_enabled) << ",\n"
        << "  \"aggregate\": {\n"
        << "    \"frame_ms\": {\"avg\": " << report.aggregate.frame_ms.avg << ", \"p50\": "
        << report.aggregate.frame_ms.p50 << ", \"p95\": " << report.aggregate.frame_ms.p95
        << ", \"max\": " << report.aggregate.frame_ms.max << "}\n"
        << "  },\n"
        << "  \"frames\": [\n";
    for (std::size_t i = 0; i < report.frames.size(); ++i) {
        const FrameStageSample& frame = report.frames[i];
        out << "    {\"frame_index\": " << frame.frame_index << ", \"camera_index\": "
            << frame.cameras.front().camera_index << "}";
        out << (i + 1 == report.frames.size() ? '\n' : ',') ;
    }
    out << "  ]\n}\n";
}

}  // namespace rt::profiling
```

```cmake
# CMakeLists.txt
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/profiling/benchmark_report.cpp
)

add_executable(test_realtime_benchmark_report)
target_sources(test_realtime_benchmark_report
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_realtime_benchmark_report.cpp
)
target_link_libraries(test_realtime_benchmark_report PRIVATE core)
add_test(NAME test_realtime_benchmark_report COMMAND test_realtime_benchmark_report)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_realtime_benchmark_report -V`

Expected: PASS with `100% tests passed`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt \
  src/realtime/profiling/benchmark_report.h \
  src/realtime/profiling/benchmark_report.cpp \
  tests/test_realtime_benchmark_report.cpp
git commit -m "feat: add realtime benchmark report model"
```

## Task 2: Add a Profiled Radiance API with Render and Download Timings

**Files:**
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Create: `tests/test_optix_profiled_render.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing profiled renderer test**

```cpp
// tests/test_optix_profiled_render.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
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

    rt::OptixRenderer renderer;
    const rt::ProfiledRadianceFrame profiled =
        renderer.render_radiance_profiled(scene.pack(), rig.pack(), rt::RenderProfile::realtime(), 0);

    expect_true(profiled.frame.width == 32, "profiled frame width");
    expect_true(profiled.frame.height == 32, "profiled frame height");
    expect_true(!profiled.frame.beauty_rgba.empty(), "beauty present");
    expect_true(profiled.timing.render_ms >= 0.0f, "render timing is non-negative");
    expect_true(profiled.timing.download_ms >= 0.0f, "download timing is non-negative");
    return 0;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target test_optix_profiled_render -j 4`

Expected: FAIL because the target or `render_radiance_profiled` API does not exist yet

- [ ] **Step 3: Add the profiled renderer path**

```cpp
// src/realtime/gpu/optix_renderer.h
struct RadianceTiming {
    float render_ms = 0.0f;
    float download_ms = 0.0f;
};

struct ProfiledRadianceFrame {
    RadianceFrame frame;
    RadianceTiming timing;
};

class OptixRenderer {
   public:
    ProfiledRadianceFrame render_radiance_profiled(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);

   private:
    void allocate_host_staging_buffers(int width, int height);
    void free_host_staging_buffers();
    RadianceFrame download_radiance_frame_profiled(int camera_index, RadianceTiming* timing);
    float measure_render_ms(const LaunchParams& params);
    float measure_download_ms(std::size_t pixel_count);

    struct HostFrameStaging {
        float4* beauty = nullptr;
        float4* normal = nullptr;
        float4* albedo = nullptr;
        float* depth = nullptr;
    };

    HostFrameStaging host_stage_{};
    int staged_width_ = 0;
    int staged_height_ = 0;
};
```

```cpp
// src/realtime/gpu/optix_renderer.cpp
namespace {

void free_host_stage_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFreeHost(ptr);
    }
}

}  // namespace

void OptixRenderer::allocate_host_staging_buffers(int width, int height) {
    if (staged_width_ == width && staged_height_ == height) {
        return;
    }
    free_host_staging_buffers();
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.beauty), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.normal), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.albedo), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&host_stage_.depth), pixel_count * sizeof(float)));
    staged_width_ = width;
    staged_height_ = height;
}

void OptixRenderer::free_host_staging_buffers() {
    free_host_stage_ptr(host_stage_.beauty);
    free_host_stage_ptr(host_stage_.normal);
    free_host_stage_ptr(host_stage_.albedo);
    free_host_stage_ptr(host_stage_.depth);
    host_stage_ = HostFrameStaging {};
    staged_width_ = 0;
    staged_height_ = 0;
}

float OptixRenderer::measure_render_ms(const LaunchParams& params) {
    cudaEvent_t start = nullptr;
    cudaEvent_t end = nullptr;
    RT_CUDA_CHECK(cudaEventCreate(&start));
    RT_CUDA_CHECK(cudaEventCreate(&end));
    RT_CUDA_CHECK(cudaEventRecord(start, stream_));
    launch_radiance_kernel(params, stream_);
    RT_CUDA_CHECK(cudaEventRecord(end, stream_));
    RT_CUDA_CHECK(cudaEventSynchronize(end));
    float elapsed_ms = 0.0f;
    RT_CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, start, end));
    RT_CUDA_CHECK(cudaEventDestroy(start));
    RT_CUDA_CHECK(cudaEventDestroy(end));
    return elapsed_ms;
}

float OptixRenderer::measure_download_ms(std::size_t pixel_count) {
    cudaEvent_t start = nullptr;
    cudaEvent_t end = nullptr;
    RT_CUDA_CHECK(cudaEventCreate(&start));
    RT_CUDA_CHECK(cudaEventCreate(&end));
    RT_CUDA_CHECK(cudaEventRecord(start, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(host_stage_.beauty, device_frame_.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(host_stage_.normal, device_frame_.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(host_stage_.albedo, device_frame_.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(host_stage_.depth, device_frame_.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost, stream_));
    RT_CUDA_CHECK(cudaEventRecord(end, stream_));
    RT_CUDA_CHECK(cudaEventSynchronize(end));
    float elapsed_ms = 0.0f;
    RT_CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, start, end));
    RT_CUDA_CHECK(cudaEventDestroy(start));
    RT_CUDA_CHECK(cudaEventDestroy(end));
    return elapsed_ms;
}

ProfiledRadianceFrame OptixRenderer::render_radiance_profiled(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    validate_radiance_request(rig, camera_index);
    upload_scene(scene);
    build_or_refit_accels(scene);

    LaunchParams params {};
    params.width = rig.cameras[camera_index].width;
    params.height = rig.cameras[camera_index].height;
    params.active_camera = make_active_camera(rig.cameras[camera_index]);
    allocate_frame_buffers(params.width, params.height);
    allocate_host_staging_buffers(params.width, params.height);
    params.frame = device_frame_;
    params.scene = DeviceSceneView {
        .spheres = device_spheres_,
        .quads = device_quads_,
        .materials = device_materials_,
        .sphere_count = scene.sphere_count,
        .quad_count = scene.quad_count,
        .material_count = scene.material_count,
    };
    params.samples_per_pixel = profile.samples_per_pixel;
    params.max_bounces = profile.max_bounces;
    params.rr_start_bounce = profile.rr_start_bounce;
    params.mode = 1;

    ProfiledRadianceFrame out {};
    out.timing.render_ms = measure_render_ms(params);
    const std::size_t pixel_count = static_cast<std::size_t>(params.width) * static_cast<std::size_t>(params.height);
    out.timing.download_ms = measure_download_ms(pixel_count);
    out.frame = download_radiance_frame_profiled(camera_index, &out.timing);
    return out;
}
```

```cmake
# CMakeLists.txt
add_executable(test_optix_profiled_render)
target_sources(test_optix_profiled_render
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optix_profiled_render.cpp
)
target_link_libraries(test_optix_profiled_render PRIVATE realtime_gpu)
add_test(NAME test_optix_profiled_render COMMAND test_optix_profiled_render)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_optix_profiled_render|test_optix_path_trace' -V`

Expected: PASS with both tests green

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt \
  src/realtime/gpu/optix_renderer.h \
  src/realtime/gpu/optix_renderer.cpp \
  tests/test_optix_profiled_render.cpp
git commit -m "feat: add profiled optix radiance timings"
```

## Task 3: Emit CLI Profiling Artifacts and Validate Them in CTest

**Files:**
- Modify: `utils/render_realtime.cpp`
- Create: `cmake/VerifyRenderRealtimeProfiling.cmake`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing CLI profiling verification**

```cmake
# cmake/VerifyRenderRealtimeProfiling.cmake
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
        --camera-count 1
        --frames 2
        --profile realtime
        --output-dir "${OUTPUT_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "render_realtime failed: ${run_stderr}")
endif()
if(NOT run_stdout MATCHES "download_ms=")
    message(FATAL_ERROR "stdout missing download_ms field:\n${run_stdout}")
endif()
if(NOT run_stdout MATCHES "host_overhead_ms=")
    message(FATAL_ERROR "stdout missing host_overhead_ms field:\n${run_stdout}")
endif()

set(csv_path "${OUTPUT_DIR}/benchmark_frames.csv")
set(json_path "${OUTPUT_DIR}/benchmark_summary.json")
if(NOT EXISTS "${csv_path}")
    message(FATAL_ERROR "missing ${csv_path}")
endif()
if(NOT EXISTS "${json_path}")
    message(FATAL_ERROR "missing ${json_path}")
endif()

file(READ "${csv_path}" csv_text)
if(NOT csv_text MATCHES "frame_index,camera_count,profile,width,height,samples_per_pixel,max_bounces,denoise_enabled")
    message(FATAL_ERROR "csv header is incomplete:\n${csv_text}")
endif()

file(READ "${json_path}" json_text)
if(NOT json_text MATCHES "\"download_ms\"")
    message(FATAL_ERROR "json missing download timing:\n${json_text}")
endif()
if(NOT json_text MATCHES "\"cameras\"")
    message(FATAL_ERROR "json missing per-camera records:\n${json_text}")
endif()
```

```cmake
# CMakeLists.txt
add_test(NAME test_render_realtime_profiling_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-profiling
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeProfiling.cmake
)
```

- [ ] **Step 2: Run the verification and verify it fails**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_profiling_cli -V`

Expected: FAIL because stdout still lacks `download_ms` / `host_overhead_ms` and no CSV or JSON artifacts exist yet

- [ ] **Step 3: Integrate profiled rendering and artifact writing into the CLI**

```cpp
// utils/render_realtime.cpp
#include "realtime/profiling/benchmark_report.h"

// ...

    rt::profiling::RunReport report {};
    report.profile = profile_name;
    report.camera_count = camera_count;
    report.width = kDefaultWidth;
    report.height = kDefaultHeight;
    report.frames_requested = frames;
    report.samples_per_pixel = profile.samples_per_pixel;
    report.max_bounces = profile.max_bounces;
    report.denoise_enabled = profile.enable_denoise;

    for (int frame_index = 0; frame_index < frames; ++frame_index) {
        const auto frame_begin = std::chrono::steady_clock::now();
        rt::profiling::FrameStageSample frame_record {};
        frame_record.frame_index = frame_index;
        frame_record.camera_count = camera_count;
        frame_record.profile = profile_name;
        frame_record.width = kDefaultWidth;
        frame_record.height = kDefaultHeight;
        frame_record.samples_per_pixel = profile.samples_per_pixel;
        frame_record.max_bounces = profile.max_bounces;
        frame_record.denoise_enabled = profile.enable_denoise;

        double frame_luminance_sum = 0.0;
        for (int camera_index = 0; camera_index < camera_count; ++camera_index) {
            const rt::ProfiledRadianceFrame profiled =
                renderer.render_radiance_profiled(packed_scene, packed_rig, profile, camera_index);
            rt::RadianceFrame frame = profiled.frame;
            frame_record.render_ms += static_cast<double>(profiled.timing.render_ms);
            frame_record.download_ms += static_cast<double>(profiled.timing.download_ms);

            double denoise_ms = 0.0;
            if (profile.enable_denoise) {
                const auto denoise_begin = std::chrono::steady_clock::now();
                denoiser.run(frame);
                const auto denoise_end = std::chrono::steady_clock::now();
                denoise_ms = std::chrono::duration<double, std::milli>(denoise_end - denoise_begin).count();
                frame_record.denoise_ms += denoise_ms;
            }

            const auto image_write_begin = std::chrono::steady_clock::now();
            write_frame_image(output_path, frame_index, camera_index, frame);
            const auto image_write_end = std::chrono::steady_clock::now();
            frame_record.image_write_ms +=
                std::chrono::duration<double, std::milli>(image_write_end - image_write_begin).count();
            frame_luminance_sum += frame.average_luminance;

            frame_record.cameras.push_back(rt::profiling::CameraStageSample {
                .camera_index = camera_index,
                .render_ms = static_cast<double>(profiled.timing.render_ms),
                .denoise_ms = denoise_ms,
                .download_ms = static_cast<double>(profiled.timing.download_ms),
                .average_luminance = frame.average_luminance,
            });
        }

        const auto frame_end = std::chrono::steady_clock::now();
        frame_record.frame_ms =
            std::chrono::duration<double, std::milli>(frame_end - frame_begin).count();
        frame_record.host_overhead_ms = frame_record.frame_ms - frame_record.render_ms
            - frame_record.denoise_ms - frame_record.download_ms - frame_record.image_write_ms;
        frame_record.fps = 1000.0 / frame_record.frame_ms;
        report.frames.push_back(frame_record);

        fmt::print(
            "frame={} cameras={} avg_luminance={:.6f} render_ms={:.3f} denoise_ms={:.3f} download_ms={:.3f} image_write_ms={:.3f} host_overhead_ms={:.3f} frame_ms={:.3f}\n",
            frame_index,
            camera_count,
            frame_luminance_sum / static_cast<double>(camera_count),
            frame_record.render_ms,
            frame_record.denoise_ms,
            frame_record.download_ms,
            frame_record.image_write_ms,
            frame_record.host_overhead_ms,
            frame_record.frame_ms);
    }

    report.aggregate = rt::profiling::compute_aggregate(report.frames);
    rt::profiling::write_csv(report, output_path / "benchmark_frames.csv");
    rt::profiling::write_json(report, output_path / "benchmark_summary.json");
```

```cmake
# CMakeLists.txt
set_tests_properties(test_render_realtime_profiling_cli PROPERTIES
    PASS_REGULAR_EXPRESSION "summary profile=realtime"
)
```

- [ ] **Step 4: Run the verification and verify it passes**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_render_realtime_profiling_cli|test_render_realtime_cli' -V`

Expected: PASS with profiling stdout and both artifact files present

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt \
  cmake/VerifyRenderRealtimeProfiling.cmake \
  utils/render_realtime.cpp
git commit -m "feat: emit realtime profiling artifacts"
```

## Task 4: Add a Reproducible Matrix Runner and Document the Workflow

**Files:**
- Create: `utils/run_realtime_benchmark_matrix.sh`
- Modify: `README.md`

- [ ] **Step 1: Write the failing matrix runner smoke**

Run: `bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix 2`

Expected: FAIL with `No such file or directory`

- [ ] **Step 2: Add the matrix runner and docs**

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <output-root> [frames]" >&2
    exit 1
fi

OUTPUT_ROOT="$1"
FRAMES="${2:-3}"
BIN="./bin/render_realtime"

mkdir -p "${OUTPUT_ROOT}"

for PROFILE in balanced realtime; do
  for CAMERAS in 1 2 4; do
    RUN_DIR="${OUTPUT_ROOT}/${PROFILE}-c${CAMERAS}"
    mkdir -p "${RUN_DIR}"
    "${BIN}" \
      --camera-count "${CAMERAS}" \
      --frames "${FRAMES}" \
      --profile "${PROFILE}" \
      --output-dir "${RUN_DIR}"
  done
done
```

```md
<!-- README.md -->
./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke
bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix 3

Each run directory writes:

- PNG outputs for the smoke render
- `benchmark_frames.csv`
- `benchmark_summary.json`
```

- [ ] **Step 3: Run the matrix runner smoke and verify it passes**

Run: `bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix 2`

Expected: exit `0` and create:

- `build/realtime-matrix/balanced-c1/benchmark_frames.csv`
- `build/realtime-matrix/balanced-c2/benchmark_frames.csv`
- `build/realtime-matrix/balanced-c4/benchmark_frames.csv`
- `build/realtime-matrix/realtime-c1/benchmark_summary.json`
- `build/realtime-matrix/realtime-c2/benchmark_summary.json`
- `build/realtime-matrix/realtime-c4/benchmark_summary.json`

- [ ] **Step 4: Commit**

```bash
git add README.md utils/run_realtime_benchmark_matrix.sh
git commit -m "docs: add realtime benchmark matrix runner"
```

## Task 5: Land the Low-Risk Optimization Selected by the Baseline

**Files:**
- Modify: `utils/render_realtime.cpp`
- Modify: `cmake/VerifyRenderRealtimeProfiling.cmake`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

Use this explicit decision rule:

- Run the baseline matrix from Task 4.
- Read `build/realtime-matrix/balanced-c4/benchmark_summary.json` and `build/realtime-matrix/realtime-c4/benchmark_summary.json`.
- If either run shows `image_write_ms.avg >= 0.15 * frame_ms.avg`, choose benchmark-path write isolation.
- The current branch is expected to satisfy this threshold, so the optimization for this task is: add `--skip-image-write` to let benchmark runs isolate render/denoise/download cost without PNG overhead.

- [ ] **Step 1: Write the failing skip-write verification**

```cmake
# cmake/VerifyRenderRealtimeProfiling.cmake
if(DEFINED EXPECT_SKIP_WRITE AND EXPECT_SKIP_WRITE)
    execute_process(
        COMMAND "${RENDER_REALTIME_EXE}"
            --camera-count 1
            --frames 2
            --profile realtime
            --skip-image-write
            --output-dir "${OUTPUT_DIR}"
        RESULT_VARIABLE skip_result
        OUTPUT_VARIABLE skip_stdout
        ERROR_VARIABLE skip_stderr
    )

    if(NOT skip_result EQUAL 0)
        message(FATAL_ERROR "skip-write render_realtime failed: ${skip_stderr}")
    endif()
    if(NOT skip_stdout MATCHES "image_write_ms=0\\.000")
        message(FATAL_ERROR "skip-write stdout missing zero image_write_ms:\n${skip_stdout}")
    endif()
    file(GLOB pngs "${OUTPUT_DIR}/*.png")
    list(LENGTH pngs png_count)
    if(NOT png_count EQUAL 0)
        message(FATAL_ERROR "skip-write run still wrote PNG files")
    endif()
endif()
```

```cmake
# CMakeLists.txt
add_test(NAME test_render_realtime_cli_skip_write
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-skip-write
        -DEXPECT_SKIP_WRITE=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeProfiling.cmake
)
```

- [ ] **Step 2: Run the verification and verify it fails**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_cli_skip_write -V`

Expected: FAIL because `--skip-image-write` does not exist yet

- [ ] **Step 3: Add the skip-write optimization**

```cpp
// utils/render_realtime.cpp
    bool skip_image_write = false;
    program.add_argument("--skip-image-write")
        .help("benchmark mode: skip PNG writes and keep image_write_ms at zero")
        .default_value(false)
        .implicit_value(true)
        .store_into(skip_image_write);

// ...

            if (!skip_image_write) {
                const auto image_write_begin = std::chrono::steady_clock::now();
                write_frame_image(output_path, frame_index, camera_index, frame);
                const auto image_write_end = std::chrono::steady_clock::now();
                frame_record.image_write_ms +=
                    std::chrono::duration<double, std::milli>(image_write_end - image_write_begin).count();
            }
```

```bash
# utils/run_realtime_benchmark_matrix.sh
"${BIN}" \
  --camera-count "${CAMERAS}" \
  --frames "${FRAMES}" \
  --profile "${PROFILE}" \
  --skip-image-write \
  --output-dir "${RUN_DIR}"
```

```md
<!-- README.md -->
For pure benchmark runs, use:

./bin/render_realtime --camera-count 4 --frames 3 --profile realtime --skip-image-write --output-dir build/realtime-benchmark
```

- [ ] **Step 4: Run the before/after comparison**

Run:

```bash
./bin/render_realtime --camera-count 4 --frames 3 --profile realtime --output-dir build/realtime-before-write
./bin/render_realtime --camera-count 4 --frames 3 --profile realtime --skip-image-write --output-dir build/realtime-after-write
ctest --test-dir build-clang-vcpkg-settings -R 'test_render_realtime_cli_skip_write|test_render_realtime_profiling_cli' -V
```

Expected:

- the skip-write run exits `0`
- `image_write_ms` becomes `0`
- no PNG files are created in `build/realtime-after-write`
- focused CLI profiling tests stay green

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt \
  README.md \
  cmake/VerifyRenderRealtimeProfiling.cmake \
  utils/render_realtime.cpp \
  utils/run_realtime_benchmark_matrix.sh
git commit -m "feat: add realtime benchmark skip-write mode"
```

## Task 6: Final Verification and Baseline Capture

**Files:**
- Modify: `README.md` only if a command changed while implementing

- [ ] **Step 1: Build the full focused target set**

Run:

```bash
VCPKG_ROOT=$HOME/vcpkg_root cmake --build build-clang-vcpkg-settings \
  --target render_realtime test_render_profile test_realtime_benchmark_report \
           test_optix_direction test_optix_profiled_render test_optix_path_trace \
           test_optix_materials_aux test_optix_equi_path_trace \
           test_realtime_pipeline test_reference_vs_realtime -j 4
```

Expected: all targets build successfully

- [ ] **Step 2: Run the full focused test suite**

Run:

```bash
ctest --test-dir build-clang-vcpkg-settings -R 'test_render_profile|test_realtime_benchmark_report|test_optix_direction|test_optix_profiled_render|test_optix_path_trace|test_optix_materials_aux|test_optix_equi_path_trace|test_render_realtime_cli|test_render_realtime_profiling_cli|test_render_realtime_cli_skip_write|test_realtime_pipeline|test_reference_vs_realtime' -V
```

Expected: `100% tests passed`

- [ ] **Step 3: Run the reproducible benchmark matrix**

Run:

```bash
bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix-final 3
```

Expected:

- exit `0`
- `build/realtime-matrix-final/balanced-c1/benchmark_frames.csv` exists
- `build/realtime-matrix-final/balanced-c4/benchmark_summary.json` exists
- `build/realtime-matrix-final/realtime-c4/benchmark_summary.json` exists

- [ ] **Step 4: Record the optimization justification**

Read:

```bash
sed -n '1,120p' build/realtime-matrix-final/realtime-c4/benchmark_summary.json
sed -n '1,120p' build/realtime-matrix-final/balanced-c4/benchmark_summary.json
```

Expected:

- the summary makes `render_ms`, `denoise_ms`, `download_ms`, `image_write_ms`, and `host_overhead_ms` visible enough to explain the selected optimization
- the skip-write path shows `image_write_ms` reduced to zero in benchmark-mode runs

- [ ] **Step 5: Check the worktree**

Run: `git status --short`

Expected: clean working tree

---

## Self-Review

Spec coverage:

- profiling data model and CLI changes: Tasks 1 and 3
- CUDA event instrumentation: Task 2
- CSV/JSON outputs: Tasks 1 and 3
- reproducible matrix: Task 4 and Task 6
- one measured low-risk optimization: Task 5
- focused verification preserved: Task 6

Placeholder scan:

- No `TODO`, `TBD`, or open-ended “choose later” steps remain.
- The only dynamic choice in the plan is the optimization selection rule, and that rule is explicit and concrete.

Type consistency:

- Shared names are fixed throughout the plan:
  - `RunReport`, `FrameStageSample`, `CameraStageSample`
  - `RadianceTiming`, `ProfiledRadianceFrame`
  - `render_radiance_profiled`
  - `test_realtime_benchmark_report`, `test_optix_profiled_render`, `test_render_realtime_profiling_cli`, `test_render_realtime_cli_skip_write`
