#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "scene/analytic_light_compiler.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 128;
constexpr int kLightCount = 64;
constexpr int kReferenceSpp = 256;
constexpr int kBaselineSpp = 16;
constexpr int kRestirFrames = 2;
constexpr int kRestirCandidates = 8;
constexpr int kRestirSpatialNeighbors = 1;
constexpr int kPerformanceTrials = 5;
constexpr double kEqualOrBetterQualityRatioLimit = 1.0;

Eigen::Matrix3d legacy_to_world_matrix() {
    Eigen::Matrix3d transform;
    transform.col(0) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitX());
    transform.col(1) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitY());
    transform.col(2) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitZ());
    return transform;
}

rt::SceneDescription make_scene() {
    rt::SceneDescription scene;
    const int receiver =
        scene.add_material(rt::LambertianMaterial {.albedo = Eigen::Vector3d {0.65, 0.58, 0.5}});
    scene.add_quad(rt::QuadPrimitive {
        .material_index = receiver,
        .origin = rt::legacy_renderer_to_world(Eigen::Vector3d {-2.5, -2.5, -4.0}),
        .edge_u = rt::legacy_renderer_to_world(Eigen::Vector3d {5.0, 0.0, 0.0}),
        .edge_v = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 5.0, 0.0}),
        .dynamic = false,
    });
    for (int index = 0; index < 128; ++index) {
        const double x = 20.0 + static_cast<double>(index % 16);
        const double y = -8.0 + static_cast<double>(index / 16);
        const double z = -2.0 - 0.1 * static_cast<double>(index % 7);
        scene.add_sphere(rt::SpherePrimitive {
            .material_index = receiver,
            .center = rt::legacy_renderer_to_world(Eigen::Vector3d {x, y, z}),
            .radius = 0.2,
            .dynamic = false,
        });
    }

    const Eigen::Matrix3d transform = legacy_to_world_matrix();
    std::vector<rt::AnalyticLightDesc> lights;
    lights.reserve(kLightCount);
    for (int index = 0; index < kLightCount; ++index) {
        const int column = index % 8;
        const int row = index / 8;
        const double x = -2.1 + 0.6 * static_cast<double>(column);
        const double y = -2.1 + 0.6 * static_cast<double>(row);
        const double z = -1.5 - 0.15 * static_cast<double>((index * 7) % 5);
        const double strength = 0.35 + 0.08 * static_cast<double>((index * 13) % 17);
        rt::AnalyticLightDesc light;
        light.type = rt::AnalyticLightType::sphere;
        light.position = transform * Eigen::Vector3d {x, y, z};
        light.local_to_world_linear = transform;
        light.radiance = Eigen::Vector3d::Constant(strength);
        light.radius = 0.02;
        light.world_area = 1.0;
        light.treat_as_point = true;
        light.delta = true;
        light.selection_weight = light.radiance.maxCoeff();
        lights.push_back(light);
    }
    rt::finalize_analytic_light_distribution(lights);
    for (const rt::AnalyticLightDesc& light : lights) {
        scene.add_analytic_light(light);
    }
    return scene;
}

rt::PackedCameraRig make_camera_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {135.0, 135.0, 64.0, 64.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), kWidth, kHeight);
    return rig.pack();
}

rt::RenderProfile make_profile(int spp, bool restir, int spatial_neighbors) {
    rt::RenderProfile profile = rt::RenderProfile::realtime();
    profile.samples_per_pixel = spp;
    profile.max_bounces = 1;
    profile.rr_start_bounce = 2;
    profile.enable_denoise = false;
    profile.enable_restir_di = restir;
    profile.restir_initial_candidates = kRestirCandidates;
    profile.restir_temporal_reuse = restir;
    profile.restir_max_history_age = 20;
    profile.restir_max_temporal_candidates = 64;
    profile.restir_spatial_neighbors = restir ? spatial_neighbors : 0;
    profile.restir_max_spatial_candidates = kRestirCandidates;
    profile.restir_min_analytic_lights = 16;
    return profile;
}

void require_frame(const rt::RadianceFrame& frame, const std::string& label) {
    const std::size_t expected = static_cast<std::size_t>(kWidth) * kHeight * 4U;
    if (frame.width != kWidth || frame.height != kHeight || frame.beauty_rgba.size() != expected) {
        throw std::runtime_error(label + " has an invalid frame shape");
    }
    for (float value : frame.beauty_rgba) {
        if (!std::isfinite(value) || value < 0.0f) {
            throw std::runtime_error(label + " contains non-finite or negative radiance");
        }
    }
}

