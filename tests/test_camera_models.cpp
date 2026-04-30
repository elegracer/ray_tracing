#include "cam_base.h"
#include "cam_equi62_lut1d.h"
#include "cam_pinhole32.h"
#include "realtime/camera_models.h"
#include "realtime/camera_projection.h"
#include "realtime/camera_rig.h"
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

void expect_vec2_near(const Eigen::Vector2d& actual, const Eigen::Vector2d& expected,
    double tol, const std::string& label) {
    if ((actual - expected).cwiseAbs().maxCoeff() > tol) {
        throw std::runtime_error("expect_vec2_near failed: " + label);
    }
}

Eigen::VectorXd make_reference_pinhole_calib(const rt::Pinhole32Params& pinhole) {
    Eigen::VectorXd calib(9);
    calib << pinhole.fx, pinhole.fy, pinhole.cx, pinhole.cy, pinhole.k1, pinhole.k2, pinhole.k3, pinhole.p1, pinhole.p2;
    return calib;
}

Eigen::VectorXd make_reference_equi_calib(const rt::Equi62Lut1DParams& equi) {
    Eigen::VectorXd calib(12);
    calib << equi.fx, equi.fy, equi.cx, equi.cy,
        equi.radial[0], equi.radial[1], equi.radial[2], equi.radial[3], equi.radial[4], equi.radial[5],
        equi.tangential.x(), equi.tangential.y();
    return calib;
}

auto make_reference_pinhole_camera(const rt::Pinhole32Params& pinhole, int width, int height) {
    pico_cam::CamPinhole32 camera(width, height, cv::Mat(height, width, CV_8UC1, cv::Scalar(0)));
    camera.setParam(make_reference_pinhole_calib(pinhole));
    return camera;
}

auto make_reference_equi_camera(const rt::Equi62Lut1DParams& equi) {
    pico_cam::CamEqui62Lut1D camera(equi.width, equi.height, cv::Mat(equi.height, equi.width, CV_8UC1, cv::Scalar(0)));
    camera.setParam(make_reference_equi_calib(equi));
    return camera;
}

}  // namespace

namespace pico_cam {

double CamBase::MAX_HALF_FOV_RAD = std::numbers::pi_v<double>;
double CamBase::MAX_HALF_FOV_RAD_SQ = CamBase::MAX_HALF_FOV_RAD * CamBase::MAX_HALF_FOV_RAD;

}  // namespace pico_cam

