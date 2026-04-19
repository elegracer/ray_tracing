#include "core/offline_shared_scene_renderer.h"
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

    rt::PackedCamera off_center_camera = rig.cameras[0];
    off_center_camera.pinhole.cx += 1.0;
    expect_throws_with_message(
        [&]() { return rt::render_shared_scene_from_camera("final_room", off_center_camera, 1); },
        "centered pinhole cameras", "offline reference rejects unsupported principal point");
    rt::PackedCamera distorted_camera = rig.cameras[0];
    distorted_camera.pinhole.k1 = 0.01;
    expect_throws_with_message(
        [&]() { return rt::render_shared_scene_from_camera("final_room", distorted_camera, 1); },
        "centered pinhole cameras", "offline reference rejects distorted pinhole parameters");
    rt::PackedCamera mismatched_fx_camera = rig.cameras[0];
    mismatched_fx_camera.pinhole.fx += 1.0;
    expect_throws_with_message(
        [&]() { return rt::render_shared_scene_from_camera("final_room", mismatched_fx_camera, 1); },
        "centered pinhole cameras", "offline reference rejects mismatched horizontal focal length");

    const cv::Mat cpu_reference = rt::render_shared_scene_from_camera("final_room", rig.cameras[0], 16);

    rt::OptixRenderer renderer;
    renderer.prepare_scene(packed_scene);

    const rt::RenderProfile preview_profile = rt::viewer::default_viewer_preview_profile();
    const rt::RenderProfile converge_profile = rt::viewer::default_viewer_converge_profile();
    rt::viewer::ViewerQualityController controller(preview_profile, converge_profile);

    controller.begin_frame("final_room", pose);
    const rt::RadianceFrame single_frame =
        renderer.render_prepared_radiance(rig, preview_profile, 0).frame;

    controller.begin_frame("final_room", pose);
    const rt::RadianceFrame first_converge_frame =
        renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
    const rt::RadianceFrame second_converge_frame =
        renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
    expect_true(!frames_match_exactly(first_converge_frame, second_converge_frame),
        "stationary converge renders should introduce new stochastic samples");

    rt::viewer::ViewerQualityController accumulation_controller(preview_profile, converge_profile);
    accumulation_controller.begin_frame("final_room", pose);
    accumulation_controller.begin_frame("final_room", pose);
    rt::RadianceFrame accumulated_frame;
    for (int i = 0; i < kAccumulationFrames; ++i) {
        accumulation_controller.begin_frame("final_room", pose);
        const rt::RadianceFrame raw_frame =
            renderer.render_prepared_radiance(rig, accumulation_controller.active_profile(), 0).frame;
        const rt::viewer::ResolvedBeautyFrameView resolved = accumulation_controller.resolve_beauty_view(0, raw_frame);
        accumulated_frame = rt::viewer::ViewerQualityController::materialize_frame(resolved, raw_frame);
    }

    const double single_error = compute_mae_to_reference(single_frame, cpu_reference);
    const double converge_single_error = compute_mae_to_reference(second_converge_frame, cpu_reference);
    const double accumulated_error = compute_mae_to_reference(accumulated_frame, cpu_reference);
    expect_true(accumulated_error < single_error,
        "stationary converge accumulation should improve agreement with CPU reference");
    expect_true(accumulated_error < converge_single_error,
        "stationary accumulation should improve agreement beyond a single converge-profile frame");
    expect_true(accumulated_error < 0.25,
        "stationary converge accumulation should stay within an absolute CPU-reference error bound");
    return 0;
}
