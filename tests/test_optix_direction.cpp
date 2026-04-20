#include "realtime/camera_models.h"
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace {

Eigen::Vector3d direction_for_pixel(const rt::PackedCamera& camera, int x, int y) {
    const Eigen::Vector2d pixel {static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5};
    const Eigen::Vector3d dir_camera = camera.model == rt::CameraModelType::equi62_lut1d
        ? rt::unproject_equi62_lut1d(camera.equi, pixel)
        : rt::unproject_pinhole32(camera.pinhole, pixel);
    return (camera.T_rc.rotationMatrix() * dir_camera).normalized();
}

std::array<std::uint8_t, 3> encode_direction(const Eigen::Vector3d& direction) {
    std::array<std::uint8_t, 3> out {};
    for (int axis = 0; axis < 3; ++axis) {
        const double normalized = std::clamp(0.5 * (direction[axis] + 1.0), 0.0, 1.0);
        out[static_cast<std::size_t>(axis)] = static_cast<std::uint8_t>(normalized * 255.0);
    }
    return out;
}

std::array<std::uint8_t, 3> pixel_rgb(const rt::DirectionDebugFrame& frame, int x, int y) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)) * 4U;
    return {
        frame.rgba[index + 0],
        frame.rgba[index + 1],
        frame.rgba[index + 2],
    };
}

void expect_rgb_near(const std::array<std::uint8_t, 3>& actual, const std::array<std::uint8_t, 3>& expected,
    int tolerance, const std::string& label) {
    for (std::size_t axis = 0; axis < actual.size(); ++axis) {
        const int delta = std::abs(static_cast<int>(actual[axis]) - static_cast<int>(expected[axis]));
        expect_true(delta <= tolerance, label + " channel[" + std::to_string(axis) + "]");
    }
}

void expect_rgb_differs(const std::array<std::uint8_t, 3>& actual, const std::array<std::uint8_t, 3>& expected,
    int min_delta, const std::string& label) {
    bool differs = false;
    for (std::size_t axis = 0; axis < actual.size(); ++axis) {
        if (std::abs(static_cast<int>(actual[axis]) - static_cast<int>(expected[axis])) >= min_delta) {
            differs = true;
            break;
        }
    }
    expect_true(differs, label);
}

}  // namespace

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {320.0, 320.0, 160.0, 120.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 320, 240);
    rig.add_equi62(rt::make_equi62_lut1d_params(320, 240, 180.0, 182.0, 160.0, 120.0,
            std::array<double, 6> {}, Eigen::Vector2d::Zero()),
        Sophus::SE3d(), 320, 240);

    const rt::PackedCameraRig packed = rig.pack();
    rt::OptixRenderer renderer;

    const rt::DirectionDebugFrame pinhole_frame = renderer.render_direction_debug(packed, 0);
    expect_near(static_cast<double>(pinhole_frame.width), 320.0, 1e-12, "pinhole debug width");
    expect_near(static_cast<double>(pinhole_frame.height), 240.0, 1e-12, "pinhole debug height");
    expect_true(!pinhole_frame.rgba.empty(), "pinhole debug image has pixels");
    expect_rgb_near(pixel_rgb(pinhole_frame, 0, 0), encode_direction(direction_for_pixel(packed.cameras[0], 0, 0)), 2,
        "pinhole top-left direction");

    const rt::DirectionDebugFrame equi_frame = renderer.render_direction_debug(packed, 1);
    expect_near(static_cast<double>(equi_frame.width), 320.0, 1e-12, "equi debug width");
    expect_near(static_cast<double>(equi_frame.height), 240.0, 1e-12, "equi debug height");
    expect_true(!equi_frame.rgba.empty(), "equi debug image has pixels");
    expect_rgb_near(
        pixel_rgb(equi_frame, 0, 0), encode_direction(direction_for_pixel(packed.cameras[1], 0, 0)), 2,
        "equi top-left direction");

    expect_rgb_differs(pixel_rgb(equi_frame, 0, 0), pixel_rgb(pinhole_frame, 0, 0), 12,
        "equi debug should differ from pinhole at the same pixel");

    return 0;
}
