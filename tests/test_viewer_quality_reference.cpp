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

double beauty_channel_to_display(float value) {
    return std::clamp(std::sqrt(std::max(0.0, static_cast<double>(value))), 0.0, 0.999);
}

double compute_mae_to_reference(const rt::RadianceFrame& frame, const cv::Mat& reference) {
    expect_true(frame.width == reference.cols, "frame width matches reference");
    expect_true(frame.height == reference.rows, "frame height matches reference");
    expect_true(reference.type() == CV_8UC3, "reference image uses CV_8UC3");

    double reference_sum = 0.0;
    double frame_sum = 0.0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel_index = static_cast<std::size_t>(y * frame.width + x) * 4;
            const cv::Vec3b reference_bgr = reference.at<cv::Vec3b>(y, x);
            reference_sum += static_cast<double>(reference_bgr[2]) / 255.0;
            reference_sum += static_cast<double>(reference_bgr[1]) / 255.0;
            reference_sum += static_cast<double>(reference_bgr[0]) / 255.0;
            frame_sum += beauty_channel_to_display(frame.beauty_rgba[pixel_index + 0]);
            frame_sum += beauty_channel_to_display(frame.beauty_rgba[pixel_index + 1]);
            frame_sum += beauty_channel_to_display(frame.beauty_rgba[pixel_index + 2]);
        }
    }
    const double scale = frame_sum > 0.0 ? reference_sum / frame_sum : 1.0;

    double error_sum = 0.0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel_index = static_cast<std::size_t>(y * frame.width + x) * 4;
            const cv::Vec3b reference_bgr = reference.at<cv::Vec3b>(y, x);
            const double reference_r = static_cast<double>(reference_bgr[2]) / 255.0;
            const double reference_g = static_cast<double>(reference_bgr[1]) / 255.0;
            const double reference_b = static_cast<double>(reference_bgr[0]) / 255.0;
            const double frame_r = std::clamp(scale * beauty_channel_to_display(frame.beauty_rgba[pixel_index + 0]), 0.0, 1.0);
            const double frame_g = std::clamp(scale * beauty_channel_to_display(frame.beauty_rgba[pixel_index + 1]), 0.0, 1.0);
            const double frame_b = std::clamp(scale * beauty_channel_to_display(frame.beauty_rgba[pixel_index + 2]), 0.0, 1.0);
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
    constexpr int kAccumulationFrames = 6;

    const rt::SceneDescription scene = rt::viewer::make_final_room_scene();
    const rt::PackedScene packed_scene = scene.pack();
    const rt::viewer::BodyPose pose = rt::default_spawn_pose_for_scene("final_room");
    const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, kWidth, kHeight).pack();

    const cv::Mat cpu_reference = rt::render_shared_scene_from_camera("final_room", rig.cameras[0], 4);

    rt::OptixRenderer renderer;
    renderer.prepare_scene(packed_scene);

    const rt::RenderProfile preview_profile = rt::viewer::default_viewer_preview_profile();
    const rt::RenderProfile converge_profile = rt::viewer::default_viewer_converge_profile();
    rt::viewer::ViewerQualityController controller(preview_profile, converge_profile);

    controller.begin_frame("final_room", pose);
    const rt::RadianceFrame single_frame =
        renderer.render_prepared_radiance(rig, preview_profile, 0).frame;

    controller.begin_frame("final_room", pose);
    rt::RadianceFrame accumulated_frame;
    for (int i = 0; i < kAccumulationFrames; ++i) {
        controller.begin_frame("final_room", pose);
        const rt::RadianceFrame raw_frame =
            renderer.render_prepared_radiance(rig, controller.active_profile(), 0).frame;
        const rt::viewer::ResolvedBeautyFrameView resolved = controller.resolve_beauty_view(0, raw_frame);
        accumulated_frame = rt::viewer::ViewerQualityController::materialize_frame(resolved, raw_frame);
    }

    const double single_error = compute_mae_to_reference(single_frame, cpu_reference);
    const double accumulated_error = compute_mae_to_reference(accumulated_frame, cpu_reference);
    expect_true(accumulated_error < single_error,
        "stationary converge accumulation should improve agreement with CPU reference");
    return 0;
}
