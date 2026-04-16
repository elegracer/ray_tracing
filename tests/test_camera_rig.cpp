#include "realtime/camera_rig.h"
#include "test_support.h"

#include <stdexcept>

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {320.0, 320.0, 320.0, 240.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 640, 480);
    rig.add_equi62(rt::make_equi62_lut1d_params(640, 480, 320.0, 320.0, 320.0, 240.0,
            std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d::Zero()),
        Eigen::Translation3d(0.1, 0.0, 0.0) * Eigen::Isometry3d::Identity(), 640, 480);

    const rt::PackedCameraRig packed = rig.pack();
    expect_near(static_cast<double>(packed.active_count), 2.0, 1e-12, "active camera count");
    expect_true(packed.cameras[0].enabled == 1, "camera 0 enabled");
    expect_true(packed.cameras[1].enabled == 1, "camera 1 enabled");
    expect_true(packed.cameras[2].enabled == 0, "camera 2 disabled");
    expect_true(packed.cameras[3].enabled == 0, "camera 3 disabled");
    expect_true(packed.cameras[0].model == rt::CameraModelType::pinhole32, "camera 0 model");
    expect_true(packed.cameras[1].model == rt::CameraModelType::equi62_lut1d, "camera 1 model");
    expect_vec3_near(packed.cameras[1].T_rc.block<3, 1>(0, 3), Eigen::Vector3d {0.0, 0.1, 0.0}, 1e-12,
        "camera 1 translation");

    rt::CameraRig overflow;
    const rt::Pinhole32Params pinhole {160.0, 120.0, 80.0, 60.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    overflow.add_pinhole(pinhole, Eigen::Isometry3d::Identity(), 320, 240);
    overflow.add_pinhole(pinhole, Eigen::Isometry3d::Identity(), 320, 240);
    overflow.add_pinhole(pinhole, Eigen::Isometry3d::Identity(), 320, 240);
    overflow.add_pinhole(pinhole, Eigen::Isometry3d::Identity(), 320, 240);

    bool threw = false;
    try {
        overflow.add_pinhole(pinhole, Eigen::Isometry3d::Identity(), 320, 240);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect_true(threw, "fifth camera throws");

    return 0;
}
