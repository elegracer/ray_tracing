#include "realtime/realtime_scene_factory.h"
#include "realtime/camera_models.h"
#include "realtime/viewer/four_camera_rig.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "scene/camera_spec.h"
#include "scene/scene_file_catalog.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <numbers>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

void write_equi_scene_file(const fs::path& scene_file, std::string_view scene_id) {
    write_text_file(scene_file, std::string(R"(format_version: 1
scene:
  id: )") + std::string(scene_id) + R"(
  label: File-backed Equi
  background: [0.0, 0.0, 0.0]
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte:
      type: diffuse
      albedo: white
  shapes:
    ball:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  instances:
    - shape: ball
      material: matte
realtime:
  default_view:
    initial_body_pose:
      position: [1.0, 2.0, 3.0]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: world_z_up
    camera:
      model: equi62_lut1d
      width: 320
      height: 240
      fx: 140.0
      fy: 142.0
      cx: 160.0
      cy: 120.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
      equi62_lut1d:
        radial: [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        tangential: [0.0, 0.0]
    base_move_speed: 2.5
)");
}

}  // namespace

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

    {
        const fs::path root = fs::temp_directory_path() / "realtime_scene_factory_file_backed_equi";
        fs::remove_all(root);
        write_equi_scene_file(root / "file_backed_equi" / "scene.yaml", "file_backed_equi");
        rt::scene::global_scene_file_catalog().scan_directory(root);

        const rt::PackedCameraRig file_backed_rig = rt::default_camera_rig_for_scene("file_backed_equi", 1, 640, 480).pack();
        expect_true(file_backed_rig.active_count == 1, "file-backed equi rig active camera count");
        expect_true(file_backed_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d,
            "file-backed equi rig model");
        expect_true(file_backed_rig.cameras[0].width == 640, "file-backed equi width");
        expect_true(file_backed_rig.cameras[0].height == 480, "file-backed equi height");
        expect_near(file_backed_rig.cameras[0].equi.fx, 280.0, 1e-9, "file-backed equi fx scales");
        expect_near(file_backed_rig.cameras[0].equi.fy, 284.0, 1e-9, "file-backed equi fy scales");
        expect_near(file_backed_rig.cameras[0].equi.cx, 320.0, 1e-9, "file-backed equi cx scales");
        expect_near(file_backed_rig.cameras[0].equi.cy, 240.0, 1e-9, "file-backed equi cy scales");
        expect_vec3_near(file_backed_rig.cameras[0].T_rc.translation(), Eigen::Vector3d {1.0, 2.0, 3.0}, 1e-12,
            "file-backed equi pose translation preserved");
    }

    const rt::PackedCameraRig earth_rig = rt::default_camera_rig_for_scene("earth_sphere", 1, 640, 480).pack();
    expect_true(earth_rig.active_count == 1, "earth rig active camera count");
    expect_true(earth_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "earth rig model");
    expect_vec3_near(earth_rig.cameras[0].T_rc.translation(),
        Eigen::Vector3d {-3.0, 6.0, -10.0}, 1e-12,
        "earth rig uses scene camera preset");
    const double expected_earth_focal =
        0.5 * 640.0 / (rt::default_hfov_deg(rt::CameraModelType::equi62_lut1d) * std::numbers::pi / 360.0);
    expect_near(earth_rig.cameras[0].equi.fy, expected_earth_focal, 1e-9,
        "earth rig preserves authored equi fy at calibration size");
    expect_near(earth_rig.cameras[0].equi.fx, expected_earth_focal, 1e-9,
        "earth rig preserves authored equi fx at calibration size");

    const rt::PackedCameraRig earth_rig_resized = rt::default_camera_rig_for_scene("earth_sphere", 1, 1280, 960).pack();
    expect_true(earth_rig_resized.cameras[0].width == 1280, "earth resized width");
    expect_true(earth_rig_resized.cameras[0].height == 960, "earth resized height");
    expect_near(earth_rig_resized.cameras[0].equi.fx, 2.0 * expected_earth_focal, 1e-9,
        "earth resized rig scales fx");
    expect_near(earth_rig_resized.cameras[0].equi.fy, 2.0 * expected_earth_focal, 1e-9,
        "earth resized rig scales fy");
    expect_near(earth_rig_resized.cameras[0].equi.cx, 640.0, 1e-9,
        "earth resized rig scales cx");
    expect_near(earth_rig_resized.cameras[0].equi.cy, 480.0, 1e-9,
        "earth resized rig scales cy");

    const rt::PackedCameraRig final_room_rig = rt::default_camera_rig_for_scene("final_room", 1, 640, 480).pack();
    expect_true(final_room_rig.cameras[0].model == rt::CameraModelType::pinhole32, "final_room rig model");
    expect_near(final_room_rig.cameras[0].pinhole.fx, 480.0, 1e-9,
        "final_room rig uses explicit authored default-viewer fx");
    expect_near(final_room_rig.cameras[0].pinhole.fy, 360.0, 1e-9,
        "final_room rig uses explicit authored default-viewer fy");

    const rt::PackedScene default_view = rt::viewer::make_default_viewer_scene().pack();
    expect_true(default_view.material_count == final_room.material_count, "default viewer materials match final_room");
    expect_true(default_view.sphere_count == final_room.sphere_count, "default viewer spheres match final_room");
    expect_true(default_view.quad_count == final_room.quad_count, "default viewer scene is final_room");

    {
        constexpr const char* kLegacyScenes[] {"earth_sphere", "quads", "simple_light", "rttnw_final_scene"};
        for (const char* scene_id : kLegacyScenes) {
            const rt::viewer::BodyPose spawn = rt::default_spawn_pose_for_scene(scene_id);
            const rt::viewer::ViewerFrameConvention convention = rt::viewer_frame_convention_for_scene(scene_id);
            const rt::PackedCameraRig preset_rig = rt::default_camera_rig_for_scene(scene_id, 1, 640, 480).pack();
            rt::scene::CameraSpec authored = {};
            authored.model = preset_rig.cameras[0].model;
            authored.width = 640;
            authored.height = 480;
            authored.fx = preset_rig.cameras[0].model == rt::CameraModelType::pinhole32
                ? preset_rig.cameras[0].pinhole.fx
                : preset_rig.cameras[0].equi.fx;
            authored.fy = preset_rig.cameras[0].model == rt::CameraModelType::pinhole32
                ? preset_rig.cameras[0].pinhole.fy
                : preset_rig.cameras[0].equi.fy;
            authored.cx = preset_rig.cameras[0].model == rt::CameraModelType::pinhole32
                ? preset_rig.cameras[0].pinhole.cx
                : preset_rig.cameras[0].equi.cx;
            authored.cy = preset_rig.cameras[0].model == rt::CameraModelType::pinhole32
                ? preset_rig.cameras[0].pinhole.cy
                : preset_rig.cameras[0].equi.cy;
            const rt::PackedCameraRig viewer_rig =
                rt::viewer::make_default_viewer_rig(spawn, authored, 1, 640, 480, convention).pack();

            const Eigen::Vector3d viewer_forward =
                viewer_rig.cameras[0].T_rc.rotationMatrix() * Eigen::Vector3d(0.0, 0.0, 1.0);
            const Eigen::Vector3d preset_forward =
                preset_rig.cameras[0].T_rc.rotationMatrix() * Eigen::Vector3d(0.0, 0.0, 1.0);
            expect_vec3_near(viewer_forward, preset_forward, 1e-9, "viewer spawn forward matches scene preset");

            const Eigen::Vector3d viewer_up =
                viewer_rig.cameras[0].T_rc.rotationMatrix() * Eigen::Vector3d(0.0, -1.0, 0.0);
            const Eigen::Vector3d preset_up =
                preset_rig.cameras[0].T_rc.rotationMatrix() * Eigen::Vector3d(0.0, -1.0, 0.0);
            expect_vec3_near(viewer_up, preset_up, 1e-9, "viewer spawn up matches scene preset");
        }
    }

    {
        rt::scene::CameraSpec equi_authored {};
        equi_authored.model = rt::CameraModelType::equi62_lut1d;
        equi_authored.width = 320;
        equi_authored.height = 240;
        equi_authored.fx = 140.0;
        equi_authored.fy = 142.0;
        equi_authored.cx = 160.0;
        equi_authored.cy = 120.0;
        equi_authored.equi62_lut1d.radial = std::array<double, 6> {};
        equi_authored.equi62_lut1d.tangential = Eigen::Vector2d::Zero();

        const rt::viewer::BodyPose pose {
            .position = Eigen::Vector3d(1.0, 2.0, 3.0),
            .yaw_deg = 5.0,
            .pitch_deg = 10.0,
        };
        const rt::PackedCameraRig equi_rig =
            rt::viewer::make_default_viewer_rig(pose, equi_authored, 1, 640, 480,
                rt::viewer::ViewerFrameConvention::world_z_up)
                .pack();
        expect_true(equi_rig.active_count == 1, "equi authored rig active count");
        expect_true(equi_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "equi authored rig model");
        expect_near(equi_rig.cameras[0].equi.fx, 280.0, 1e-9, "equi authored fx scales with width");
        expect_near(equi_rig.cameras[0].equi.fy, 284.0, 1e-9, "equi authored fy scales with height");
        expect_near(equi_rig.cameras[0].equi.cx, 320.0, 1e-9, "equi authored cx scales with width");
        expect_near(equi_rig.cameras[0].equi.cy, 240.0, 1e-9, "equi authored cy scales with height");
        expect_vec3_near(equi_rig.cameras[0].T_rc.translation(), pose.position, 1e-12,
            "equi authored pose translation preserved");
    }

    {
        const rt::PackedCameraRig quads_rig = rt::default_camera_rig_for_scene("quads", 1, 640, 480).pack();
        const rt::PackedCamera& camera = quads_rig.cameras[0];
        const Eigen::Matrix3d R = camera.T_rc.rotationMatrix();
        const Eigen::Vector3d top_ray =
            (R * rt::unproject_equi62_lut1d(camera.equi, Eigen::Vector2d {320.0, 0.0})).normalized();
        const Eigen::Vector3d right_ray =
            (R * rt::unproject_equi62_lut1d(camera.equi, Eigen::Vector2d {639.0, 240.0})).normalized();

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
