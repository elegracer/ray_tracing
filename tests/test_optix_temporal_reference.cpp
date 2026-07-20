#include "realtime/camera_projection.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr int kHistoryFrames = 4;

double display_channel(float value) {
    return std::clamp(std::sqrt(std::max(0.0, static_cast<double>(value))), 0.0, 1.0);
}

double beauty_mae(const rt::RadianceFrame& actual, const rt::RadianceFrame& reference,
    const std::vector<std::uint8_t>* mask = nullptr) {
    expect_true(actual.width == reference.width && actual.height == reference.height,
        "MAE frame dimensions match");
    expect_true(actual.beauty_rgba.size() == reference.beauty_rgba.size(),
        "MAE beauty sizes match");
    if (mask != nullptr) {
        expect_true(mask->size() == static_cast<std::size_t>(actual.width * actual.height),
            "MAE mask dimensions match");
    }

    double error_sum = 0.0;
    std::size_t channel_count = 0;
    for (int pixel = 0; pixel < actual.width * actual.height; ++pixel) {
        if (mask != nullptr && (*mask)[static_cast<std::size_t>(pixel)] == 0U) {
            continue;
        }
        const std::size_t base = static_cast<std::size_t>(pixel) * 4U;
        for (std::size_t channel = 0; channel < 3U; ++channel) {
            error_sum += std::abs(display_channel(actual.beauty_rgba[base + channel])
                                  - display_channel(reference.beauty_rgba[base + channel]));
            ++channel_count;
        }
    }
    expect_true(channel_count > 0U, "MAE includes at least one pixel");
    return error_sum / static_cast<double>(channel_count);
}

std::vector<std::uint8_t> disocclusion_mask(const rt::PackedCamera& previous_camera,
    const rt::RadianceFrame& previous, const rt::PackedCamera& current_camera,
    const rt::RadianceFrame& current) {
    expect_true(previous.width == current.width && previous.height == current.height,
        "disocclusion mask dimensions match");
    expect_true(previous.depth.size() == current.depth.size(),
        "disocclusion mask depth sizes match");

    std::vector<std::uint8_t> mask(previous.depth.size(), 0U);
    const Eigen::Vector3d current_origin = current_camera.T_rc.translation();
    const Eigen::Matrix3d current_rotation = current_camera.T_rc.rotationMatrix();
    const Eigen::Vector3d previous_origin = previous_camera.T_rc.translation();
    const Eigen::Matrix3d previous_rotation = previous_camera.T_rc.rotationMatrix();
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * current.width + x);
            const double current_depth = current.depth[index];
            if (current_depth <= 0.0) {
                continue;
            }

            const Eigen::Vector3d current_ray =
                rt::unproject_camera_pixel(current_camera, Eigen::Vector2d {x + 0.5, y + 0.5});
            const Eigen::Vector3d world_point =
                current_origin + current_rotation * current_ray * current_depth;
            const Eigen::Vector3d previous_camera_point =
                previous_rotation.transpose() * (world_point - previous_origin);
            const Eigen::Vector2d previous_pixel =
                rt::project_camera_pixel(previous_camera, previous_camera_point);
            if (!previous_pixel.allFinite()) {
                mask[index] = 1U;
                continue;
            }
            const int previous_x = static_cast<int>(std::floor(previous_pixel.x()));
            const int previous_y = static_cast<int>(std::floor(previous_pixel.y()));
            if (previous_x < 0 || previous_x >= previous.width || previous_y < 0
                || previous_y >= previous.height) {
                mask[index] = 1U;
                continue;
            }

            const std::size_t previous_index =
                static_cast<std::size_t>(previous_y * previous.width + previous_x);
            const double previous_depth = previous.depth[previous_index];
            const double expected_previous_depth = previous_camera_point.norm();
            const double tolerance =
                std::max(0.02, 0.05 * std::max(previous_depth, expected_previous_depth));
            if (previous_depth <= 0.0
                || std::abs(previous_depth - expected_previous_depth) >= tolerance) {
                mask[index] = 1U;
            }
        }
    }
    return mask;
}

