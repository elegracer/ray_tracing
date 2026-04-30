#include "realtime/gpu/render_request_validation.h"
#include "test_support.h"

#include <stdexcept>
#include <string>

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
    }
    expect_true(threw, label + " threw");
    expect_true(matched, label + " message");
}

rt::PackedCamera make_camera() {
    rt::PackedCamera camera;
    camera.enabled = 1;
    camera.width = 64;
    camera.height = 48;
    return camera;
}

rt::PackedCameraRig make_rig(int active_count) {
    rt::PackedCameraRig rig;
    rig.active_count = active_count;
    for (int i = 0; i < active_count && i < 4; ++i) {
        rig.cameras[static_cast<std::size_t>(i)] = make_camera();
    }
    return rig;
}

}  // namespace

int main() {
    rt::PackedCameraRig rig = make_rig(2);

    rt::validate_render_camera_request(rig, 0, "render_radiance");
    rt::validate_render_camera_request(rig, 1, "render_radiance");
    rt::validate_render_pool_request(rig, 2, 4, "RendererPool");

    expect_throws_with_message(
        [&]() {
            rt::PackedCameraRig invalid = make_rig(0);
            rt::validate_render_camera_request(invalid, 0, "render_radiance");
        },
        "render_radiance requires rig.active_count in [1, 4], got 0",
        "reject zero active rig");

    expect_throws_with_message(
        [&]() { rt::validate_render_camera_request(rig, 2, "render_radiance"); },
        "render_radiance camera_index out of range: camera_index=2, active_count=2",
        "reject camera index outside active slots");

    expect_throws_with_message(
        [&]() {
            rt::PackedCameraRig disabled = rig;
            disabled.cameras[1].enabled = 0;
            rt::validate_render_camera_request(disabled, 1, "render_radiance");
        },
        "render_radiance camera slot is disabled at index 1",
        "reject disabled camera");

    expect_throws_with_message(
        [&]() {
            rt::PackedCameraRig invalid_resolution = rig;
            invalid_resolution.cameras[1].width = 0;
            rt::validate_render_camera_request(invalid_resolution, 1, "render_radiance");
        },
        "render_radiance camera slot has invalid resolution at index 1: width=0, height=48",
        "reject invalid camera resolution");

    expect_throws_with_message(
        [&]() { rt::validate_render_pool_request(rig, 0, 4, "RendererPool"); },
        "RendererPool active_cameras out of range",
        "reject zero active cameras");

    expect_throws_with_message(
        [&]() { rt::validate_render_pool_request(rig, 5, 4, "RendererPool"); },
        "RendererPool active_cameras out of range",
        "reject active cameras above renderer count");

    expect_throws_with_message(
        [&]() { rt::validate_render_pool_request(rig, 3, 4, "RendererPool"); },
        "RendererPool active_cameras exceeds rig.active_count",
        "reject pool active cameras above rig active count");

    expect_throws_with_message(
        [&]() {
            rt::PackedCameraRig disabled = make_rig(2);
            disabled.cameras[0].enabled = 0;
            rt::validate_render_pool_request(disabled, 2, 4, "RendererPool");
        },
        "RendererPool leading camera slot disabled at index 0",
        "reject disabled leading camera");

    expect_throws_with_message(
        [&]() {
            rt::PackedCameraRig invalid_resolution = make_rig(2);
            invalid_resolution.cameras[1].height = -1;
            rt::validate_render_pool_request(invalid_resolution, 2, 4, "RendererPool");
        },
        "RendererPool leading camera slot has invalid resolution at index 1",
        "reject invalid leading camera resolution");

    return 0;
}
