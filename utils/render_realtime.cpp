#include "core/version.h"

#include "realtime/camera_rig.h"
#include "realtime/gpu/denoiser.h"
#include "realtime/gpu/renderer_pool.h"
#include "realtime/profiling/benchmark_report.h"
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
#include <future>
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

rt::SceneDescription make_final_room_scene() {
    rt::SceneDescription scene;
    const int white = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.73, 0.73, 0.73}});
    const int green = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.30, 0.70, 0.35}});
    const int red = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.72, 0.25, 0.22}});
    const int blue = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.25, 0.35, 0.75}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {12.0, 12.0, 12.0}});

    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, 3.5, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        green,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        red,
        Eigen::Vector3d {4.0, -1.0, -4.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        blue,
        Eigen::Vector3d {-4.0, -1.0, 4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        false,
    });

    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-1.0, 3.15, -1.0},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 2.0},
        false,
    });

    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-3.2, -0.25, -3.0},
        Eigen::Vector3d {1.8, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.8},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {1.2, 0.15, 1.0},
        Eigen::Vector3d {1.6, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.6},
        false,
    });
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {0.0, 0.1, 0.0}, 0.75, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-1.6, 0.35, 1.7}, 0.55, false});

    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-3.1, 1.0, 0.8}, 0.55, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {3.0, 1.35, -0.9}, 0.65, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {1.1, 1.1, -3.0}, 0.60, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-0.8, 2.55, 2.2}, 0.45, false});

    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {0.9, 0.55, -0.1}, 0.45, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-0.35, 0.4, -1.15}, 0.35, false});

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

rt::SceneDescription make_scene(const std::string& scene_name) {
    if (scene_name == "final_room") {
        return make_final_room_scene();
    }
    return make_smoke_scene();
}

rt::CameraRig make_rig(const std::string& scene_name, int camera_count) {
    if (scene_name == "final_room") {
        return make_smoke_rig(camera_count);
    }
    return make_smoke_rig(camera_count);
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

bool render_profiles_equal(const rt::RenderProfile& lhs, const rt::RenderProfile& rhs) {
    return lhs.samples_per_pixel == rhs.samples_per_pixel && lhs.max_bounces == rhs.max_bounces &&
        lhs.enable_denoise == rhs.enable_denoise && lhs.rr_start_bounce == rhs.rr_start_bounce &&
        lhs.accumulation_reset_rotation_deg == rhs.accumulation_reset_rotation_deg &&
        lhs.accumulation_reset_translation == rhs.accumulation_reset_translation;
}

std::string render_profile_name(const rt::RenderProfile& profile) {
    if (render_profiles_equal(profile, rt::RenderProfile::quality())) {
        return "quality";
    }
    if (render_profiles_equal(profile, rt::RenderProfile::balanced())) {
        return "balanced";
    }
    if (render_profiles_equal(profile, rt::RenderProfile::realtime())) {
        return "realtime";
    }
    return "default";
}

struct PostprocessResult {
    int camera_index = 0;
    float render_ms = 0.0f;
    float download_ms = 0.0f;
    rt::RadianceFrame frame;
    double denoise_ms = 0.0;
};

}  // namespace

