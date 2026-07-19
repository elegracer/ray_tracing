#include "core/version.h"

#include "realtime/build_provenance.h"
#include "realtime/camera_rig.h"
#include "realtime/display_transfer.h"
#include "realtime/gpu/renderer_pool.h"
#include "realtime/profiling/benchmark_report.h"
#include "realtime/render_profile.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/scene_catalog.h"
#include "realtime/scene_description.h"

#include <argparse/argparse.hpp>
#include <cuda_runtime_api.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <optix.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/utsname.h>
#include <vector>

#ifndef RT_BUILD_CONFIGURATION
#define RT_BUILD_CONFIGURATION "unknown"
#endif
#ifndef RT_CXX_COMPILER
#define RT_CXX_COMPILER "unknown"
#endif

namespace {

constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 480;
constexpr int kDefaultWarmupFrames = 8;

void check_cuda(cudaError_t result, const char* operation) {
    if (result != cudaSuccess) {
        throw std::runtime_error(
            fmt::format("{} failed: {}", operation, cudaGetErrorString(result)));
    }
}

struct GpuMemorySnapshot {
    std::uint64_t total_bytes = 0;
    std::uint64_t used_bytes = 0;
};

GpuMemorySnapshot query_gpu_memory() {
    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;
    check_cuda(cudaMemGetInfo(&free_bytes, &total_bytes), "cudaMemGetInfo");
    return GpuMemorySnapshot {
        .total_bytes = static_cast<std::uint64_t>(total_bytes),
        .used_bytes = static_cast<std::uint64_t>(total_bytes - free_bytes),
    };
}

std::string cuda_version_text(int encoded) {
    return fmt::format("{}.{}", encoded / 1000, (encoded % 1000) / 10);
}

std::string optix_version_text(int encoded) {
    return fmt::format("{}.{}.{}", encoded / 10000, (encoded % 10000) / 100, encoded % 100);
}

std::string read_first_line_or(const std::filesystem::path& path, const std::string& fallback) {
    std::ifstream input(path);
    std::string value;
    if (!input.is_open() || !std::getline(input, value)) {
        return fallback;
    }
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return fallback;
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc {};
    if (gmtime_r(&now, &utc) == nullptr) {
        throw std::runtime_error("failed to convert benchmark timestamp to UTC");
    }
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

rt::profiling::RunEnvironment collect_environment() {
    utsname host {};
    if (uname(&host) != 0) {
        throw std::runtime_error("uname failed while collecting benchmark provenance");
    }

    int device = 0;
    int driver_version = 0;
    int runtime_version = 0;
    cudaDeviceProp properties {};
    check_cuda(cudaGetDevice(&device), "cudaGetDevice");
    check_cuda(cudaGetDeviceProperties(&properties, device), "cudaGetDeviceProperties");
    check_cuda(cudaDriverGetVersion(&driver_version), "cudaDriverGetVersion");
    check_cuda(cudaRuntimeGetVersion(&runtime_version), "cudaRuntimeGetVersion");

    return rt::profiling::RunEnvironment {
        .operating_system = fmt::format("{} {}", host.sysname, host.release),
        .architecture = host.machine,
        .gpu_device = device,
        .gpu_name = properties.name,
        .compute_capability = fmt::format("{}.{}", properties.major, properties.minor),
        .nvidia_driver_version = read_first_line_or("/sys/module/nvidia/version", "unavailable"),
        .cuda_driver_api_version = driver_version,
        .cuda_driver_api_version_text = cuda_version_text(driver_version),
        .cuda_runtime_version = runtime_version,
        .cuda_runtime_version_text = cuda_version_text(runtime_version),
        .optix_version = OPTIX_VERSION,
        .optix_version_text = optix_version_text(OPTIX_VERSION),
    };
}

bool is_supported_realtime_scene(const std::string& scene_name) {
    const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(scene_name);
    return entry != nullptr && entry->supports_realtime;
}

rt::SceneDescription make_scene(const std::string& scene_name) {
    return rt::make_realtime_scene(scene_name);
}

rt::CameraRig make_rig(const std::string& scene_name, int camera_count) {
    return rt::default_camera_rig_for_scene(scene_name, camera_count, kDefaultWidth,
        kDefaultHeight);
}

cv::Mat make_beauty_image(const rt::RadianceFrame& frame) {
    cv::Mat image(frame.height, frame.width, CV_8UC3);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel_index = static_cast<std::size_t>(y * frame.width + x);
            const std::size_t rgba_index = pixel_index * 4U;
            image.at<cv::Vec3b>(y, x) = cv::Vec3b {
                rt::linear_to_display_u8(frame.beauty_rgba[rgba_index + 2]),
                rt::linear_to_display_u8(frame.beauty_rgba[rgba_index + 1]),
                rt::linear_to_display_u8(frame.beauty_rgba[rgba_index + 0]),
            };
        }
    }
    return image;
}