std::size_t count_masked(const std::vector<std::uint8_t>& mask) {
    return static_cast<std::size_t>(std::count(mask.begin(), mask.end(), 1U));
}

void expect_beauty_near(const rt::RadianceFrame& actual, const rt::RadianceFrame& expected,
    double tolerance, const std::string& label) {
    expect_true(actual.width == expected.width && actual.height == expected.height,
        label + " dimensions");
    expect_true(actual.beauty_rgba.size() == expected.beauty_rgba.size(), label + " size");
    for (std::size_t i = 0; i < actual.beauty_rgba.size(); ++i) {
        expect_near(actual.beauty_rgba[i], expected.beauty_rgba[i], tolerance,
            label + " value[" + std::to_string(i) + "]");
    }
}

rt::PackedCameraRig make_rig(const rt::viewer::BodyPose& pose, int width, int height) {
    return rt::viewer::make_default_viewer_rig(pose, width, height).pack();
}

rt::RenderProfile reference_profile() {
    rt::RenderProfile profile = rt::RenderProfile::quality();
    profile.samples_per_pixel = 32;
    profile.max_bounces = 4;
    profile.rr_start_bounce = 3;
    return profile;
}

rt::RadianceFrame render_fresh(const rt::PackedScene& scene, const rt::PackedCameraRig& rig,
    const rt::RenderProfile& profile, std::uint32_t sample_stream) {
    rt::OptixRenderer renderer;
    renderer.prepare_scene(scene);
    renderer.reset_sequence(sample_stream);
    return renderer.render_prepared_radiance(rig, profile, 0).frame;
}

} // namespace

