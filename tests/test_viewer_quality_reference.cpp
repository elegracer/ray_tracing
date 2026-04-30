#include "core/offline_shared_scene_renderer.h"
#include "realtime/camera_models.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

#include <opencv2/core.hpp>
#include <tbb/global_control.h>

#include <algorithm>
#include <cmath>

namespace {

template <typename Fn>
void expect_throws_with_message(Fn&& fn, const std::string& message, const std::string& label) {
    bool threw = false;
    bool matched = false;
    try {
        fn();
    } catch (const std::exception& ex) {
        threw = true;
        matched = std::string(ex.what()).find(message) != std::string::npos;
    } catch (...) {
        threw = true;
    }
    expect_true(threw, label + " threw");
    expect_true(matched, label + " message");
}

double beauty_channel_to_display(float value) {
    return std::clamp(std::sqrt(std::max(0.0, static_cast<double>(value))), 0.0, 0.999);
}

bool frames_match_exactly(const rt::RadianceFrame& a, const rt::RadianceFrame& b) {
    return a.width == b.width && a.height == b.height && a.beauty_rgba == b.beauty_rgba;
}

rt::RadianceFrame accumulate_stationary_frame(rt::OptixRenderer& renderer,
    const rt::PackedCameraRig& rig,
    const rt::RenderProfile& preview_profile,
    const rt::RenderProfile& converge_profile,
    std::string_view scene_id,
    const rt::viewer::BodyPose& pose,
    int accumulation_frames) {
    rt::viewer::ViewerQualityController controller(preview_profile, converge_profile);
    controller.begin_frame(scene_id, pose);
    controller.begin_frame(scene_id, pose);

    rt::RadianceFrame accumulated_frame;
    for (int i = 0; i < accumulation_frames; ++i) {
        controller.begin_frame(scene_id, pose);
        const rt::RadianceFrame raw_frame = renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
        const rt::viewer::ResolvedBeautyFrameView resolved = controller.resolve_beauty_view(0, raw_frame);
        accumulated_frame = rt::viewer::ViewerQualityController::materialize_frame(resolved, raw_frame);
    }
    return accumulated_frame;
}

double compute_mae_to_reference(const rt::RadianceFrame& frame, const cv::Mat& reference) {
    expect_true(frame.width == reference.cols, "frame width matches reference");
    expect_true(frame.height == reference.rows, "frame height matches reference");
    expect_true(reference.type() == CV_8UC3, "reference image uses CV_8UC3");

    double error_sum = 0.0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel_index = static_cast<std::size_t>(y * frame.width + x) * 4;
            const cv::Vec3b reference_bgr = reference.at<cv::Vec3b>(y, x);
            const double reference_r = static_cast<double>(reference_bgr[2]) / 255.0;
            const double reference_g = static_cast<double>(reference_bgr[1]) / 255.0;
            const double reference_b = static_cast<double>(reference_bgr[0]) / 255.0;
            const double frame_r = beauty_channel_to_display(frame.beauty_rgba[pixel_index + 0]);
            const double frame_g = beauty_channel_to_display(frame.beauty_rgba[pixel_index + 1]);
            const double frame_b = beauty_channel_to_display(frame.beauty_rgba[pixel_index + 2]);
            error_sum += std::abs(frame_r - reference_r);
            error_sum += std::abs(frame_g - reference_g);
            error_sum += std::abs(frame_b - reference_b);
        }
    }
    return error_sum / static_cast<double>(frame.width * frame.height * 3);
}

}  // namespace