double mean_squared_error(const rt::RadianceFrame& actual, const rt::RadianceFrame& reference) {
    double sum = 0.0;
    const std::size_t pixels = static_cast<std::size_t>(kWidth) * kHeight;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        for (int channel = 0; channel < 3; ++channel) {
            const double delta =
                actual.beauty_rgba[pixel * 4U + static_cast<std::size_t>(channel)]
                - reference.beauty_rgba[pixel * 4U + static_cast<std::size_t>(channel)];
            sum += delta * delta;
        }
    }
    return sum / static_cast<double>(pixels * 3U);
}

std::uint8_t display_channel(float linear) {
    const float mapped = std::max(0.0f, linear) / (1.0f + std::max(0.0f, linear));
    const float encoded = std::sqrt(mapped);
    return static_cast<std::uint8_t>(std::lround(255.0f * std::min(encoded, 1.0f)));
}

void write_png(const std::filesystem::path& path, const rt::RadianceFrame& frame) {
    cv::Mat image(kHeight, kWidth, CV_8UC3);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)) * 4U;
            image.at<cv::Vec3b>(y, x) = cv::Vec3b {display_channel(frame.beauty_rgba[offset + 2]),
                display_channel(frame.beauty_rgba[offset + 1]),
                display_channel(frame.beauty_rgba[offset])};
        }
    }
    if (!cv::imwrite(path.string(), image)) {
        throw std::runtime_error("failed to write " + path.string());
    }
}

double max_display_error(const std::filesystem::path& actual_path,
    const std::filesystem::path& reference_path) {
    const cv::Mat actual = cv::imread(actual_path.string(), cv::IMREAD_COLOR);
    const cv::Mat reference = cv::imread(reference_path.string(), cv::IMREAD_COLOR);
    if (actual.empty() || reference.empty() || actual.size() != reference.size()
        || actual.type() != reference.type()) {
        throw std::runtime_error("invalid ReSTIR reference pair: " + actual_path.string() + " / "
                                 + reference_path.string());
    }
    cv::Mat difference;
    cv::absdiff(actual, reference, difference);
    double maximum = 0.0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &maximum);
    return maximum;
}

