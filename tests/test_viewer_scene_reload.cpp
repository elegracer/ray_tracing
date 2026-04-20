#include "realtime/realtime_scene_factory.h"
#include "realtime/scene_catalog.h"
#include "realtime/viewer/scene_switch_controller.h"
#include "scene/scene_file_catalog.h"
#include "test_support.h"

#include <filesystem>
#include <fstream>
#include <string_view>

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

void write_scene_file(const fs::path& scene_file, std::string_view scene_id, std::string_view label,
    double base_move_speed, double spawn_z) {
    write_text_file(scene_file, std::string(R"(format_version: 1
scene:
  id: )") + std::string(scene_id) + R"(
  label: )" + std::string(label) + R"(
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
cpu_presets:
  default:
    samples_per_pixel: 16
    camera:
      model: pinhole32
      width: 128
      height: 128
      fx: 175.83855484509584
      fy: 175.83855484509584
      cx: 64.0
      cy: 64.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
      pinhole32:
        k1: 0.0
        k2: 0.0
        k3: 0.0
        p1: 0.0
        p2: 0.0
      lookfrom: [0.0, 0.0, 5.0]
      lookat: [0.0, 0.0, 0.0]
      aspect_ratio: 1.0
      image_width: 128
      max_depth: 8
      vup: [0.0, 1.0, 0.0]
      defocus_angle: 0.0
      focus_dist: 5.0
realtime:
  default_view:
    initial_body_pose:
      position: [0.0, 0.0, )" + std::to_string(spawn_z) + R"(]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: legacy_y_up
    camera:
      model: pinhole32
      width: 640
      height: 480
      fx: 415.69219381653056
      fy: 415.69219381653056
      cx: 320.0
      cy: 240.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
      pinhole32:
        k1: 0.0
        k2: 0.0
        k3: 0.0
        p1: 0.0
        p2: 0.0
    base_move_speed: )" + std::to_string(base_move_speed) + "\n");
}

void write_equi_scene_file(const fs::path& scene_file, std::string_view scene_id, std::string_view label,
    double base_move_speed, double spawn_z, double fx, double fy) {
    write_text_file(scene_file, std::string(R"(format_version: 1
scene:
  id: )") + std::string(scene_id) + R"(
  label: )" + std::string(label) + R"(
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
      position: [0.0, 0.0, )" + std::to_string(spawn_z) + R"(]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: world_z_up
    camera:
      model: equi62_lut1d
      width: 320
      height: 240
      fx: )" + std::to_string(fx) + R"(
      fy: )" + std::to_string(fy) + R"(
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
    base_move_speed: )" + std::to_string(base_move_speed) + "\n");
}

void test_reload_current_scene_refreshes_realtime_preset() {
    const fs::path root = fs::temp_directory_path() / "viewer_scene_reload_catalog";
    fs::remove_all(root);

    const fs::path scene_file = root / "editable" / "scene.yaml";
    write_equi_scene_file(scene_file, "editable_scene", "Editable Scene", 1.0, 2.0, 140.0, 142.0);

    rt::scene::global_scene_file_catalog().scan_directory(root);

    rt::viewer::SceneSwitchController controller("editable_scene");
    expect_true(rt::default_move_speed_for_scene("editable_scene") == 1.0, "initial move speed");
    expect_true(rt::default_spawn_pose_for_scene("editable_scene").position.z() == 2.0, "initial spawn pose");
    const rt::PackedCameraRig before_rig = rt::default_camera_rig_for_scene("editable_scene", 1, 640, 480).pack();
    expect_true(before_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "initial reload rig model");
    expect_near(before_rig.cameras[0].equi.fx, 280.0, 1e-9, "initial reload rig fx");
    expect_near(before_rig.cameras[0].equi.fy, 284.0, 1e-9, "initial reload rig fy");
    const rt::SceneCatalogEntry* before = rt::find_scene_catalog_entry("editable_scene");
    expect_true(before != nullptr, "editable scene visible");
    expect_true(before->label == "Editable Scene", "initial label");

    write_equi_scene_file(scene_file, "editable_scene", "Reloaded Scene", 3.5, 5.0, 150.0, 152.0);

    const rt::viewer::SceneCatalogUpdateResult reload_result = controller.reload_current_scene();
    expect_true(reload_result.ok, "reload current scene succeeds");
    expect_true(reload_result.reload_active_scene, "reload requests active scene rebuild");
    expect_true(rt::default_move_speed_for_scene("editable_scene") == 3.5, "move speed updated after reload");
    expect_true(rt::default_spawn_pose_for_scene("editable_scene").position.z() == 5.0,
        "spawn pose updated after reload");
    const rt::PackedCameraRig after_rig = rt::default_camera_rig_for_scene("editable_scene", 1, 640, 480).pack();
    expect_true(after_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "reloaded rig model stays equi");
    expect_near(after_rig.cameras[0].equi.fx, 300.0, 1e-9, "reloaded rig fx");
    expect_near(after_rig.cameras[0].equi.fy, 304.0, 1e-9, "reloaded rig fy");
    const rt::SceneCatalogEntry* after = rt::find_scene_catalog_entry("editable_scene");
    expect_true(after != nullptr, "editable scene still visible");
    expect_true(after->label == "Reloaded Scene", "label updated after reload");
}

void test_rescan_scene_directory_discovers_new_scene() {
    const fs::path root = fs::temp_directory_path() / "viewer_scene_rescan_catalog";
    fs::remove_all(root);

    write_scene_file(root / "first" / "scene.yaml", "first_scene", "First Scene", 1.0, 2.0);
    rt::scene::global_scene_file_catalog().scan_directory(root);

    rt::viewer::SceneSwitchController controller("first_scene");
    expect_true(rt::find_scene_catalog_entry("second_scene") == nullptr, "second scene absent before rescan");

    write_equi_scene_file(root / "second" / "scene.yaml", "second_scene", "Second Scene", 2.0, 3.0, 140.0, 142.0);
    const rt::viewer::SceneCatalogUpdateResult rescan_result =
        controller.rescan_scene_directory(root.string());
    expect_true(rescan_result.ok, "rescan succeeds");
    expect_true(rescan_result.reload_active_scene, "rescan requests active scene rebuild");
    expect_true(rt::find_scene_catalog_entry("second_scene") != nullptr, "second scene visible after rescan");
    const rt::PackedCameraRig second_rig = rt::default_camera_rig_for_scene("second_scene", 1, 640, 480).pack();
    expect_true(second_rig.cameras[0].model == rt::CameraModelType::equi62_lut1d, "rescanned scene keeps equi model");
    expect_near(second_rig.cameras[0].equi.fx, 280.0, 1e-9, "rescanned scene fx scales");
    expect_near(second_rig.cameras[0].equi.fy, 284.0, 1e-9, "rescanned scene fy scales");

    controller.request_scene("second_scene");
    const rt::viewer::SceneSwitchResult switched = controller.resolve_pending();
    expect_true(switched.applied, "switch to rescanned scene succeeds");
}

}  // namespace

int main() {
    test_reload_current_scene_refreshes_realtime_preset();
    test_rescan_scene_directory_discovers_new_scene();
    return 0;
}
