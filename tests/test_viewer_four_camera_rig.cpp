#include "realtime/frame_convention.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

#include <Eigen/Geometry>

#include <array>

namespace {

double to_radians(double degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

Eigen::Matrix3d body_yaw_rotation(double yaw_deg) {
    return Eigen::AngleAxisd(to_radians(yaw_deg), Eigen::Vector3d::UnitX()).toRotationMatrix();
}

Eigen::Matrix3d yaw_offset_rotation(double yaw_deg) {
    return Eigen::AngleAxisd(to_radians(yaw_deg), Eigen::Vector3d::UnitX()).toRotationMatrix();
}

Eigen::Matrix3d camera_pitch_rotation(double pitch_deg) {
    return Eigen::AngleAxisd(to_radians(-pitch_deg), Eigen::Vector3d::UnitY()).toRotationMatrix();
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
    expect_true(profile.max_bounces == 3, "viewer bounces");
    expect_true(!profile.enable_denoise, "viewer denoise disabled");

    const rt::viewer::BodyPose pose {
        .position = Eigen::Vector3d(1.0, 2.0, 3.0),
        .yaw_deg = 15.0,
        .pitch_deg = 20.0,
    };
    const rt::PackedCameraRig packed_rig = rt::viewer::make_default_viewer_rig(pose, 640, 480).pack();
    expect_true(packed_rig.active_count == 4, "rig active cameras");

    const Eigen::Vector3d expected_translation = pose.position;
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
        const Eigen::Vector3d expected_forward = rt::body_to_world(
            body_yaw_rotation(pose.yaw_deg)
            * yaw_offset_rotation(expected_yaw_offsets_deg[static_cast<std::size_t>(i)])
            * camera_pitch_rotation(pose.pitch_deg)
            * Eigen::Vector3d(0.0, 0.0, -1.0));
        expect_vec3_near(camera_forward_renderer, expected_forward, 1e-9, "camera forward");
    }

    {
        rt::viewer::BodyPose moved = pose;
        rt::viewer::integrate_wasd(moved, true, false, false, false, false, false, 1.0);
        const rt::PackedCameraRig moved_rig = rt::viewer::make_default_viewer_rig(moved, 640, 480).pack();
        const Eigen::Vector3d translation_delta =
            moved_rig.cameras[0].T_rc.block<3, 1>(0, 3) - packed_rig.cameras[0].T_rc.block<3, 1>(0, 3);
        const Eigen::Vector3d front_forward_renderer =
            packed_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(0.0, 0.0, 1.0);
        expect_near((translation_delta.normalized() - front_forward_renderer.normalized()).norm(), 0.0, 1e-9,
            "W follows front camera forward");
    }

    {
        rt::viewer::BodyPose moved = pose;
        rt::viewer::integrate_wasd(moved, false, false, false, true, false, false, 1.0);
        const rt::PackedCameraRig moved_rig = rt::viewer::make_default_viewer_rig(moved, 640, 480).pack();
        const Eigen::Vector3d translation_delta =
            moved_rig.cameras[0].T_rc.block<3, 1>(0, 3) - packed_rig.cameras[0].T_rc.block<3, 1>(0, 3);
        const Eigen::Vector3d front_right_renderer =
            packed_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(1.0, 0.0, 0.0);
        expect_near((translation_delta.normalized() - front_right_renderer.normalized()).norm(), 0.0, 1e-9,
            "D follows front camera right");
    }

    {
        const rt::viewer::BodyPose neutral_side_pose {
            .position = Eigen::Vector3d::Zero(),
            .yaw_deg = 0.0,
            .pitch_deg = 0.0,
        };
        const rt::viewer::BodyPose pitched_side_pose {
            .position = Eigen::Vector3d::Zero(),
            .yaw_deg = 0.0,
            .pitch_deg = 20.0,
        };
        const rt::PackedCameraRig neutral_rig = rt::viewer::make_default_viewer_rig(neutral_side_pose, 640, 480).pack();
        const rt::PackedCameraRig pitched_rig = rt::viewer::make_default_viewer_rig(pitched_side_pose, 640, 480).pack();
        const Eigen::Matrix3d R_neutral = neutral_rig.cameras[1].T_rc.block<3, 3>(0, 0);
        const Eigen::Matrix3d R_pitched = pitched_rig.cameras[1].T_rc.block<3, 3>(0, 0);
        const Eigen::Matrix3d relative = R_neutral.transpose() * R_pitched;
        expect_vec3_near(relative * Eigen::Vector3d(1.0, 0.0, 0.0), Eigen::Vector3d(1.0, 0.0, 0.0), 1e-9,
            "side camera pitch keeps local right axis");
    }

    {
        rt::PackedScene converted_scene = rt::viewer::make_final_room_scene().pack();
        const rt::PackedScene shared_scene = rt::make_realtime_scene("final_room").pack();
        expect_true(converted_scene.material_count == shared_scene.material_count,
            "viewer final_room material count matches shared factory");
        expect_true(converted_scene.sphere_count == shared_scene.sphere_count,
            "viewer final_room sphere count matches shared factory");
        expect_true(converted_scene.quad_count == shared_scene.quad_count,
            "viewer final_room quad count matches shared factory");
        expect_true(!converted_scene.quads.empty(), "final_room exposes quads");
        const rt::QuadPrimitive& floor = converted_scene.quads.front();
        expect_near(floor.origin.z(), -1.0, 1e-12, "floor z origin");
        expect_near(floor.edge_u.z(), 0.0, 1e-12, "floor edge_u horizontal");
        expect_near(floor.edge_v.z(), 0.0, 1e-12, "floor edge_v horizontal");
    }
    return 0;
}
