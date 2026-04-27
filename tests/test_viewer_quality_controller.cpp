#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>

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

    // --- Initial state ---
    {
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "initial mode is preview");
        expect_true(controller.active_profile().samples_per_pixel == preview_profile.samples_per_pixel,
            "initial profile is preview profile");
        expect_true(controller.history_length(0) == 0,
            "initial history_length is 0 for camera 0");
        expect_true(controller.history_length(1) == 0,
            "initial history_length is 0 for camera 1");
        expect_true(controller.history_length(99) == 0,
            "initial history_length is 0 for any camera index");
    }

    // --- begin_frame transitions to converge after two frames with the same scene_id ---
    {
        controller.begin_frame("scene_a", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "first frame with new scene starts in preview");
        expect_true(controller.active_profile().samples_per_pixel == preview_profile.samples_per_pixel,
            "preview profile is active in preview mode");

        controller.begin_frame("scene_a", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "second frame with same scene transitions to converge");
        expect_true(controller.active_profile().samples_per_pixel == converge_profile.samples_per_pixel,
            "converge profile is active in converge mode");
    }

    // --- resolve_beauty_view pass-through in converge mode ---
    {
        const rt::RadianceFrame raw_frame = make_frame(3, 2, 0.5f);
        const auto resolved = controller.resolve_beauty_view(0, raw_frame);

        expect_true(resolved.width == raw_frame.width,
            "resolve_beauty_view preserves width in converge mode");
        expect_true(resolved.height == raw_frame.height,
            "resolve_beauty_view preserves height in converge mode");
        expect_true(resolved.beauty_rgba.data() == raw_frame.beauty_rgba.data(),
            "resolve_beauty_view aliases raw frame data (no copy into history buffer)");
        expect_near(resolved.average_luminance, compute_average_luminance(raw_frame.beauty_rgba), 1e-9,
            "resolve_beauty_view computes average_luminance from raw pixel data");

        // history_length is always 0 since blending is GPU-managed
        expect_true(controller.history_length(0) == 0,
            "history_length is 0 after resolve in converge mode");
    }

    // --- Scene change resets to preview, resolve still works as pass-through ---
    {
        controller.begin_frame("scene_b", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "scene change resets mode to preview");

        const rt::RadianceFrame raw_frame = make_frame(2, 2, 0.3f);
        const auto resolved = controller.resolve_beauty_view(0, raw_frame);

        expect_true(resolved.width == raw_frame.width,
            "resolve preserves width after scene change");
        expect_true(resolved.height == raw_frame.height,
            "resolve preserves height after scene change");
        expect_true(resolved.beauty_rgba.data() == raw_frame.beauty_rgba.data(),
            "resolve aliases raw data after scene change");
        expect_near(resolved.average_luminance, compute_average_luminance(raw_frame.beauty_rgba), 1e-9,
            "resolve computes average_luminance after scene change");
    }

    // --- Re-converge after scene change ---
    {
        controller.begin_frame("scene_b", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "second frame after scene change re-converges");
    }

    // --- reset_all returns to preview and clears state ---
    {
        controller.reset_all();
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "reset_all returns to preview mode");
        expect_true(controller.history_length(0) == 0,
            "history_length is 0 after reset");
        expect_true(controller.active_profile().samples_per_pixel == preview_profile.samples_per_pixel,
            "preview profile is active after reset");
    }

    // --- First frame after reset starts in preview ---
    {
        controller.begin_frame("scene_a", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "first frame after reset starts in preview");
    }

    // --- materialize_frame copies beauty data from resolved view ---
    {
        const rt::RadianceFrame src_frame = make_frame(2, 2, 0.75f);
        const auto resolved = controller.resolve_beauty_view(0, src_frame);
        const rt::RadianceFrame materialized =
            rt::viewer::ViewerQualityController::materialize_frame(resolved, src_frame);

        expect_true(materialized.width == src_frame.width,
            "materialize_frame preserves source width");
        expect_true(materialized.height == src_frame.height,
            "materialize_frame preserves source height");
        expect_near(materialized.average_luminance, resolved.average_luminance, 1e-9,
            "materialize_frame uses resolved average_luminance");
        expect_true(materialized.beauty_rgba.data() != resolved.beauty_rgba.data(),
            "materialize_frame deep-copies beauty data (does not alias the span)");
        expect_near(static_cast<double>(materialized.beauty_rgba[0]), 0.75, 1e-6,
            "materialized beauty pixels match source values");
    }

    return 0;
}