int main() {
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);

    constexpr int kWidth = 64;
    constexpr int kHeight = 48;
    constexpr int kAccumulationFrames = 24;

    const rt::SceneDescription scene = rt::viewer::make_final_room_scene();
    const rt::PackedScene packed_scene = scene.pack();
    const rt::viewer::BodyPose pose = rt::default_spawn_pose_for_scene("final_room");
    const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, kWidth, kHeight).pack();
    expect_true(rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "default viewer rig now uses equi");
    const cv::Mat cpu_reference = rt::render_shared_scene_from_camera("final_room", rig.cameras[0], 16);

    rt::PackedCamera equi_camera = rig.cameras[0];
    equi_camera.model = rt::CameraModelType::equi62_lut1d;
    equi_camera.equi = rt::make_equi62_lut1d_params(kWidth, kHeight, 18.0, 18.0, 31.5, 23.5,
        std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d::Zero());
    const cv::Mat equi_reference = rt::render_shared_scene_from_camera("final_room", equi_camera, 1);
    expect_true(!equi_reference.empty(), "offline reference accepts equi packed cameras");
    expect_true(equi_reference.cols == kWidth && equi_reference.rows == kHeight,
        "equi packed-camera reference keeps rig dimensions");
    expect_true(cv::norm(equi_reference, cpu_reference, cv::NORM_L1) > 0.0,
        "equi packed-camera reference changes the CPU image");

    const rt::PackedCamera explicit_pinhole_camera =
        rt::default_camera_rig_for_scene("final_room", 1, kWidth, kHeight).pack().cameras[0];

    rt::PackedCamera off_center_camera = explicit_pinhole_camera;
    off_center_camera.pinhole.cx += 1.0;
    const cv::Mat off_center_reference = rt::render_shared_scene_from_camera("final_room", off_center_camera, 1);
    expect_true(!off_center_reference.empty(), "offline reference accepts off-center pinhole camera");
    expect_true(off_center_reference.cols == kWidth && off_center_reference.rows == kHeight,
        "off-center pinhole reference keeps rig dimensions");
    expect_true(cv::norm(off_center_reference, cpu_reference, cv::NORM_L1) > 0.0,
        "off-center pinhole reference changes the CPU image");

    rt::PackedCamera distorted_camera = explicit_pinhole_camera;
    distorted_camera.pinhole.k1 = 0.01;
    distorted_camera.pinhole.k2 = -0.002;
    const cv::Mat distorted_reference = rt::render_shared_scene_from_camera("final_room", distorted_camera, 1);
    expect_true(!distorted_reference.empty(), "offline reference accepts distorted pinhole camera");
    expect_true(cv::norm(distorted_reference, cpu_reference, cv::NORM_L1) > 0.0,
        "distorted pinhole reference changes the CPU image");

    rt::PackedCamera mismatched_fx_camera = explicit_pinhole_camera;
    mismatched_fx_camera.pinhole.fx += 1.0;
    const cv::Mat mismatched_fx_reference = rt::render_shared_scene_from_camera("final_room", mismatched_fx_camera, 1);
    expect_true(!mismatched_fx_reference.empty(), "offline reference accepts non-square pinhole focal lengths");
    expect_true(cv::norm(mismatched_fx_reference, cpu_reference, cv::NORM_L1) > 0.0,
        "non-square pinhole focal lengths change the CPU image");

    rt::PackedCamera invalid_pinhole_camera = explicit_pinhole_camera;
    invalid_pinhole_camera.pinhole.fx = 0.0;
    expect_throws_with_message(
        [&]() { return rt::render_shared_scene_from_camera("final_room", invalid_pinhole_camera, 1); },
        "packed pinhole focal lengths must be positive",
        "offline reference rejects non-positive pinhole focal lengths");

    rt::PackedCamera invalid_equi_camera = equi_camera;
    invalid_equi_camera.width = 0;
    expect_throws_with_message(
        [&]() { return rt::render_shared_scene_from_camera("final_room", invalid_equi_camera, 1); },
        "packed camera dimensions must be positive",
        "offline reference rejects non-positive packed-camera dimensions");

    rt::OptixRenderer renderer;
    renderer.prepare_scene(packed_scene);

    const rt::RenderProfile preview_profile = rt::viewer::default_viewer_preview_profile();
    const rt::RenderProfile converge_profile = rt::viewer::default_viewer_converge_profile();
    rt::viewer::ViewerQualityController controller(preview_profile, converge_profile);

    controller.begin_frame("final_room", pose);
    const rt::RadianceFrame single_frame =
        renderer.render_prepared_radiance(rig, preview_profile, 0).frame;
    expect_true(single_frame.average_luminance > 0.0,
        "preview frame should be lit");

    controller.begin_frame("final_room", pose);
    const rt::RadianceFrame first_converge_frame =
        renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
    const rt::RadianceFrame second_converge_frame =
        renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
    expect_true(!frames_match_exactly(first_converge_frame, second_converge_frame),
        "stationary converge renders should introduce new stochastic samples");

    rt::viewer::ViewerQualityController accumulation_controller(preview_profile, converge_profile);
    accumulation_controller.begin_frame("final_room", pose);
    renderer.reset_accumulation();
    accumulation_controller.begin_frame("final_room", pose);
    rt::RadianceFrame accumulated_frame;
    for (int i = 0; i < kAccumulationFrames; ++i) {
        accumulation_controller.begin_frame("final_room", pose);
        const rt::RadianceFrame raw_frame =
            renderer.render_prepared_radiance(rig, accumulation_controller.active_profile(), 0).frame;
        const rt::viewer::ResolvedBeautyFrameView resolved = accumulation_controller.resolve_beauty_view(0, raw_frame);
        accumulated_frame = rt::viewer::ViewerQualityController::materialize_frame(resolved, raw_frame);
    }

    const double accumulated_error = compute_mae_to_reference(accumulated_frame, cpu_reference);
    expect_true(accumulated_error < 0.25,
        "stationary converge accumulation should stay within an absolute CPU-reference error bound");

    const rt::SceneDescription cornell_scene = rt::make_realtime_scene("cornell_box");
    const rt::PackedScene packed_cornell_scene = cornell_scene.pack();
    const rt::viewer::BodyPose cornell_pose = rt::default_spawn_pose_for_scene("cornell_box");
    const rt::PackedCameraRig cornell_rig = rt::viewer::make_default_viewer_rig(
                                                cornell_pose,
                                                kWidth,
                                                kHeight,
                                                rt::viewer_frame_convention_for_scene("cornell_box"))
                                                .pack();
    const cv::Mat cornell_reference = rt::render_shared_scene_from_camera("cornell_box", cornell_rig.cameras[0], 64);

    renderer.prepare_scene(packed_cornell_scene);
    const rt::RadianceFrame cornell_short_accumulation =
        accumulate_stationary_frame(renderer, cornell_rig, preview_profile, converge_profile, "cornell_box", cornell_pose, 4);
    const rt::RadianceFrame cornell_long_accumulation =
        accumulate_stationary_frame(renderer, cornell_rig, preview_profile, converge_profile, "cornell_box", cornell_pose, 24);
    const double cornell_short_error = compute_mae_to_reference(cornell_short_accumulation, cornell_reference);
    const double cornell_long_error = compute_mae_to_reference(cornell_long_accumulation, cornell_reference);
    expect_true(cornell_long_error <= cornell_short_error + 0.02,
        "black-background stationary accumulation should not drift farther from the CPU reference over time");
    return 0;
}