int main() {
    const rt::PackedScene scene = rt::make_realtime_scene("final_room").pack();
    const rt::viewer::BodyPose pose_a = rt::default_spawn_pose_for_scene("final_room");
    rt::viewer::BodyPose pose_b = pose_a;
    pose_b.position.x() += 0.03;

    const rt::PackedCameraRig rig_a = make_rig(pose_a, kWidth, kHeight);
    const rt::PackedCameraRig rig_b = make_rig(pose_b, kWidth, kHeight);
    const rt::RenderProfile temporal_profile = rt::RenderProfile::realtime();

    rt::OptixRenderer history_renderer;
    history_renderer.prepare_scene(scene);
    history_renderer.reset_sequence(100U);
    rt::RadianceFrame history_a;
    for (int i = 0; i < kHistoryFrames; ++i) {
        history_a = history_renderer.render_prepared_radiance(rig_a, temporal_profile, 0).frame;
    }
    const rt::RadianceFrame temporal_b =
        history_renderer.render_prepared_radiance(rig_b, temporal_profile, 0).frame;
    const rt::RadianceFrame fresh_b =
        render_fresh(scene, rig_b, temporal_profile, 100U + kHistoryFrames);

    const rt::RenderProfile high_spp_profile = reference_profile();
    const rt::RadianceFrame reference_a = render_fresh(scene, rig_a, high_spp_profile, 700U);
    const rt::RadianceFrame reference_b = render_fresh(scene, rig_b, high_spp_profile, 701U);
    const std::vector<std::uint8_t> disoccluded =
        disocclusion_mask(rig_a.cameras[0], reference_a, rig_b.cameras[0], reference_b);
    const std::size_t disoccluded_pixel_count = count_masked(disoccluded);
    expect_true(disoccluded_pixel_count >= 4U,
        "motion fixture exposes a non-trivial disocclusion region");

    const double temporal_error = beauty_mae(temporal_b, reference_b);
    const double fresh_error = beauty_mae(fresh_b, reference_b);
    const double temporal_disocclusion_error = beauty_mae(temporal_b, reference_b, &disoccluded);
    const double fresh_disocclusion_error = beauty_mae(fresh_b, reference_b, &disoccluded);
    expect_true(temporal_error < 0.20, "smooth temporal motion stays within an absolute MAE gate");
    expect_true(temporal_error <= fresh_error + 0.015,
        "smooth temporal motion stays within fresh-frame reference tolerance");
    expect_true(temporal_disocclusion_error < 0.20,
        "disoccluded pixels stay within an absolute MAE gate");
    expect_true(temporal_disocclusion_error <= fresh_disocclusion_error + 0.015,
        "disoccluded pixels stay within fresh-frame reference tolerance");
    expect_true(beauty_mae(history_a, temporal_b) > 0.001,
        "motion transition produces a different image");

    rt::OptixRenderer resize_renderer;
    resize_renderer.prepare_scene(scene);
    resize_renderer.reset_sequence(300U);
    for (int i = 0; i < kHistoryFrames; ++i) {
        resize_renderer.render_prepared_radiance(rig_a, temporal_profile, 0);
    }
    const rt::PackedCameraRig resized_rig = make_rig(pose_a, 48, 36);
    const rt::RadianceFrame resized =
        resize_renderer.render_prepared_radiance(resized_rig, temporal_profile, 0).frame;
    const rt::RadianceFrame fresh_resized =
        render_fresh(scene, resized_rig, temporal_profile, 300U + kHistoryFrames);
    expect_beauty_near(resized, fresh_resized, 1e-6,
        "resize clears framebuffer and denoiser history");

    rt::viewer::BodyPose jumped_pose = pose_a;
    jumped_pose.position.x() += 0.75;
    jumped_pose.yaw_deg += 25.0;
    const rt::PackedCameraRig jumped_rig = make_rig(jumped_pose, kWidth, kHeight);
    rt::OptixRenderer jump_renderer;
    jump_renderer.prepare_scene(scene);
    jump_renderer.reset_sequence(500U);
    for (int i = 0; i < kHistoryFrames; ++i) {
        jump_renderer.render_prepared_radiance(rig_a, temporal_profile, 0);
    }
    jump_renderer.reset_accumulation();
    const rt::RadianceFrame jumped =
        jump_renderer.render_prepared_radiance(jumped_rig, temporal_profile, 0).frame;
    const rt::RadianceFrame fresh_jumped =
        render_fresh(scene, jumped_rig, temporal_profile, 500U + kHistoryFrames);
    expect_beauty_near(jumped, fresh_jumped, 1e-6,
        "camera-jump reset matches a same-seed cold start");

    const rt::PackedScene changed_scene = rt::make_realtime_scene("cornell_box").pack();
    const rt::viewer::BodyPose changed_pose = rt::default_spawn_pose_for_scene("cornell_box");
    const rt::PackedCameraRig changed_rig = make_rig(changed_pose, kWidth, kHeight);
    rt::OptixRenderer scene_change_renderer;
    scene_change_renderer.prepare_scene(scene);
    scene_change_renderer.reset_sequence(900U);
    for (int i = 0; i < kHistoryFrames; ++i) {
        scene_change_renderer.render_prepared_radiance(rig_a, temporal_profile, 0);
    }
    scene_change_renderer.prepare_scene(changed_scene);
    const rt::RadianceFrame scene_changed =
        scene_change_renderer.render_prepared_radiance(changed_rig, temporal_profile, 0).frame;
    const rt::RadianceFrame fresh_scene_changed =
        render_fresh(changed_scene, changed_rig, temporal_profile, 0U);
    expect_beauty_near(scene_changed, fresh_scene_changed, 1e-6,
        "scene change matches a same-seed cold start");

    std::cout << "temporal_reference motion_mae=" << temporal_error << " fresh_mae=" << fresh_error
              << " disocclusion_motion_mae=" << temporal_disocclusion_error
              << " disocclusion_fresh_mae=" << fresh_disocclusion_error
              << " disoccluded_pixels=" << disoccluded_pixel_count << "\n";
    return 0;
}
