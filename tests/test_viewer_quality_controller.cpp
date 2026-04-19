#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

namespace {

rt::RadianceFrame make_frame(int width, int height, float value) {
    return rt::RadianceFrame {
        .width = width,
        .height = height,
        .average_luminance = value,
        .beauty_rgba = std::vector<float>(static_cast<std::size_t>(width * height * 4), value),
    };
}

void begin_converged_frame(rt::viewer::ViewerQualityController& controller,
    const rt::viewer::BodyPose& pose) {
    controller.begin_frame("scene_a", pose);
    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge, "stable frame converges");
}

}  // namespace

int main() {
    const rt::RenderProfile preview_profile {
        .samples_per_pixel = 1,
        .max_bounces = 2,
        .enable_denoise = true,
        .rr_start_bounce = 2,
        .accumulation_reset_rotation_deg = 5.0,
        .accumulation_reset_translation = 0.25,
    };
    const rt::RenderProfile converge_profile {
        .samples_per_pixel = 4,
        .max_bounces = 6,
        .enable_denoise = false,
        .rr_start_bounce = 4,
        .accumulation_reset_rotation_deg = 5.0,
        .accumulation_reset_translation = 0.25,
    };

    rt::viewer::ViewerQualityController controller(preview_profile, converge_profile);
    const rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "first frame starts preview");
    expect_true(controller.active_profile().samples_per_pixel == preview_profile.samples_per_pixel, "preview profile active");

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge, "stable second frame converges");
    expect_true(controller.active_profile().samples_per_pixel == converge_profile.samples_per_pixel, "converge profile active");

    const rt::RadianceFrame converge_frame = make_frame(2, 1, 1.0f);
    const rt::RadianceFrame resolved_first = controller.resolve_frame(0, converge_frame);
    expect_true(resolved_first.width == converge_frame.width, "resolve returns raw frame width");
    expect_true(controller.history_length(0) == 1, "first converge resolve initializes history");
    controller.resolve_frame(0, converge_frame);
    controller.resolve_frame(0, converge_frame);
    expect_true(controller.history_length(0) == 3, "repeated converge resolves grow history");

    controller.resolve_frame(1, converge_frame);
    expect_true(controller.history_length(1) == 1, "other camera maintains independent history");
    expect_true(controller.history_length(0) == 3, "primary camera history remains unchanged");

    const rt::RadianceFrame resized_frame = make_frame(1, 2, 2.0f);
    controller.resolve_frame(0, resized_frame);
    expect_true(controller.history_length(0) == 1, "resolution change resets history for camera");
    expect_true(controller.history_length(1) == 1, "resolution change does not affect other camera");

    rt::viewer::BodyPose rotated = pose;
    rotated.yaw_deg = 12.0;
    controller.begin_frame("scene_a", rotated);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "large rotation returns preview");
    expect_true(controller.history_length(0) == 0, "large rotation clears history");
    expect_true(controller.history_length(1) == 0, "large rotation clears all camera histories");

    controller.begin_frame("scene_b", rotated);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "scene change stays preview");
    expect_true(controller.history_length(0) == 0, "scene change keeps history cleared");

    begin_converged_frame(controller, rotated);
    controller.resolve_frame(0, converge_frame);
    controller.resolve_frame(1, converge_frame);
    expect_true(controller.history_length(0) == 1, "camera 0 repopulates history after reconverge");
    expect_true(controller.history_length(1) == 1, "camera 1 repopulates history after reconverge");

    controller.reset_all();
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "reset_all returns preview mode");
    expect_true(controller.history_length(0) == 0, "reset_all clears camera 0 history");
    expect_true(controller.history_length(1) == 0, "reset_all clears camera 1 history");

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "first frame after reset starts preview");

    return 0;
}