int main(int argc, const char* argv[]) {
    const std::string version_string = fmt::format("{}.{}.{}.{}", CORE_MAJOR_VERSION,
        CORE_MINOR_VERSION, CORE_PATCH_VERSION, CORE_TWEAK_VERSION);

    int camera_count = 4;
    int frames = 1;
    std::string output_dir = "build/realtime-smoke";
    std::string scene_name = "smoke";
    std::string profile_arg;
    bool skip_image_write = false;

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    std::string profile_name = render_profile_name(profile);

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
    program.add_argument("--scene")
        .help("realtime scene: smoke|final_room")
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
    if (scene_name != "smoke" && scene_name != "final_room") {
        fmt::print(stderr, "--scene must be one of: smoke, final_room\n");
        return EXIT_FAILURE;
    }

    if (!profile_arg.empty()) {
        if (!resolve_render_profile(profile_arg, profile)) {
            fmt::print(stderr, "--profile must be one of: quality, balanced, realtime\n");
            return EXIT_FAILURE;
        }
        profile_name = profile_arg;
    }

    const std::filesystem::path output_path = output_dir;
    std::filesystem::create_directories(output_path);

    const rt::PackedScene packed_scene = make_scene(scene_name).pack();
    const rt::PackedCameraRig packed_rig = make_rig(scene_name, camera_count).pack();
    rt::RendererPool renderer_pool(camera_count);
    renderer_pool.prepare_scene(packed_scene);
    std::vector<rt::OptixDenoiserWrapper> denoisers(
        static_cast<std::size_t>(profile.enable_denoise ? camera_count : 0));

    rt::profiling::RunReport report {};
    report.profile = profile_name;
    report.camera_count = camera_count;
    report.width = kDefaultWidth;
    report.height = kDefaultHeight;
    report.frames_requested = frames;
    report.samples_per_pixel = profile.samples_per_pixel;
    report.max_bounces = profile.max_bounces;
    report.denoise_enabled = profile.enable_denoise;
    report.frames.reserve(static_cast<std::size_t>(frames));

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
        frame_record.cameras.reserve(static_cast<std::size_t>(camera_count));

        double frame_luminance_sum = 0.0;
        std::vector<rt::CameraRenderResult> camera_results =
            renderer_pool.render_frame(packed_rig, profile, camera_count);
        std::sort(camera_results.begin(), camera_results.end(),
            [](const rt::CameraRenderResult& lhs, const rt::CameraRenderResult& rhs) {
                return lhs.camera_index < rhs.camera_index;
            });

        std::vector<PostprocessResult> postprocessed;
        postprocessed.reserve(camera_results.size());
        if (profile.enable_denoise) {
            std::vector<std::future<PostprocessResult>> postprocess_futures;
            postprocess_futures.reserve(camera_results.size());
            for (rt::CameraRenderResult& result : camera_results) {
                rt::OptixDenoiserWrapper& camera_denoiser =
                    denoisers.at(static_cast<std::size_t>(result.camera_index));
                postprocess_futures.push_back(std::async(std::launch::async,
                    [denoiser = &camera_denoiser, result = std::move(result)]() mutable {
                        PostprocessResult out {};
                        out.camera_index = result.camera_index;
                        out.render_ms = result.profiled.timing.render_ms;
                        out.download_ms = result.profiled.timing.download_ms;
                        out.frame = std::move(result.profiled.frame);
                        const auto denoise_begin = std::chrono::steady_clock::now();
                        denoiser->run(out.frame);
                        const auto denoise_end = std::chrono::steady_clock::now();
                        out.denoise_ms =
                            std::chrono::duration<double, std::milli>(denoise_end - denoise_begin).count();
                        return out;
                    }));
            }
            for (std::future<PostprocessResult>& future : postprocess_futures) {
                postprocessed.push_back(future.get());
            }
        } else {
            for (rt::CameraRenderResult& result : camera_results) {
                PostprocessResult out {};
                out.camera_index = result.camera_index;
                out.render_ms = result.profiled.timing.render_ms;
                out.download_ms = result.profiled.timing.download_ms;
                out.frame = std::move(result.profiled.frame);
                postprocessed.push_back(std::move(out));
            }
        }
        std::sort(postprocessed.begin(), postprocessed.end(),
            [](const PostprocessResult& lhs, const PostprocessResult& rhs) {
                return lhs.camera_index < rhs.camera_index;
            });

        for (PostprocessResult& item : postprocessed) {
            frame_record.render_ms += static_cast<double>(item.render_ms);
            frame_record.download_ms += static_cast<double>(item.download_ms);
            frame_record.denoise_ms += item.denoise_ms;
            frame_luminance_sum += item.frame.average_luminance;

            if (!skip_image_write) {
                const auto image_write_begin = std::chrono::steady_clock::now();
                write_frame_image(output_path, frame_index, item.camera_index, item.frame);
                const auto image_write_end = std::chrono::steady_clock::now();
                const double image_write_ms =
                    std::chrono::duration<double, std::milli>(image_write_end - image_write_begin).count();
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
        frame_record.frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_begin).count();
        frame_record.host_overhead_ms = frame_record.frame_ms - frame_record.render_ms - frame_record.denoise_ms
            - frame_record.download_ms - frame_record.image_write_ms;
        frame_record.fps = 1000.0 / frame_record.frame_ms;
        report.frames.push_back(frame_record);

        fmt::print(
            "frame={} cameras={} avg_luminance={:.6f} render_ms={:.3f} denoise_ms={:.3f} download_ms={:.3f} image_write_ms={:.3f} host_overhead_ms={:.3f} frame_ms={:.3f}\n",
            frame_index, camera_count, frame_luminance_sum / static_cast<double>(camera_count), frame_record.render_ms,
            frame_record.denoise_ms, frame_record.download_ms, frame_record.image_write_ms,
            frame_record.host_overhead_ms, frame_record.frame_ms);
    }

    report.aggregate = rt::profiling::compute_aggregate(report.frames);
    rt::profiling::write_csv(report, output_path / "benchmark_frames.csv");
    rt::profiling::write_json(report, output_path / "benchmark_summary.json");

    const double avg_frame_ms = report.aggregate.frame_ms.avg;
    const double p95_frame_ms = report.aggregate.frame_ms.p95;
    const double avg_denoise_ms = report.aggregate.denoise_ms.avg;
    const double fps = 1000.0 / avg_frame_ms;

    fmt::print(
        "summary profile={} frames={} cameras={} resolution={}x{} spp={} max_bounces={} denoise={} avg_frame_ms={:.3f} avg_denoise_ms={:.3f} p95_frame_ms={:.3f} fps={:.2f} output_dir={}\n",
        profile_name, frames, camera_count, kDefaultWidth, kDefaultHeight, profile.samples_per_pixel,
        profile.max_bounces, profile.enable_denoise ? "true" : "false", avg_frame_ms, avg_denoise_ms, p95_frame_ms, fps,
        output_path.string());
    return EXIT_SUCCESS;
}
