#include "realtime/camera_models.h"
#include "test_support.h"

#include <array>
#include <numbers>

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
    } catch (...) {
        threw = true;
    }
    expect_true(threw, label + " threw");
    expect_true(matched, label + " message");
}

}  // namespace

int main() {
    using rt::DefaultCameraIntrinsics;
    using rt::Equi62Lut1DParams;
    using rt::Pinhole32Params;
    using rt::default_hfov_deg;
    using rt::derive_default_camera_intrinsics;
    using rt::make_equi62_lut1d_params;
    using rt::project_equi62_lut1d;
    using rt::project_pinhole32;
    using rt::unproject_equi62_lut1d;
    using rt::unproject_pinhole32;

    const Pinhole32Params pinhole{
        320.0, 330.0, 160.0, 120.0,
        0.07, -0.03, 0.01, 0.002, -0.0015,
    };

    expect_vec3_near(unproject_pinhole32(pinhole, Eigen::Vector2d {160.0, 120.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "pinhole center unproject");
    expect_near(default_hfov_deg(rt::CameraModelType::pinhole32), 90.0, 1e-12, "default pinhole hfov");
    expect_near(default_hfov_deg(rt::CameraModelType::equi62_lut1d), 120.0, 1e-12, "default equi hfov");

    const DefaultCameraIntrinsics pinhole_default =
        derive_default_camera_intrinsics(rt::CameraModelType::pinhole32, 640, 480, default_hfov_deg(rt::CameraModelType::pinhole32));
    expect_near(pinhole_default.fx, 320.0, 1e-12, "default pinhole fx");
    expect_near(pinhole_default.fy, 320.0, 1e-12, "default pinhole fy");
    expect_near(pinhole_default.cx, 320.0, 1e-12, "default pinhole cx");
    expect_near(pinhole_default.cy, 240.0, 1e-12, "default pinhole cy");

    const DefaultCameraIntrinsics equi_default =
        derive_default_camera_intrinsics(rt::CameraModelType::equi62_lut1d, 640, 480, default_hfov_deg(rt::CameraModelType::equi62_lut1d));
    const double expected_equi_focal = 320.0 / (120.0 * std::numbers::pi / 360.0);
    expect_near(equi_default.fx, expected_equi_focal, 1e-12, "default equi fx");
    expect_near(equi_default.fy, expected_equi_focal, 1e-12, "default equi fy");
    expect_near(equi_default.cx, 320.0, 1e-12, "default equi cx");
    expect_near(equi_default.cy, 240.0, 1e-12, "default equi cy");

    expect_throws_with_message(
        [&]() { (void)derive_default_camera_intrinsics(rt::CameraModelType::pinhole32, 0, 480, 90.0); },
        "camera dimensions must be positive",
        "default intrinsics reject non-positive width");
    expect_throws_with_message(
        [&]() { (void)derive_default_camera_intrinsics(rt::CameraModelType::equi62_lut1d, 640, 480, 0.0); },
        "horizontal field of view must be positive",
        "default intrinsics reject non-positive hfov");
    expect_throws_with_message(
        [&]() { (void)derive_default_camera_intrinsics(rt::CameraModelType::pinhole32, 640, 480, 180.0); },
        "pinhole horizontal field of view must be less than 180 degrees",
        "default pinhole intrinsics reject 180 degrees");

    expect_vec3_near(unproject_pinhole32(pinhole, project_pinhole32(pinhole, Eigen::Vector3d {0.23, -0.17, 1.0})),
        Eigen::Vector3d {0.23, -0.17, 1.0}.normalized(), 1e-9, "pinhole off-axis roundtrip");
    expect_true((project_pinhole32(pinhole, unproject_pinhole32(pinhole, Eigen::Vector2d {160.0, 120.0}))
                    - Eigen::Vector2d {160.0, 120.0})
                    .cwiseAbs()
                    .maxCoeff()
                < 1e-12,
        "pinhole center project roundtrip");

    const Pinhole32Params pinhole_hard{
        300.0, 315.0, 158.0, 121.0,
        0.8, -0.7, 0.25, 0.03, -0.025,
    };
    expect_vec3_near(
        unproject_pinhole32(pinhole_hard, project_pinhole32(pinhole_hard, Eigen::Vector3d {1.28, -0.96, 1.0})),
        Eigen::Vector3d {1.28, -0.96, 1.0}.normalized(), 1e-12, "pinhole hard off-axis roundtrip");

    const Equi62Lut1DParams equi = make_equi62_lut1d_params(640, 480, 280.0, 282.0, 320.0, 240.0,
        std::array<double, 6> {0.02, -0.005, 0.001, -0.0002, 0.00005, -0.00001},
        Eigen::Vector2d {0.0012, -0.0009});

    expect_vec3_near(unproject_equi62_lut1d(equi, Eigen::Vector2d {320.0, 240.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "equi center unproject");
    expect_vec3_near(unproject_equi62_lut1d(equi, project_equi62_lut1d(equi, Eigen::Vector3d {0.21, 0.08, 1.0})),
        Eigen::Vector3d {0.21, 0.08, 1.0}.normalized(), 2e-6, "equi off-axis roundtrip");

    const Equi62Lut1DParams equi_identity = make_equi62_lut1d_params(640, 480, 280.0, 280.0, 320.0, 240.0,
        std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d {0.0, 0.0});
    const double theta = 0.3;
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, Eigen::Vector2d {320.0 + 280.0 * theta, 240.0}),
        Eigen::Vector3d {std::sin(theta), 0.0, std::cos(theta)}, 1e-12, "equi analytic off-axis unproject");

    Equi62Lut1DParams equi_lut_ceiling = equi_identity;
    equi_lut_ceiling.width = 1;
    equi_lut_ceiling.height = 1;
    equi_lut_ceiling.fx = 1.0;
    equi_lut_ceiling.fy = 1.0;
    equi_lut_ceiling.cx = 0.0;
    equi_lut_ceiling.cy = 0.0;
    equi_lut_ceiling.lut_step = 0.25;
    equi_lut_ceiling.lut.fill(0.0);
    equi_lut_ceiling.lut.back() = 0.3;
    const double lut_ceiling_radius = equi_lut_ceiling.lut_step * static_cast<double>(equi_lut_ceiling.lut.size() - 1);
    expect_vec3_near(unproject_equi62_lut1d(equi_lut_ceiling, Eigen::Vector2d {lut_ceiling_radius, 0.0}),
        Eigen::Vector3d {std::sin(0.3), 0.0, std::cos(0.3)}, 1e-12, "equi lut ceiling sample");

    const Eigen::Vector2d lut_out_of_range_pixel {320.0 + 280.0 * 5.0, 240.0 - 280.0 * 1.5};
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, lut_out_of_range_pixel),
        Eigen::Vector3d {5.0, -1.5, 1.0}.normalized(), 1e-12, "equi lut out-of-range fallback");

    return 0;
}
