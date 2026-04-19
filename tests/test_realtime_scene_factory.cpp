#include "realtime/realtime_scene_factory.h"
#include "realtime/camera_models.h"
#include "realtime/viewer/four_camera_rig.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "test_support.h"

#include <numbers>

int main() {
    const rt::PackedScene smoke = rt::make_realtime_scene("smoke").pack();
    expect_true(smoke.sphere_count >= 1, "smoke scene has geometry");

    const rt::PackedScene final_room = rt::make_realtime_scene("final_room").pack();
    expect_true(final_room.quad_count >= 7, "final_room quads");
    expect_vec3_near(final_room.background, Eigen::Vector3d::Zero(), 1e-12,
        "final_room realtime scene keeps black background");

    const rt::PackedScene imported = rt::make_realtime_scene("imported_obj_smoke").pack();
    expect_true(imported.triangle_count >= 1, "file-backed imported scene keeps triangle geometry");
    expect_vec3_near(imported.background, Eigen::Vector3d::Zero(), 1e-12,
        "file-backed imported scene keeps yaml background");

    const rt::PackedScene bouncing = rt::make_realtime_scene("bouncing_spheres").pack();
    expect_vec3_near(bouncing.background, Eigen::Vector3d(0.70, 0.80, 1.00), 1e-12,
        "bouncing_spheres realtime scene keeps shared sky background");

    const rt::PackedScene cornell_box = rt::make_realtime_scene("cornell_box").pack();
    expect_true(cornell_box.quad_count >= 12, "cornell_box box lowers to quads");

    expect_true(rt::realtime_scene_supported("smoke"), "smoke supported");
    expect_true(rt::realtime_scene_supported("imported_obj_smoke"), "file-backed imported scene supported");
    expect_true(rt::realtime_scene_supported("cornell_box"), "cornell_box supported");
    expect_true(rt::realtime_scene_supported("rttnw_final_scene"), "shared final scene supported");
    expect_true(!rt::realtime_scene_supported("rttnw_final_scene_extreme"),
        "duplicate realtime scene id removed");

    const rt::viewer::BodyPose earth_spawn = rt::default_spawn_pose_for_scene("earth_sphere");
    expect_vec3_near(earth_spawn.position, Eigen::Vector3d {-3.0, 6.0, -10.0}, 1e-12,
        "earth spawn uses realtime preset pose");
    expect_true(rt::default_move_speed_for_scene("earth_sphere") > 0.0, "earth move speed is positive");

    const rt::viewer::BodyPose imported_spawn = rt::default_spawn_pose_for_scene("imported_obj_smoke");
    expect_vec3_near(imported_spawn.position, Eigen::Vector3d {0.0, 0.0, 2.0}, 1e-12,
        "file-backed imported spawn uses yaml preset pose");

    const rt::PackedCameraRig earth_rig = rt::default_camera_rig_for_scene("earth_sphere", 1, 640, 480).pack();
    expect_true(earth_rig.active_count == 1, "earth rig active camera count");
    expect_vec3_near(Eigen::Vector3d(
                         earth_rig.cameras[0].T_rc(0, 3),
                         earth_rig.cameras[0].T_rc(1, 3),
                         earth_rig.cameras[0].T_rc(2, 3)),
        Eigen::Vector3d {-3.0, 6.0, -10.0}, 1e-12,
        "earth rig uses scene camera preset");
    const double expected_earth_fy =
        0.5 * 480.0 / std::tan((20.0 * std::numbers::pi / 180.0) * 0.5);
    expect_near(earth_rig.cameras[0].pinhole.fy, expected_earth_fy, 1e-9,
        "earth rig preserves legacy scene vfov");

    const rt::PackedScene default_view = rt::viewer::make_default_viewer_scene().pack();
    expect_true(default_view.material_count == final_room.material_count, "default viewer materials match final_room");
    expect_true(default_view.sphere_count == final_room.sphere_count, "default viewer spheres match final_room");
    expect_true(default_view.quad_count == final_room.quad_count, "default viewer scene is final_room");

    {
        constexpr const char* kLegacyScenes[] {"earth_sphere", "quads", "simple_light", "rttnw_final_scene"};
        for (const char* scene_id : kLegacyScenes) {
            const rt::viewer::BodyPose spawn = rt::default_spawn_pose_for_scene(scene_id);
            const rt::viewer::ViewerFrameConvention convention = rt::viewer_frame_convention_for_scene(scene_id);
            const rt::PackedCameraRig viewer_rig =
                rt::viewer::make_default_viewer_rig(spawn, 640, 480, convention).pack();
            const rt::PackedCameraRig preset_rig = rt::default_camera_rig_for_scene(scene_id, 1, 640, 480).pack();

            const Eigen::Vector3d viewer_forward =
                viewer_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(0.0, 0.0, 1.0);
            const Eigen::Vector3d preset_forward =
                preset_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(0.0, 0.0, 1.0);
            expect_vec3_near(viewer_forward, preset_forward, 1e-9, "viewer spawn forward matches scene preset");

            const Eigen::Vector3d viewer_up =
                viewer_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(0.0, -1.0, 0.0);
            const Eigen::Vector3d preset_up =
                preset_rig.cameras[0].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d(0.0, -1.0, 0.0);
            expect_vec3_near(viewer_up, preset_up, 1e-9, "viewer spawn up matches scene preset");
        }
    }

    {
        const rt::PackedCameraRig quads_rig = rt::default_camera_rig_for_scene("quads", 1, 640, 480).pack();
        const rt::PackedCamera& camera = quads_rig.cameras[0];
        const Eigen::Matrix3d R = camera.T_rc.block<3, 3>(0, 0);
        const Eigen::Vector3d top_ray =
            (R * rt::unproject_pinhole32(camera.pinhole, Eigen::Vector2d {320.0, 0.0})).normalized();
        const Eigen::Vector3d right_ray =
            (R * rt::unproject_pinhole32(camera.pinhole, Eigen::Vector2d {639.0, 240.0})).normalized();

        expect_true(top_ray.y() > 0.0, "legacy scene top pixel points upward");
        expect_true(right_ray.x() > 0.0, "legacy scene right pixel points rightward");
    }

    bool move_speed_unknown_threw = false;
    try {
        (void)rt::default_move_speed_for_scene("unknown");
    } catch (...) {
        move_speed_unknown_threw = true;
    }
    expect_true(move_speed_unknown_threw, "unknown move speed throws");
    return 0;
}
