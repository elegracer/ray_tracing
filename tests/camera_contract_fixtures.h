#pragma once

#include "realtime/camera_models.h"
#include "realtime/camera_projection.h"
#include "realtime/camera_rig.h"

#include <Eigen/Geometry>

namespace rt::test {

inline PackedCamera make_contract_test_pinhole_camera() {
    PackedCamera camera;
    camera.width = 64;
    camera.height = 48;
    camera.model = CameraModelType::pinhole32;
    camera.T_rc = Sophus::SE3d(Eigen::AngleAxisd(0.35, Eigen::Vector3d::UnitY()).toRotationMatrix(),
        Eigen::Vector3d(1.0, -2.0, 3.5));
    camera.pinhole = Pinhole32Params {
        52.0, 50.0, 31.5, 23.5,
        0.02, -0.01, 0.003, 0.001, -0.0015,
    };
    return camera;
}

inline PackedCamera make_contract_test_equi_camera() {
    PackedCamera camera;
    camera.width = 64;
    camera.height = 48;
    camera.model = CameraModelType::equi62_lut1d;
    camera.T_rc = Sophus::SE3d(Eigen::AngleAxisd(-0.2, Eigen::Vector3d::UnitX()).toRotationMatrix(),
        Eigen::Vector3d(-1.5, 0.75, 2.25));
    camera.equi = make_equi62_lut1d_params(camera.width, camera.height, 28.0, 29.0, 31.5, 23.5,
        std::array<double, 6> {0.01, -0.003, 0.0008, -0.0002, 0.00005, -0.00001},
        Eigen::Vector2d {0.0007, -0.0005});
    return camera;
}

inline PackedCameraRig make_contract_test_rig() {
    PackedCameraRig rig;
    rig.active_count = 2;
    rig.cameras[0] = make_contract_test_pinhole_camera();
    rig.cameras[1] = make_contract_test_equi_camera();
    rig.cameras[0].enabled = 1;
    rig.cameras[1].enabled = 1;
    return rig;
}

inline Eigen::Vector2d contract_pinhole_sample_pixel() {
    return Eigen::Vector2d {17.5, 11.5};
}

inline Eigen::Vector2d contract_equi_sample_pixel() {
    return Eigen::Vector2d {45.5, 14.5};
}

inline Eigen::Vector3d world_direction_for_pixel(const PackedCamera& camera, const Eigen::Vector2d& pixel) {
    return rt::world_direction_for_pixel(camera, pixel);
}

}  // namespace rt::test
