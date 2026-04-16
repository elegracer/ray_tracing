#include "core/version.h"

#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <opencv2/opencv.hpp>

#include <Eigen/Geometry>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kDefaultWidth = 640;
constexpr int kDefaultHeight = 480;

rt::SceneDescription make_smoke_scene() {
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

rt::CameraRig make_smoke_rig(int camera_count) {
    rt::CameraRig rig;
    const double fx = 0.75 * static_cast<double>(kDefaultWidth);
    const double fy = 0.75 * static_cast<double>(kDefaultHeight);
    const double cx = 0.5 * static_cast<double>(kDefaultWidth);
    const double cy = 0.5 * static_cast<double>(kDefaultHeight);

    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = Eigen::Vector3d {0.03 * static_cast<double>(i), 0.0, 0.0};
        rig.add_pinhole(rt::Pinhole32Params {fx, fy, cx, cy, 0.0, 0.0, 0.0, 0.0, 0.0},
            T_bc, kDefaultWidth, kDefaultHeight);
    }

    return rig;
}

cv::Mat make_beauty_image(const rt::RadianceFrame& frame) {
    cv::Mat image(frame.height, frame.width, CV_8UC3);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel_index = static_cast<std::size_t>(y * frame.width + x);
            const std::size_t rgba_index = pixel_index * 4U;
            const float r = std::clamp(frame.beauty_rgba[rgba_index + 0], 0.0f, 1.0f);
            const float g = std::clamp(frame.beauty_rgba[rgba_index + 1], 0.0f, 1.0f);
            const float b = std::clamp(frame.beauty_rgba[rgba_index + 2], 0.0f, 1.0f);
            image.at<cv::Vec3b>(y, x) = cv::Vec3b {
                static_cast<std::uint8_t>(255.0f * b),
                static_cast<std::uint8_t>(255.0f * g),
                static_cast<std::uint8_t>(255.0f * r),
            };
        }
    }
    return image;
}

void write_frame_image(const std::filesystem::path& output_dir, int frame_index, int camera_index,
    const rt::RadianceFrame& frame) {
    const std::filesystem::path path = output_dir / fmt::format("frame_{:04d}_cam_{}.png",
        frame_index, camera_index);
    if (!cv::imwrite(path.string(), make_beauty_image(frame))) {
        throw std::runtime_error(fmt::format("failed to write {}", path.string()));
    }
}

bool resolve_render_profile(const std::string& profile_name, rt::RenderProfile& profile) {
    if (profile_name == "quality") {
        profile = rt::RenderProfile::quality();
        return true;
    }
    if (profile_name == "balanced") {
        profile = rt::RenderProfile::balanced();
        return true;
    }
    if (profile_name == "realtime") {
        profile = rt::RenderProfile::realtime();
        return true;
    }
    return false;
}

double compute_p95_frame_ms(std::vector<double> frame_times_ms) {
    std::sort(frame_times_ms.begin(), frame_times_ms.end());
    const std::size_t count = frame_times_ms.size();
    const std::size_t p95_index = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(count))) - 1U;
    return frame_times_ms[p95_index];
}

}  // namespace

int main(int argc, const char* argv[]) {
    const std::string version_string = fmt::format("{}.{}.{}.{}", CORE_MAJOR_VERSION,
        CORE_MINOR_VERSION, CORE_PATCH_VERSION, CORE_TWEAK_VERSION);

    int camera_count = 4;
    int frames = 1;
    std::string output_dir = "build/realtime-smoke";
    std::string profile_name = "balanced";

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
    program.add_argument("--output-dir")
        .help("directory for per-camera png outputs")
        .default_value(output_dir)
        .store_into(output_dir);
    program.add_argument("--profile")
        .help("render profile: quality|balanced|realtime")
        .default_value(profile_name)
        .store_into(profile_name);

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

    rt::RenderProfile profile;
    if (!resolve_render_profile(profile_name, profile)) {
        fmt::print(stderr, "--profile must be one of: quality, balanced, realtime\n");
        return EXIT_FAILURE;
    }

    const std::filesystem::path output_path = output_dir;
    std::filesystem::create_directories(output_path);

    const rt::PackedScene packed_scene = make_smoke_scene().pack();
    const rt::PackedCameraRig packed_rig = make_smoke_rig(camera_count).pack();
    rt::OptixRenderer renderer;

    std::vector<double> frame_times_ms;
    frame_times_ms.reserve(static_cast<std::size_t>(frames));

    for (int frame_index = 0; frame_index < frames; ++frame_index) {
        const auto frame_begin = std::chrono::steady_clock::now();
        double frame_luminance_sum = 0.0;
        double render_ms = 0.0;
        double image_write_ms = 0.0;

        for (int camera_index = 0; camera_index < camera_count; ++camera_index) {
            const auto render_begin = std::chrono::steady_clock::now();
            const rt::RadianceFrame frame =
                renderer.render_radiance(packed_scene, packed_rig, profile, camera_index);
            const auto render_end = std::chrono::steady_clock::now();
            render_ms += std::chrono::duration<double, std::milli>(render_end - render_begin).count();
            frame_luminance_sum += frame.average_luminance;

            const auto image_write_begin = std::chrono::steady_clock::now();
            write_frame_image(output_path, frame_index, camera_index, frame);
            const auto image_write_end = std::chrono::steady_clock::now();
            image_write_ms += std::chrono::duration<double, std::milli>(image_write_end - image_write_begin).count();
        }

        const auto frame_end = std::chrono::steady_clock::now();
        const double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_begin).count();
        frame_times_ms.push_back(frame_ms);

        fmt::print("frame={} cameras={} avg_luminance={:.6f} render_ms={:.3f} image_write_ms={:.3f} frame_ms={:.3f}\n",
            frame_index, camera_count, frame_luminance_sum / static_cast<double>(camera_count), render_ms,
            image_write_ms, frame_ms);
    }

    const double total_ms = std::accumulate(frame_times_ms.begin(), frame_times_ms.end(), 0.0);
    const double avg_frame_ms = total_ms / static_cast<double>(frame_times_ms.size());
    const double p95_frame_ms = compute_p95_frame_ms(frame_times_ms);
    const double fps = 1000.0 / avg_frame_ms;

    fmt::print(
        "summary profile={} frames={} cameras={} resolution={}x{} spp={} max_bounces={} denoise={} avg_frame_ms={:.3f} p95_frame_ms={:.3f} fps={:.2f} output_dir={}\n",
        profile_name, frames, camera_count, kDefaultWidth, kDefaultHeight, profile.samples_per_pixel,
        profile.max_bounces, profile.enable_denoise ? "true" : "false", avg_frame_ms, p95_frame_ms, fps,
        output_path.string());
    return EXIT_SUCCESS;
}
