#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_render_session.h"
#include "test_support.h"

#include <Eigen/Core>

#include <string>
#include <vector>

namespace {

rt::RenderProfile make_profile(int samples_per_pixel) {
    rt::RenderProfile profile {};
    profile.samples_per_pixel = samples_per_pixel;
    return profile;
}

rt::RadianceFrame make_frame(float value) {
    return rt::RadianceFrame {
        .width = 2,
        .height = 1,
        .average_luminance = 0.0,
        .beauty_rgba = {
            value, value, value, 1.0f,
            value, value, value, 1.0f,
        },
    };
}

}  // namespace

int main() {
    const rt::RenderProfile preview_profile = make_profile(1);
    const rt::RenderProfile converge_profile = make_profile(4);
    rt::viewer::ViewerRenderSession session(preview_profile, converge_profile);
    const rt::viewer::BodyPose pose {.position = Eigen::Vector3d::Zero(), .yaw_deg = 0.0, .pitch_deg = 0.0};

    std::vector<std::string> events;
    int render_count = 0;
    const auto reset_accumulation = [&]() {
        events.push_back("reset");
    };
    const auto render = [&](const rt::RenderProfile& profile) {
        events.push_back("render:" + std::to_string(profile.samples_per_pixel));
        ++render_count;
        return std::vector<rt::viewer::ViewerRawFrame> {
            rt::viewer::ViewerRawFrame {.camera_index = 1, .frame = make_frame(0.25f)},
        };
    };

    const rt::viewer::ViewerRenderBatch first =
        session.render_frame("scene_a", pose, reset_accumulation, render);
    expect_true(first.mode == rt::viewer::ViewerQualityMode::preview, "first frame starts preview");
    expect_true(first.profile.samples_per_pixel == preview_profile.samples_per_pixel, "first frame uses preview profile");
    expect_true(events.size() == 2, "first frame event count");
    expect_true(events[0] == "reset", "first frame resets before render");
    expect_true(events[1] == "render:1", "first frame renders with preview profile");
    expect_true(first.frames.size() == 1, "first frame output count");
    expect_true(first.frames[0].camera_index == 1, "first frame preserves camera index");
    expect_true(first.frames[0].display_frame.beauty_rgba.data() == first.frames[0].frame.beauty_rgba.data(),
        "display frame aliases owned raw frame");
    expect_true(first.frames[0].display_frame.average_luminance > 0.0, "display frame computes luminance");

    const rt::viewer::ViewerRenderBatch second =
        session.render_frame("scene_a", pose, reset_accumulation, render);
    expect_true(second.mode == rt::viewer::ViewerQualityMode::converge, "second stable frame converges");
    expect_true(second.profile.samples_per_pixel == converge_profile.samples_per_pixel,
        "second stable frame uses converge profile");
    expect_true(events.size() == 3, "second frame only renders");
    expect_true(events[2] == "render:4", "second frame renders with converge profile");

    const rt::viewer::ViewerRenderBatch scene_change =
        session.render_frame("scene_b", pose, reset_accumulation, render);
    expect_true(scene_change.mode == rt::viewer::ViewerQualityMode::preview, "scene change returns to preview");
    expect_true(events.size() == 5, "scene change reset and render");
    expect_true(events[3] == "reset", "scene change resets before render");
    expect_true(events[4] == "render:1", "scene change renders with preview profile");
    expect_true(render_count == 3, "render called once per session frame");

    return 0;
}
