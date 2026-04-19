#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

#include <limits>

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
    const rt::RadianceFrame resolved_second = controller.resolve_frame(0, converge_frame);
    expect_true(controller.history_length(0) == 2, "repeated converge resolves grow history to two frames");
    expect_true(resolved_second.beauty_rgba[0] == 1.0f, "identical converge frames keep accumulated beauty");
    controller.resolve_frame(0, converge_frame);
    expect_true(controller.history_length(0) == 3, "later converge resolves continue growing history");

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
    begin_converged_frame(controller, pose);
    rt::RadianceFrame average_first = make_frame(1, 1, 0.20f);
    average_first.beauty_rgba[3] = 1.0f;
    rt::RadianceFrame average_second = make_frame(1, 1, 0.60f);
    average_second.beauty_rgba[3] = 1.0f;
    const rt::RadianceFrame averaged_first = controller.resolve_frame(0, average_first);
    const rt::RadianceFrame averaged_second = controller.resolve_frame(0, average_second);
    expect_near(averaged_first.beauty_rgba[0], 0.20f, 1e-6, "first converge frame seeds accumulated beauty");
    expect_near(averaged_second.beauty_rgba[0], 0.40f, 1e-6, "converge mode averages beauty across frames");
    expect_true(controller.history_length(0) == 2, "averaging path increments history length");

    controller.reset_all();
    begin_converged_frame(controller, pose);
    rt::RadianceFrame valid_history = make_frame(1, 1, 0.50f);
    valid_history.beauty_rgba[1] = 0.25f;
    valid_history.beauty_rgba[2] = 0.75f;
    valid_history.beauty_rgba[3] = 1.0f;
    const rt::RadianceFrame seeded_history = controller.resolve_frame(0, valid_history);

    rt::RadianceFrame invalid_frame = make_frame(1, 1, 0.60f);
    invalid_frame.beauty_rgba[0] = -1.0f;
    invalid_frame.beauty_rgba[1] = std::numeric_limits<float>::quiet_NaN();
    invalid_frame.beauty_rgba[2] = std::numeric_limits<float>::infinity();
    invalid_frame.beauty_rgba[3] = std::numeric_limits<float>::quiet_NaN();
    const rt::RadianceFrame sanitized = controller.resolve_frame(0, invalid_frame);
    expect_true(sanitized.beauty_rgba[0] == seeded_history.beauty_rgba[0], "negative beauty falls back to previous history");
    expect_true(sanitized.beauty_rgba[1] == seeded_history.beauty_rgba[1], "nan beauty falls back to previous history");
    expect_true(sanitized.beauty_rgba[2] == seeded_history.beauty_rgba[2], "inf beauty falls back to previous history");
    expect_true(sanitized.beauty_rgba[3] == seeded_history.beauty_rgba[3], "invalid alpha falls back to previous history");
    expect_true(controller.history_length(0) == 2, "sanitized converge frame still advances history");

    controller.reset_all();
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "reset_all returns preview mode");
    expect_true(controller.history_length(0) == 0, "reset_all clears camera 0 history");
    expect_true(controller.history_length(1) == 0, "reset_all clears camera 1 history");

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "first frame after reset starts preview");

    return 0;
}
