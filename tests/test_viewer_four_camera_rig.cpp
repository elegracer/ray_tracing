#include "realtime/frame_convention.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {

double yaw_from_forward(const Eigen::Vector3d& forward) {
    return std::atan2(forward.x(), -forward.z()) * 180.0 / std::numbers::pi;
}

double pitch_from_forward(const Eigen::Vector3d& forward) {
    return std::asin(std::clamp(forward.y(), -1.0, 1.0)) * 180.0 / std::numbers::pi;
}

double wrap_degrees(double deg) {
    while (deg > 180.0) {
        deg -= 360.0;
    }
    while (deg < -180.0) {
        deg += 360.0;
    }
    return deg;
}

}  // namespace

int main() {
    const rt::SceneDescription scene = rt::viewer::make_default_viewer_scene();
    const rt::PackedScene packed_scene = scene.pack();
    expect_true(packed_scene.material_count >= 5, "final_room materials");
    expect_true(packed_scene.sphere_count >= 6, "final_room spheres");
    expect_true(packed_scene.quad_count >= 7, "final_room quads");

    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();
    expect_true(profile.samples_per_pixel == 1, "viewer spp");
    expect_true(profile.max_bounces == 2, "viewer bounces");
    expect_true(!profile.enable_denoise, "viewer denoise disabled");

    const rt::viewer::BodyPose pose {
        .position = Eigen::Vector3d(1.0, 2.0, 3.0),
        .yaw_deg = 15.0,
        .pitch_deg = 20.0,
    };
    const rt::PackedCameraRig packed_rig = rt::viewer::make_default_viewer_rig(pose, 640, 480).pack();
    expect_true(packed_rig.active_count == 4, "rig active cameras");

    const Eigen::Vector3d expected_translation = rt::body_to_renderer(pose.position);
    constexpr std::array<double, 4> expected_yaw_offsets_deg {0.0, 90.0, -90.0, 180.0};
    for (int i = 0; i < 4; ++i) {
        const rt::PackedCamera& camera = packed_rig.cameras[static_cast<std::size_t>(i)];
        expect_true(camera.enabled == 1, "camera enabled");
        expect_true(camera.width == 640, "camera width");
        expect_true(camera.height == 480, "camera height");

        const Eigen::Vector3d camera_translation(
            camera.T_rc(0, 3),
            camera.T_rc(1, 3),
            camera.T_rc(2, 3));
        expect_vec3_near(camera_translation, expected_translation, 1e-12, "camera translation");

        const Eigen::Matrix3d R_rc = camera.T_rc.block<3, 3>(0, 0);
        const Eigen::Vector3d camera_forward_renderer = R_rc * Eigen::Vector3d(0.0, 0.0, 1.0);
        const double actual_yaw = yaw_from_forward(camera_forward_renderer);
        const double actual_pitch = pitch_from_forward(camera_forward_renderer);
        const double expected_yaw =
            wrap_degrees(pose.yaw_deg + expected_yaw_offsets_deg[static_cast<std::size_t>(i)]);
        expect_near(wrap_degrees(actual_yaw - expected_yaw), 0.0, 1e-9, "camera yaw");
        expect_near(actual_pitch, pose.pitch_deg, 1e-9, "camera pitch");
    }
    return 0;
}
