#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

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

    const rt::RadianceFrame converge_frame {
        .width = 2,
        .height = 1,
        .average_luminance = 0.5,
        .beauty_rgba = std::vector<float>(8, 1.0f),
    };
    const rt::RadianceFrame resolved_first = controller.resolve_frame(0, converge_frame);
    expect_true(resolved_first.width == converge_frame.width, "resolve returns raw frame width");
    expect_true(controller.history_length(0) == 1, "first converge resolve initializes history");

    rt::viewer::BodyPose rotated = pose;
    rotated.yaw_deg = 12.0;
    controller.begin_frame("scene_a", rotated);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "large rotation returns preview");
    expect_true(controller.history_length(0) == 0, "large rotation clears history");

    controller.begin_frame("scene_b", rotated);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "scene change stays preview");
    expect_true(controller.history_length(0) == 0, "scene change keeps history cleared");

    return 0;
}
