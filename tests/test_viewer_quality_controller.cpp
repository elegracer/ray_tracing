#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

double compute_average_luminance(const std::vector<float>& rgba) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

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
    const rt::RadianceFrame preview_frame = make_frame(2, 1, 0.5f);
    const auto preview_view = controller.resolve_beauty_view(0, preview_frame);
    expect_true(preview_view.width == preview_frame.width, "preview beauty view keeps raw width");
    expect_true(preview_view.height == preview_frame.height, "preview beauty view keeps raw height");
    expect_true(preview_view.average_luminance == preview_frame.average_luminance,
        "preview beauty view keeps raw luminance");
    expect_true(preview_view.beauty_rgba.data() == preview_frame.beauty_rgba.data(),
        "preview beauty view aliases raw beauty without copying");
    expect_true(controller.history_length(0) == 0, "preview beauty view does not create history");

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge, "stable second frame converges");
    expect_true(controller.active_profile().samples_per_pixel == converge_profile.samples_per_pixel, "converge profile active");

    const rt::RadianceFrame converge_frame = make_frame(2, 1, 1.0f);
    const auto converge_view = controller.resolve_beauty_view(0, converge_frame);
    expect_true(converge_view.beauty_rgba.data() != converge_frame.beauty_rgba.data(),
        "converge beauty view aliases accumulated history instead of raw beauty");
    expect_true(controller.history_length(0) == 1, "converge beauty view initializes history");
    const rt::RadianceFrame resolved_first = controller.resolve_frame(0, converge_frame);
    expect_true(resolved_first.width == converge_frame.width, "resolve returns raw frame width");
    expect_true(controller.history_length(0) == 2, "resolve_frame continues converge history after beauty view");
    const auto second_converge_view = controller.resolve_beauty_view(0, converge_frame);
    expect_true(second_converge_view.beauty_rgba.data() == converge_view.beauty_rgba.data(),
        "converge beauty view reuses the same accumulated history buffer");
    expect_true(controller.history_length(0) == 3, "repeated converge beauty views grow history to three frames");
    const rt::RadianceFrame resolved_second = controller.resolve_frame(0, converge_frame);
    expect_true(controller.history_length(0) == 4, "repeated resolve_frame calls still grow converge history");
    expect_true(resolved_second.beauty_rgba[0] == 1.0f, "identical converge frames keep accumulated beauty");
    controller.resolve_frame(0, converge_frame);
    expect_true(controller.history_length(0) == 5, "later converge resolves continue growing history");

    controller.resolve_frame(1, converge_frame);
    expect_true(controller.history_length(1) == 1, "other camera maintains independent history");
    expect_true(controller.history_length(0) == 5, "primary camera history remains unchanged");

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
    expect_near(averaged_second.average_luminance, compute_average_luminance(averaged_second.beauty_rgba), 1e-9,
        "resolve keeps average_luminance in sync with averaged beauty");
    expect_true(controller.history_length(0) == 2, "averaging path increments history length");

    controller.reset_all();
    begin_converged_frame(controller, pose);
    rt::RadianceFrame stale_first = make_frame(1, 1, 0.20f);
    stale_first.beauty_rgba[3] = 1.0f;
    rt::RadianceFrame stale_second = make_frame(1, 1, 0.60f);
    stale_second.beauty_rgba[3] = 1.0f;
    controller.resolve_frame(0, stale_first);
    controller.resolve_frame(0, stale_second);
    expect_true(controller.history_length(0) == 2, "accumulation starts before malformed frame regression");

    rt::RadianceFrame undersized_frame = make_frame(1, 1, 0.80f);
    undersized_frame.beauty_rgba.resize(3);
    controller.resolve_frame(0, undersized_frame);
    expect_true(controller.history_length(0) == 0, "undersized converge frame clears stale camera history");

    rt::RadianceFrame restarted_frame = make_frame(1, 1, 0.90f);
    restarted_frame.beauty_rgba[3] = 1.0f;
    const rt::RadianceFrame restarted_resolve = controller.resolve_frame(0, restarted_frame);
    expect_true(controller.history_length(0) == 1, "next valid converge frame restarts history from one sample");
    expect_near(restarted_resolve.beauty_rgba[0], 0.90f, 1e-6, "next valid converge frame does not blend with stale history");

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
    begin_converged_frame(controller, pose);
    rt::RadianceFrame ceiling_seed = make_frame(1, 1, 0.25f);
    ceiling_seed.beauty_rgba[0] = 0.25f;
    ceiling_seed.beauty_rgba[1] = 0.50f;
    ceiling_seed.beauty_rgba[2] = 1.00f;
    ceiling_seed.beauty_rgba[3] = 1.0f;
    const rt::RadianceFrame seeded_ceiling = controller.resolve_frame(0, ceiling_seed);

    rt::RadianceFrame above_ceiling = make_frame(1, 1, 0.75f);
    above_ceiling.beauty_rgba[0] = 65.0f;
    above_ceiling.beauty_rgba[1] = 128.0f;
    above_ceiling.beauty_rgba[2] = 64.0f;
    above_ceiling.beauty_rgba[3] = 1.0f;
    const rt::RadianceFrame rejected_above_ceiling = controller.resolve_frame(0, above_ceiling);
    expect_true(rejected_above_ceiling.beauty_rgba[0] == seeded_ceiling.beauty_rgba[0],
        "finite beauty above the documented ceiling falls back to previous history");
    expect_true(rejected_above_ceiling.beauty_rgba[1] == seeded_ceiling.beauty_rgba[1],
        "all above-ceiling beauty channels are rejected");
    expect_true(rejected_above_ceiling.beauty_rgba[2] != seeded_ceiling.beauty_rgba[2],
        "beauty values at the documented ceiling remain valid");

    controller.reset_all();
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "reset_all returns preview mode");
    expect_true(controller.history_length(0) == 0, "reset_all clears camera 0 history");
    expect_true(controller.history_length(1) == 0, "reset_all clears camera 1 history");

    controller.begin_frame("scene_a", pose);
    expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview, "first frame after reset starts preview");

    return 0;
}