void write_frame_image(const std::filesystem::path& output_dir, int frame_index, int camera_index,
    const rt::RadianceFrame& frame) {
    const std::filesystem::path path =
        output_dir / fmt::format("frame_{:04d}_cam_{}.png", frame_index, camera_index);
    if (!cv::imwrite(path.string(), make_beauty_image(frame))) {
        throw std::runtime_error(fmt::format("failed to write {}", path.string()));
    }
}

struct PostprocessResult {
    int camera_index = 0;
    float render_ms = 0.0f;
    float download_ms = 0.0f;
    rt::RadianceFrame frame;
    double denoise_ms = 0.0;
};

} // namespace

int main(int argc, const char* argv[]) {
    const std::string version_string = fmt::format("{}.{}.{}.{}", CORE_MAJOR_VERSION,
        CORE_MINOR_VERSION, CORE_PATCH_VERSION, CORE_TWEAK_VERSION);

    int camera_count = 4;
    int frames = 1;
    int warmup_frames = kDefaultWarmupFrames;
    unsigned int random_seed = 0;
    std::string output_dir = "build/realtime-smoke";
    std::string scene_name = "smoke";
    std::string profile_arg;
    bool skip_image_write = false;

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    std::string profile_name = rt::render_profile_name(profile);

    argparse::ArgumentParser program("render_realtime", version_string);
    program.add_argument("--camera-count")
        .help("active camera count in [1, 4]")
        .scan<'i', int>()
        .default_value(camera_count)
        .store_into(camera_count);
    program.add_argument("--frames")
        .help("number of frames to render")
        .scan<'i', int>()
        .default_value(frames)
        .store_into(frames);
    program.add_argument("--warmup-frames")
        .help("unmeasured frames rendered before benchmark capture")
        .scan<'i', int>()
        .default_value(warmup_frames)
        .store_into(warmup_frames);
    program.add_argument("--seed")
        .help("deterministic initial GPU sample stream")
        .scan<'u', unsigned int>()
        .default_value(random_seed)
        .store_into(random_seed);
    program.add_argument("--output-dir")
        .help("directory for per-camera png outputs")
        .default_value(output_dir)
        .store_into(output_dir);
    program.add_argument("--scene")
        .help("registered realtime scene id")
        .default_value(scene_name)
        .store_into(scene_name);
    program.add_argument("--profile")
        .help("render profile: quality|balanced|realtime")
        .store_into(profile_arg);
    program.add_argument("--skip-image-write")
        .help("benchmark mode: skip PNG writes and keep image_write_ms at zero")
        .default_value(false)
        .implicit_value(true)
        .store_into(skip_image_write);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        fmt::print(stderr, "{}\n\n", err.what());
        fmt::print(stderr, "{}\n", fmt::streamed(program));
        return EXIT_FAILURE;
    }

    if (camera_count < 1 || camera_count > 4) {
        fmt::print(stderr, "--camera-count must be in [1, 4]\n");
        return EXIT_FAILURE;
    }
    if (frames < 1) {
        fmt::print(stderr, "--frames must be >= 1\n");
        return EXIT_FAILURE;
    }
    if (warmup_frames < 0) {
        fmt::print(stderr, "--warmup-frames must be >= 0\n");
        return EXIT_FAILURE;
    }
    const std::uint64_t last_sample_stream = static_cast<std::uint64_t>(random_seed)
                                             + static_cast<std::uint64_t>(warmup_frames)
                                             + static_cast<std::uint64_t>(frames) - 1U;
    if (last_sample_stream > std::numeric_limits<std::uint32_t>::max()) {
        fmt::print(stderr, "--seed + --warmup-frames + --frames exceeds uint32 sample streams\n");
        return EXIT_FAILURE;
    }
    if (!is_supported_realtime_scene(scene_name)) {
        fmt::print(stderr, "--scene must reference a registered realtime scene\n");
        return EXIT_FAILURE;
    }

    if (!profile_arg.empty()) {
        const std::optional<rt::RenderProfile> resolved_profile =
            rt::render_profile_from_name(profile_arg);
        if (!resolved_profile.has_value()) {
            fmt::print(stderr, "--profile must be one of: quality, balanced, realtime\n");
            return EXIT_FAILURE;
        }
        profile = *resolved_profile;
        profile_name = profile_arg;
    }

    const std::filesystem::path output_path = output_dir;
    std::filesystem::create_directories(output_path);

    const rt::profiling::RunEnvironment environment = collect_environment();
    const GpuMemorySnapshot baseline_memory = query_gpu_memory();
    const rt::PackedScene packed_scene = make_scene(scene_name).pack();
    const rt::PackedCameraRig packed_rig = make_rig(scene_name, camera_count).pack();
    rt::RendererPool renderer_pool(camera_count);
    renderer_pool.prepare_scene(packed_scene);
    renderer_pool.reset_sequence(static_cast<std::uint32_t>(random_seed));
    const GpuMemorySnapshot prepared_memory = query_gpu_memory();
    std::uint64_t peak_used_gpu_memory =
        std::max(baseline_memory.used_bytes, prepared_memory.used_bytes);

    rt::profiling::RunReport report {};
    report.provenance = rt::profiling::RunProvenance {
        .captured_at_utc = utc_timestamp(),
        .project_version = version_string,
        .source_revision = RT_SOURCE_REVISION,
        .source_dirty = RT_SOURCE_DIRTY != 0,
        .source_scope = RT_SOURCE_SCOPE,
        .source_state_sha256 = RT_SOURCE_STATE_SHA256,
        .build_configuration = RT_BUILD_CONFIGURATION,
        .cxx_compiler = RT_CXX_COMPILER,
    };
    report.environment = environment;
    report.gpu_memory.total_bytes = baseline_memory.total_bytes;
    report.gpu_memory.baseline_used_bytes = baseline_memory.used_bytes;
    report.gpu_memory.prepared_used_bytes = prepared_memory.used_bytes;
    report.scene = scene_name;
    report.profile = profile_name;
    report.camera_count = camera_count;
    report.width = kDefaultWidth;
    report.height = kDefaultHeight;
    report.frames_requested = frames;
    report.warmup_frames = warmup_frames;
    report.random_seed = static_cast<std::uint32_t>(random_seed);
    report.samples_per_pixel = profile.samples_per_pixel;
    report.max_bounces = profile.max_bounces;
    report.denoise_enabled = profile.enable_denoise;
    report.image_write_enabled = !skip_image_write;
    report.frames.reserve(static_cast<std::size_t>(frames));

    for (int warmup_index = 0; warmup_index < warmup_frames; ++warmup_index) {
        const auto warmup_begin = std::chrono::steady_clock::now();
        std::vector<rt::CameraRenderResult> warmup_results =
            renderer_pool.render_frame(packed_rig, profile, camera_count);
        const auto warmup_end = std::chrono::steady_clock::now();
        const GpuMemorySnapshot warmup_memory = query_gpu_memory();
        peak_used_gpu_memory = std::max(peak_used_gpu_memory, warmup_memory.used_bytes);
        const double warmup_ms =
            std::chrono::duration<double, std::milli>(warmup_end - warmup_begin).count();
        fmt::print("warmup={} sample_stream={} cameras={} pipeline_ms={:.3f}\n", warmup_index,
            static_cast<std::uint64_t>(random_seed) + static_cast<std::uint64_t>(warmup_index),
            warmup_results.size(), warmup_ms);
    }

    for (int frame_index = 0; frame_index < frames; ++frame_index) {
        const auto frame_begin = std::chrono::steady_clock::now();
        rt::profiling::FrameStageSample frame_record {};
        frame_record.frame_index = frame_index;
        frame_record.sample_stream = static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(random_seed) + static_cast<std::uint64_t>(warmup_frames)
            + static_cast<std::uint64_t>(frame_index));
        frame_record.camera_count = camera_count;
        frame_record.profile = profile_name;
        frame_record.width = kDefaultWidth;
        frame_record.height = kDefaultHeight;
        frame_record.samples_per_pixel = profile.samples_per_pixel;
        frame_record.max_bounces = profile.max_bounces;
        frame_record.denoise_enabled = profile.enable_denoise;
        frame_record.cameras.reserve(static_cast<std::size_t>(camera_count));

        double frame_luminance_sum = 0.0;
        const auto pipeline_begin = std::chrono::steady_clock::now();
        std::vector<rt::CameraRenderResult> camera_results =
            renderer_pool.render_frame(packed_rig, profile, camera_count);
        const auto pipeline_end = std::chrono::steady_clock::now();
        frame_record.pipeline_ms =
            std::chrono::duration<double, std::milli>(pipeline_end - pipeline_begin).count();
        std::sort(camera_results.begin(), camera_results.end(),
            [](const rt::CameraRenderResult& lhs, const rt::CameraRenderResult& rhs) {
                return lhs.camera_index < rhs.camera_index;
            });

        std::vector<PostprocessResult> postprocessed;
        postprocessed.reserve(camera_results.size());
        for (rt::CameraRenderResult& result : camera_results) {
            PostprocessResult out {};
            out.camera_index = result.camera_index;
            out.render_ms = result.profiled.timing.render_ms;
            out.denoise_ms = result.profiled.timing.denoise_ms;
            out.download_ms = result.profiled.timing.download_ms;
            out.frame = std::move(result.profiled.frame);
            postprocessed.push_back(std::move(out));
        }
        std::sort(postprocessed.begin(), postprocessed.end(),
            [](const PostprocessResult& lhs, const PostprocessResult& rhs) {
                return lhs.camera_index < rhs.camera_index;
            });

        for (PostprocessResult& item : postprocessed) {
            frame_record.render_ms =
                std::max(frame_record.render_ms, static_cast<double>(item.render_ms));
            frame_record.denoise_ms = std::max(frame_record.denoise_ms, item.denoise_ms);
            frame_record.download_ms =
                std::max(frame_record.download_ms, static_cast<double>(item.download_ms));
            frame_record.render_work_ms += static_cast<double>(item.render_ms);
            frame_record.denoise_work_ms += item.denoise_ms;
            frame_record.download_work_ms += static_cast<double>(item.download_ms);
            frame_luminance_sum += item.frame.average_luminance;

            if (!skip_image_write) {
                const auto image_write_begin = std::chrono::steady_clock::now();
                write_frame_image(output_path, frame_index, item.camera_index, item.frame);
                const auto image_write_end = std::chrono::steady_clock::now();
                const double image_write_ms =
                    std::chrono::duration<double, std::milli>(image_write_end - image_write_begin)
                        .count();
                frame_record.image_write_ms += image_write_ms;
            }

            frame_record.cameras.push_back(rt::profiling::CameraStageSample {
                .camera_index = item.camera_index,
                .render_ms = static_cast<double>(item.render_ms),
                .denoise_ms = item.denoise_ms,
                .download_ms = static_cast<double>(item.download_ms),
                .average_luminance = item.frame.average_luminance,
            });
        }

        const auto frame_end = std::chrono::steady_clock::now();
        frame_record.frame_ms =
            std::chrono::duration<double, std::milli>(frame_end - frame_begin).count();
        frame_record.host_overhead_ms = std::max(0.0,
            frame_record.frame_ms - frame_record.pipeline_ms - frame_record.image_write_ms);
        frame_record.fps = 1000.0 / frame_record.frame_ms;
        report.frames.push_back(frame_record);
        const GpuMemorySnapshot frame_memory = query_gpu_memory();
        peak_used_gpu_memory = std::max(peak_used_gpu_memory, frame_memory.used_bytes);

        fmt::print(
            "frame={} sample_stream={} cameras={} avg_luminance={:.6f} pipeline_ms={:.3f} "
            "render_ms={:.3f} "
            "denoise_ms={:.3f} download_ms={:.3f} render_work_ms={:.3f} denoise_work_ms={:.3f} "
            "download_work_ms={:.3f} image_write_ms={:.3f} host_overhead_ms={:.3f} "
            "frame_ms={:.3f}\n",
            frame_index, frame_record.sample_stream, camera_count,
            frame_luminance_sum / static_cast<double>(camera_count), frame_record.pipeline_ms,
            frame_record.render_ms, frame_record.denoise_ms, frame_record.download_ms,
            frame_record.render_work_ms, frame_record.denoise_work_ms,
            frame_record.download_work_ms, frame_record.image_write_ms,
            frame_record.host_overhead_ms, frame_record.frame_ms);
    }

    const GpuMemorySnapshot final_memory = query_gpu_memory();
    peak_used_gpu_memory = std::max(peak_used_gpu_memory, final_memory.used_bytes);
    report.gpu_memory.peak_used_bytes = peak_used_gpu_memory;
    report.gpu_memory.peak_delta_bytes = peak_used_gpu_memory >= baseline_memory.used_bytes
                                             ? peak_used_gpu_memory - baseline_memory.used_bytes
                                             : 0;
    report.gpu_memory.final_used_bytes = final_memory.used_bytes;
    report.aggregate = rt::profiling::compute_aggregate(report.frames);
    const std::filesystem::path csv_path = output_path / "benchmark_frames.csv";
    const std::filesystem::path json_path = output_path / "benchmark_summary.json";
    const std::filesystem::path manifest_path = output_path / "benchmark_manifest.json";
    rt::profiling::write_csv(report, csv_path);
    rt::profiling::write_json(report, json_path);
    rt::profiling::write_artifact_manifest(report, {csv_path, json_path}, manifest_path);

    const double avg_frame_ms = report.aggregate.frame_ms.avg;
    const double p95_frame_ms = report.aggregate.frame_ms.p95;
    const double p99_frame_ms = report.aggregate.frame_ms.p99;
    const double avg_denoise_ms = report.aggregate.denoise_ms.avg;
    const double fps = 1000.0 / avg_frame_ms;
    const double peak_gpu_memory_mib =
        static_cast<double>(report.gpu_memory.peak_used_bytes) / (1024.0 * 1024.0);
    const double peak_gpu_memory_delta_mib =
        static_cast<double>(report.gpu_memory.peak_delta_bytes) / (1024.0 * 1024.0);

    fmt::print(
        "summary profile={} warmup_frames={} frames={} seed={} cameras={} resolution={}x{} spp={} "
        "max_bounces={} denoise={} avg_frame_ms={:.3f} avg_denoise_ms={:.3f} "
        "p95_frame_ms={:.3f} p99_frame_ms={:.3f} fps={:.2f} peak_gpu_memory_mib={:.2f} "
        "peak_gpu_memory_delta_mib={:.2f} "
        "artifacts=benchmark_frames.csv,benchmark_summary.json,benchmark_manifest.json "
        "output_dir={}\n",
        profile_name, warmup_frames, frames, random_seed, camera_count, kDefaultWidth,
        kDefaultHeight, profile.samples_per_pixel, profile.max_bounces,
        profile.enable_denoise ? "true" : "false", avg_frame_ms, avg_denoise_ms, p95_frame_ms,
        p99_frame_ms, fps, peak_gpu_memory_mib, peak_gpu_memory_delta_mib, output_path.string());
    return EXIT_SUCCESS;
}