void write_json_array(std::ostream& output, const std::vector<double>& values) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << values[index];
    }
    output << ']';
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path output_dir = "restir-acceptance";
    std::filesystem::path reference_dir;
    bool approve_references = false;
    bool expect_quality_rejected = false;
    int spatial_neighbors = kRestirSpatialNeighbors;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--output-dir" && index + 1 < argc) {
            output_dir = argv[++index];
        } else if (argument == "--reference-dir" && index + 1 < argc) {
            reference_dir = argv[++index];
        } else if (argument == "--approve-references") {
            approve_references = true;
        } else if (argument == "--expect-quality-rejected") {
            expect_quality_rejected = true;
        } else if (argument == "--spatial-neighbors" && index + 1 < argc) {
            spatial_neighbors = std::stoi(argv[++index]);
        } else {
            throw std::runtime_error("unknown or incomplete argument: " + argument);
        }
    }
    if (reference_dir.empty()) {
        throw std::runtime_error("--reference-dir is required");
    }
    if (spatial_neighbors < 1 || spatial_neighbors > 8) {
        throw std::runtime_error("--spatial-neighbors must be in [1, 8]");
    }
    std::filesystem::create_directories(output_dir);

    const rt::PackedScene scene = make_scene().pack();
    const rt::PackedCameraRig rig = make_camera_rig();

    rt::OptixRenderer reference_renderer;
    const rt::ProfiledRadianceFrame reference = reference_renderer.render_radiance_profiled(scene,
        rig, make_profile(kReferenceSpp, false, 0), 0);
    require_frame(reference.frame, "reference");

    rt::OptixRenderer baseline_renderer;
    const rt::ProfiledRadianceFrame baseline = baseline_renderer.render_radiance_profiled(scene,
        rig, make_profile(kBaselineSpp, false, 0), 0);
    require_frame(baseline.frame, "baseline");

    rt::OptixRenderer restir_renderer;
    restir_renderer.prepare_scene(scene);
    rt::ProfiledRadianceFrame restir;
    float restir_render_ms = 0.0f;
    rt::RestirDiagnostics first_diagnostics {};
    for (int frame = 0; frame < kRestirFrames; ++frame) {
        restir = restir_renderer.render_prepared_radiance(rig,
            make_profile(1, true, spatial_neighbors), 0);
        restir_render_ms += restir.timing.render_ms;
        if (frame == 0) {
            first_diagnostics = restir_renderer.restir_diagnostics();
        }
    }
    require_frame(restir.frame, "ReSTIR");
    const rt::RestirDiagnostics final_diagnostics = restir_renderer.restir_diagnostics();

    restir_renderer.prepare_scene(scene);
    restir_renderer.render_prepared_radiance(rig, make_profile(1, true, spatial_neighbors), 0);
    const rt::RestirDiagnostics reset_diagnostics = restir_renderer.restir_diagnostics();

    std::vector<double> baseline_render_trials {baseline.timing.render_ms};
    std::vector<double> restir_render_trials {restir_render_ms};
    for (int trial = 1; trial < kPerformanceTrials; ++trial) {
        rt::OptixRenderer baseline_trial_renderer;
        const rt::ProfiledRadianceFrame baseline_trial =
            baseline_trial_renderer.render_radiance_profiled(scene, rig,
                make_profile(kBaselineSpp, false, 0), 0);
        baseline_render_trials.push_back(baseline_trial.timing.render_ms);

        rt::OptixRenderer restir_trial_renderer;
        restir_trial_renderer.prepare_scene(scene);
        double restir_trial_ms = 0.0;
        for (int frame = 0; frame < kRestirFrames; ++frame) {
            const rt::ProfiledRadianceFrame restir_trial =
                restir_trial_renderer.render_prepared_radiance(rig,
                    make_profile(1, true, spatial_neighbors), 0);
            restir_trial_ms += restir_trial.timing.render_ms;
        }
        restir_render_trials.push_back(restir_trial_ms);
    }

    const double baseline_mse = mean_squared_error(baseline.frame, reference.frame);
    const double restir_mse = mean_squared_error(restir.frame, reference.frame);
    const double quality_ratio = baseline_mse > 0.0 ? restir_mse / baseline_mse : 0.0;
    const bool quality_passed = restir_mse <= baseline_mse * kEqualOrBetterQualityRatioLimit + 1e-8;
    const bool quality_expectation_passed =
        expect_quality_rejected ? !quality_passed : quality_passed;
    double baseline_render_mean_ms = 0.0;
    double restir_render_mean_ms = 0.0;
    int performance_win_count = 0;
    for (std::size_t trial = 0; trial < baseline_render_trials.size(); ++trial) {
        baseline_render_mean_ms += baseline_render_trials[trial];
        restir_render_mean_ms += restir_render_trials[trial];
        performance_win_count +=
            restir_render_trials[trial] < baseline_render_trials[trial] ? 1 : 0;
    }
    baseline_render_mean_ms /= static_cast<double>(baseline_render_trials.size());
    restir_render_mean_ms /= static_cast<double>(restir_render_trials.size());
    const bool performance_passed = restir_render_mean_ms < baseline_render_mean_ms
                                    && performance_win_count >= (kPerformanceTrials + 1) / 2;
    const bool temporal_passed =
        first_diagnostics.temporal_reuse_count == 0 && final_diagnostics.temporal_reuse_count > 0
        && final_diagnostics.max_age > 0 && reset_diagnostics.temporal_reuse_count == 0;
    const bool spatial_passed = first_diagnostics.spatial_reuse_count == 0
                                && final_diagnostics.spatial_reuse_count > 0
                                && reset_diagnostics.spatial_reuse_count == 0;

    write_png(output_dir / "reference.png", reference.frame);
    write_png(output_dir / "baseline.png", baseline.frame);
    write_png(output_dir / "restir.png", restir.frame);
    const std::array image_names {"reference.png", "baseline.png", "restir.png"};
    if (approve_references) {
        std::filesystem::create_directories(reference_dir);
        for (const char* image_name : image_names) {
            std::filesystem::copy_file(output_dir / image_name, reference_dir / image_name,
                std::filesystem::copy_options::overwrite_existing);
        }
    }
    double reference_max_display_error = 0.0;
    for (const char* image_name : image_names) {
        reference_max_display_error = std::max(reference_max_display_error,
            max_display_error(output_dir / image_name, reference_dir / image_name));
    }
    const bool reference_gate_passed = reference_max_display_error <= 2.0;
    std::ofstream report(output_dir / "report.json");
    report << std::fixed << std::setprecision(9) << "{\n"
           << "  \"schema_version\": \"restir_di_acceptance_v1\",\n"
           << "  \"light_count\": " << kLightCount << ",\n"
           << "  \"reference_spp\": " << kReferenceSpp << ",\n"
           << "  \"baseline_spp\": " << kBaselineSpp << ",\n"
           << "  \"restir_frames\": " << kRestirFrames << ",\n"
           << "  \"restir_candidates_per_frame\": " << kRestirCandidates << ",\n"
           << "  \"restir_spatial_neighbors\": " << spatial_neighbors << ",\n"
           << "  \"baseline_mse\": " << baseline_mse << ",\n"
           << "  \"restir_mse\": " << restir_mse << ",\n"
           << "  \"quality_mse_ratio\": " << quality_ratio << ",\n"
           << "  \"equal_or_better_quality_mse_ratio_limit\": " << kEqualOrBetterQualityRatioLimit
           << ",\n"
           << "  \"expected_quality_decision\": \""
           << (expect_quality_rejected ? "rejected" : "selected") << "\",\n"
           << "  \"baseline_render_ms\": " << baseline.timing.render_ms << ",\n"
           << "  \"restir_render_ms\": " << restir_render_ms << ",\n"
           << "  \"performance_trial_count\": " << kPerformanceTrials << ",\n"
           << "  \"performance_win_count\": " << performance_win_count << ",\n"
           << "  \"baseline_render_mean_ms\": " << baseline_render_mean_ms << ",\n"
           << "  \"restir_render_mean_ms\": " << restir_render_mean_ms << ",\n"
           << "  \"baseline_render_ms_trials\": ";
    write_json_array(report, baseline_render_trials);
    report << ",\n  \"restir_render_ms_trials\": ";
    write_json_array(report, restir_render_trials);
    report << ",\n"
           << "  \"first_temporal_reuse_pixels\": " << first_diagnostics.temporal_reuse_count
           << ",\n"
           << "  \"final_temporal_reuse_pixels\": " << final_diagnostics.temporal_reuse_count
           << ",\n"
           << "  \"reset_temporal_reuse_pixels\": " << reset_diagnostics.temporal_reuse_count
           << ",\n"
           << "  \"first_spatial_reuse_pixels\": " << first_diagnostics.spatial_reuse_count << ",\n"
           << "  \"final_spatial_reuse_pixels\": " << final_diagnostics.spatial_reuse_count << ",\n"
           << "  \"reset_spatial_reuse_pixels\": " << reset_diagnostics.spatial_reuse_count << ",\n"
           << "  \"reference_max_display_error\": " << reference_max_display_error << ",\n"
           << "  \"reference_gate_passed\": " << (reference_gate_passed ? "true" : "false") << ",\n"
           << "  \"quality_passed\": " << (quality_passed ? "true" : "false") << ",\n"
           << "  \"quality_expectation_passed\": "
           << (quality_expectation_passed ? "true" : "false") << ",\n"
           << "  \"performance_passed\": " << (performance_passed ? "true" : "false") << ",\n"
           << "  \"temporal_validity_passed\": " << (temporal_passed ? "true" : "false") << ",\n"
           << "  \"spatial_validity_passed\": " << (spatial_passed ? "true" : "false") << "\n}\n";
    report.close();

    std::cout << "restir_acceptance baseline_mse=" << baseline_mse << " restir_mse=" << restir_mse
              << " quality_ratio=" << quality_ratio
              << " quality_decision=" << (quality_passed ? "selected" : "rejected")
              << " baseline_mean_ms=" << baseline_render_mean_ms
              << " restir_mean_ms=" << restir_render_mean_ms
              << " performance_wins=" << performance_win_count << '/' << kPerformanceTrials
              << " temporal_pixels=" << final_diagnostics.temporal_reuse_count
              << " spatial_pixels=" << final_diagnostics.spatial_reuse_count << '\n';
    if (!quality_expectation_passed || !performance_passed || !temporal_passed || !spatial_passed
        || !reference_gate_passed) {
        throw std::runtime_error(
            "ReSTIR DI evaluation failed; inspect " + (output_dir / "report.json").string());
    }
    return 0;
}