int main() {
    using rt::DefaultCameraIntrinsics;
    using rt::Equi62Lut1DParams;
    using rt::Pinhole32Params;
    using rt::default_hfov_deg;
    using rt::derive_default_camera_intrinsics;
    using rt::make_equi62_lut1d_params;
    using rt::project_camera_pixel;
    using rt::project_equi62_lut1d;
    using rt::project_pinhole32;
    using rt::unproject_camera_pixel;
    using rt::unproject_equi62_lut1d;
    using rt::unproject_pinhole32;

    const Pinhole32Params pinhole{
        320.0, 330.0, 160.0, 120.0,
        0.07, -0.03, 0.01, 0.002, -0.0015,
    };
    const auto reference_pinhole = make_reference_pinhole_camera(pinhole, 320, 240);

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
    expect_vec2_near(project_pinhole32(pinhole, Eigen::Vector3d {0.0, 0.0, 1.0}),
        reference_pinhole.world2pixel(Eigen::Vector3d {0.0, 0.0, 1.0}), 1e-12, "pinhole reference center project");
    expect_vec2_near(project_pinhole32(pinhole, Eigen::Vector3d {0.23, -0.17, 1.0}),
        reference_pinhole.world2pixel(Eigen::Vector3d {0.23, -0.17, 1.0}), 1e-9, "pinhole reference off-axis project");
    expect_vec3_near(unproject_pinhole32(pinhole, Eigen::Vector2d {200.0, 80.0}),
        reference_pinhole.pixel2world(Eigen::Vector2d {200.0, 80.0}), 1e-9, "pinhole reference off-axis unproject");

    rt::PackedCamera dispatch_pinhole;
    dispatch_pinhole.model = rt::CameraModelType::pinhole32;
    dispatch_pinhole.pinhole = pinhole;
    expect_vec2_near(project_camera_pixel(dispatch_pinhole, Eigen::Vector3d {0.23, -0.17, 1.0}),
        project_pinhole32(pinhole, Eigen::Vector3d {0.23, -0.17, 1.0}), 1e-12,
        "camera projection dispatches pinhole project");
    expect_vec3_near(unproject_camera_pixel(dispatch_pinhole, Eigen::Vector2d {200.0, 80.0}),
        unproject_pinhole32(pinhole, Eigen::Vector2d {200.0, 80.0}), 1e-12,
        "camera projection dispatches pinhole unproject");

    const Pinhole32Params pinhole_hard{
        300.0, 315.0, 158.0, 121.0,
        0.8, -0.7, 0.25, 0.03, -0.025,
    };
    const auto reference_pinhole_hard = make_reference_pinhole_camera(pinhole_hard, 320, 240);
    expect_vec3_near(
        unproject_pinhole32(pinhole_hard, project_pinhole32(pinhole_hard, Eigen::Vector3d {1.28, -0.96, 1.0})),
        Eigen::Vector3d {1.28, -0.96, 1.0}.normalized(), 1e-12, "pinhole hard off-axis roundtrip");
    expect_vec2_near(project_pinhole32(pinhole_hard, Eigen::Vector3d {1.28, -0.96, 1.0}),
        reference_pinhole_hard.world2pixel(Eigen::Vector3d {1.28, -0.96, 1.0}), 1e-9,
        "pinhole reference distorted project");
    expect_vec3_near(unproject_pinhole32(pinhole_hard, Eigen::Vector2d {260.0, 60.0}),
        reference_pinhole_hard.pixel2world(Eigen::Vector2d {260.0, 60.0}), 1e-8,
        "pinhole reference distorted unproject");

    const Equi62Lut1DParams equi = make_equi62_lut1d_params(640, 480, 280.0, 282.0, 320.0, 240.0,
        std::array<double, 6> {0.02, -0.005, 0.001, -0.0002, 0.00005, -0.00001},
        Eigen::Vector2d {0.0012, -0.0009});
    const auto reference_equi = make_reference_equi_camera(equi);

    expect_vec3_near(unproject_equi62_lut1d(equi, Eigen::Vector2d {320.0, 240.0}),
        Eigen::Vector3d {0.0, 0.0, 1.0}, 1e-12, "equi center unproject");
    expect_vec3_near(unproject_equi62_lut1d(equi, project_equi62_lut1d(equi, Eigen::Vector3d {0.21, 0.08, 1.0})),
        Eigen::Vector3d {0.21, 0.08, 1.0}.normalized(), 2e-6, "equi off-axis roundtrip");
    expect_vec2_near(project_equi62_lut1d(equi, Eigen::Vector3d {0.0, 0.0, 1.0}),
        reference_equi.world2pixel(Eigen::Vector3d {0.0, 0.0, 1.0}), 1e-9, "equi reference center project");
    expect_vec2_near(project_equi62_lut1d(equi, Eigen::Vector3d {0.21, 0.08, 1.0}),
        reference_equi.world2pixel(Eigen::Vector3d {0.21, 0.08, 1.0}), 5e-6, "equi reference off-axis project");
    expect_vec3_near(unproject_equi62_lut1d(equi, Eigen::Vector2d {360.0, 180.0}),
        reference_equi.pixel2world(Eigen::Vector2d {360.0, 180.0}), 5e-6, "equi reference off-axis unproject");

    rt::PackedCamera dispatch_equi;
    dispatch_equi.model = rt::CameraModelType::equi62_lut1d;
    dispatch_equi.equi = equi;
    expect_vec2_near(project_camera_pixel(dispatch_equi, Eigen::Vector3d {0.21, 0.08, 1.0}),
        project_equi62_lut1d(equi, Eigen::Vector3d {0.21, 0.08, 1.0}), 1e-12,
        "camera projection dispatches equi project");
    expect_vec3_near(unproject_camera_pixel(dispatch_equi, Eigen::Vector2d {360.0, 180.0}),
        unproject_equi62_lut1d(equi, Eigen::Vector2d {360.0, 180.0}), 1e-12,
        "camera projection dispatches equi unproject");

    const Equi62Lut1DParams equi_identity = make_equi62_lut1d_params(640, 480, 280.0, 280.0, 320.0, 240.0,
        std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d {0.0, 0.0});
    const auto reference_equi_identity = make_reference_equi_camera(equi_identity);
    const double theta = 0.3;
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, Eigen::Vector2d {320.0 + 280.0 * theta, 240.0}),
        Eigen::Vector3d {std::sin(theta), 0.0, std::cos(theta)}, 1e-12, "equi analytic off-axis unproject");
    expect_vec2_near(project_equi62_lut1d(equi_identity, Eigen::Vector3d {std::sin(theta), 0.0, std::cos(theta)}),
        reference_equi_identity.world2pixel(Eigen::Vector3d {std::sin(theta), 0.0, std::cos(theta)}),
        1e-9, "equi identity reference project");
    expect_vec3_near(unproject_equi62_lut1d(equi_identity, Eigen::Vector2d {320.0 + 280.0 * theta, 240.0}),
        reference_equi_identity.pixel2world(Eigen::Vector2d {320.0 + 280.0 * theta, 240.0}),
        1e-9, "equi identity reference unproject");

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

    const DefaultCameraIntrinsics helper_equi_default = derive_default_camera_intrinsics(
        rt::CameraModelType::equi62_lut1d, 640, 480, default_hfov_deg(rt::CameraModelType::equi62_lut1d));
    const Equi62Lut1DParams helper_equi = make_equi62_lut1d_params(640, 480,
        helper_equi_default.fx, helper_equi_default.fy, helper_equi_default.cx, helper_equi_default.cy,
        std::array<double, 6> {}, Eigen::Vector2d::Zero());
    const auto reference_helper_equi = make_reference_equi_camera(helper_equi);
    expect_vec2_near(project_equi62_lut1d(helper_equi, Eigen::Vector3d {0.35, 0.0, 1.0}.normalized()),
        reference_helper_equi.world2pixel(Eigen::Vector3d {0.35, 0.0, 1.0}.normalized()),
        1e-9, "helper-derived equi reference project");
    expect_vec3_near(unproject_equi62_lut1d(helper_equi, Eigen::Vector2d {400.0, 240.0}),
        reference_helper_equi.pixel2world(Eigen::Vector2d {400.0, 240.0}),
        1e-9, "helper-derived equi reference unproject");

    return 0;
}
