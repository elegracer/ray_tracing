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
      lookfrom: [0.0, 0.0, 5.0]
      lookat: [0.0, 0.0, 0.0]
      vfov: 40.0
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
    vfov_deg: 60.0
    use_default_viewer_intrinsics: false
    base_move_speed: )" + std::to_string(base_move_speed) + "\n");
}

void test_reload_current_scene_refreshes_realtime_preset() {
    const fs::path root = fs::temp_directory_path() / "viewer_scene_reload_catalog";
    fs::remove_all(root);

    const fs::path scene_file = root / "editable" / "scene.yaml";
    write_scene_file(scene_file, "editable_scene", "Editable Scene", 1.0, 2.0);

    rt::scene::global_scene_file_catalog().scan_directory(root);

    rt::viewer::SceneSwitchController controller("editable_scene");
    expect_true(rt::default_move_speed_for_scene("editable_scene") == 1.0, "initial move speed");
    expect_true(rt::default_spawn_pose_for_scene("editable_scene").position.z() == 2.0, "initial spawn pose");
    const rt::SceneCatalogEntry* before = rt::find_scene_catalog_entry("editable_scene");
    expect_true(before != nullptr, "editable scene visible");
    expect_true(before->label == "Editable Scene", "initial label");

    write_scene_file(scene_file, "editable_scene", "Reloaded Scene", 3.5, 5.0);

    const rt::viewer::SceneCatalogUpdateResult reload_result = controller.reload_current_scene();
    expect_true(reload_result.ok, "reload current scene succeeds");
    expect_true(reload_result.reload_active_scene, "reload requests active scene rebuild");
    expect_true(rt::default_move_speed_for_scene("editable_scene") == 3.5, "move speed updated after reload");
    expect_true(rt::default_spawn_pose_for_scene("editable_scene").position.z() == 5.0,
        "spawn pose updated after reload");
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

    write_scene_file(root / "second" / "scene.yaml", "second_scene", "Second Scene", 2.0, 3.0);
    const rt::viewer::SceneCatalogUpdateResult rescan_result =
        controller.rescan_scene_directory(root.string());
    expect_true(rescan_result.ok, "rescan succeeds");
    expect_true(rescan_result.reload_active_scene, "rescan requests active scene rebuild");
    expect_true(rt::find_scene_catalog_entry("second_scene") != nullptr, "second scene visible after rescan");

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
