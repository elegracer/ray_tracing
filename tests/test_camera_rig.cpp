#include "realtime/camera_rig.h"
#include "scene/camera_spec.h"
#include "test_support.h"

#include <stdexcept>

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {320.0, 320.0, 320.0, 240.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 640, 480);
    rig.add_equi62(rt::make_equi62_lut1d_params(640, 480, 320.0, 320.0, 320.0, 240.0,
            std::array<double, 6> {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, Eigen::Vector2d::Zero()),
        Sophus::SE3d(Sophus::SO3d(), Eigen::Vector3d(0.1, 0.0, 0.0)), 640, 480);

    const rt::PackedCameraRig packed = rig.pack();
    expect_near(static_cast<double>(packed.active_count), 2.0, 1e-12, "active camera count");
    expect_true(packed.cameras[0].enabled == 1, "camera 0 enabled");
    expect_true(packed.cameras[1].enabled == 1, "camera 1 enabled");
    expect_true(packed.cameras[2].enabled == 0, "camera 2 disabled");
    expect_true(packed.cameras[3].enabled == 0, "camera 3 disabled");
    expect_true(packed.cameras[0].model == rt::CameraModelType::pinhole32, "camera 0 model");
    expect_true(packed.cameras[1].model == rt::CameraModelType::equi62_lut1d, "camera 1 model");
    expect_vec3_near(packed.cameras[1].T_rc.translation(), Eigen::Vector3d {0.0, 0.0, 0.1}, 1e-12,
        "camera 1 translation");
    expect_near(packed.cameras[1].equi.tangential.x(), 0.0, 1e-12, "camera 1 tangential x");
    expect_near(packed.cameras[1].equi.tangential.y(), 0.0, 1e-12, "camera 1 tangential y");

    rt::scene::CameraSpec authored_pinhole {};
    authored_pinhole.model = rt::CameraModelType::pinhole32;
    authored_pinhole.width = 800;
    authored_pinhole.height = 600;
    authored_pinhole.fx = 500.0;
    authored_pinhole.fy = 510.0;
    authored_pinhole.cx = 400.0;
    authored_pinhole.cy = 300.0;
    authored_pinhole.T_bc = Sophus::SE3d(Sophus::SO3d(), Eigen::Vector3d(0.0, 0.2, 0.0));

    rt::scene::CameraSpec authored_equi {};
    authored_equi.model = rt::CameraModelType::equi62_lut1d;
    authored_equi.width = 1280;
    authored_equi.height = 720;
    authored_equi.fx = 640.0;
    authored_equi.fy = 645.0;
    authored_equi.cx = 639.5;
    authored_equi.cy = 359.5;
    authored_equi.T_bc = Sophus::SE3d(Sophus::SO3d(), Eigen::Vector3d(0.3, 0.0, 0.1));

    rt::CameraRig authored_rig;
    authored_rig.add_camera(authored_pinhole);
    authored_rig.add_camera(authored_equi);

    const rt::PackedCameraRig authored_packed = authored_rig.pack();
    expect_true(authored_packed.cameras[0].model == rt::CameraModelType::pinhole32, "authored camera 0 model");
    expect_true(authored_packed.cameras[1].model == rt::CameraModelType::equi62_lut1d, "authored camera 1 model");
    expect_near(authored_packed.cameras[0].pinhole.fx, 500.0, 1e-12, "authored pinhole fx");
    expect_near(authored_packed.cameras[0].pinhole.k1, 0.0, 1e-12, "authored pinhole default k1");
    expect_near(authored_packed.cameras[1].equi.fx, 640.0, 1e-12, "authored equi fx");
    expect_near(authored_packed.cameras[1].equi.tangential.x(), 0.0, 1e-12, "authored equi default tangential x");
    expect_vec3_near(authored_packed.cameras[0].T_rc.translation(), Eigen::Vector3d {-0.2, 0.0, 0.0}, 1e-12,
        "authored camera 0 translation");
    expect_vec3_near(authored_packed.cameras[1].T_rc.translation(), Eigen::Vector3d {0.0, -0.1, 0.3}, 1e-12,
        "authored camera 1 translation");

    rt::CameraRig overflow;
    const rt::Pinhole32Params pinhole {160.0, 120.0, 80.0, 60.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    overflow.add_pinhole(pinhole, Sophus::SE3d(), 320, 240);
    overflow.add_pinhole(pinhole, Sophus::SE3d(), 320, 240);
    overflow.add_pinhole(pinhole, Sophus::SE3d(), 320, 240);
    overflow.add_pinhole(pinhole, Sophus::SE3d(), 320, 240);

    bool threw = false;
    try {
        overflow.add_pinhole(pinhole, Sophus::SE3d(), 320, 240);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect_true(threw, "fifth camera throws");

    return 0;
}
