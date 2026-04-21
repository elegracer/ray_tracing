#include "camera_contract_fixtures.h"
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
    const rt::PackedCameraRig packed = rt::test::make_contract_test_rig();
    rt::OptixRenderer renderer;

    const rt::DirectionDebugFrame pinhole_frame = renderer.render_direction_debug(packed, 0);
    expect_near(static_cast<double>(pinhole_frame.width), 64.0, 1e-12, "pinhole debug width");
    expect_near(static_cast<double>(pinhole_frame.height), 48.0, 1e-12, "pinhole debug height");
    expect_true(!pinhole_frame.rgba.empty(), "pinhole debug image has pixels");
    expect_rgb_near(pixel_rgb(pinhole_frame,
                        static_cast<int>(rt::test::contract_pinhole_sample_pixel().x()),
                        static_cast<int>(rt::test::contract_pinhole_sample_pixel().y())),
        encode_direction(rt::test::world_direction_for_pixel(packed.cameras[0], rt::test::contract_pinhole_sample_pixel())),
        2, "pinhole contract sample direction");

    const rt::DirectionDebugFrame equi_frame = renderer.render_direction_debug(packed, 1);
    expect_near(static_cast<double>(equi_frame.width), 64.0, 1e-12, "equi debug width");
    expect_near(static_cast<double>(equi_frame.height), 48.0, 1e-12, "equi debug height");
    expect_true(!equi_frame.rgba.empty(), "equi debug image has pixels");
    expect_rgb_near(
        pixel_rgb(equi_frame,
            static_cast<int>(rt::test::contract_equi_sample_pixel().x()),
            static_cast<int>(rt::test::contract_equi_sample_pixel().y())),
        encode_direction(rt::test::world_direction_for_pixel(packed.cameras[1], rt::test::contract_equi_sample_pixel())),
        2, "equi contract sample direction");

    expect_rgb_differs(
        pixel_rgb(equi_frame,
            static_cast<int>(rt::test::contract_equi_sample_pixel().x()),
            static_cast<int>(rt::test::contract_equi_sample_pixel().y())),
        pixel_rgb(pinhole_frame,
            static_cast<int>(rt::test::contract_pinhole_sample_pixel().x()),
            static_cast<int>(rt::test::contract_pinhole_sample_pixel().y())),
        12, "equi contract sample should differ from pinhole contract sample");

    return 0;
}
