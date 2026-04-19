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

void test_scan_includes_builtin_and_file_backed_scenes() {
    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory("assets/scenes");

    expect_true(!catalog.entries().empty(), "catalog has entries");
    expect_true(catalog.find_scene("final_room") != nullptr, "builtin final_room loaded");
    expect_true(catalog.find_scene("imported_obj_smoke") != nullptr, "file-backed scene loaded");

    const auto status = catalog.reload_scene("final_room");
    expect_true(status.ok, "reload builtin scene");
}

void test_reload_scene_picks_up_file_changes() {
    const fs::path root = fs::temp_directory_path() / "scene_file_catalog_reload";
    fs::remove_all(root);

    const fs::path scene_file = root / "editable" / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: editable_scene
  label: Editable Scene
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
      position: [0.0, 0.0, 2.0]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: legacy_y_up
    vfov_deg: 60.0
    use_default_viewer_intrinsics: false
    base_move_speed: 1.0
)");

    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory(root);

    const rt::scene::SceneDefinition* before = catalog.find_scene("editable_scene");
    expect_true(before != nullptr, "editable scene present");
    expect_true(before->metadata.label == "Editable Scene", "editable scene initial label");

    write_text_file(scene_file, R"(format_version: 1
scene:
  id: editable_scene
  label: Reloaded Scene
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
    samples_per_pixel: 32
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
      position: [0.0, 0.0, 2.0]
      yaw_deg: 0.0
      pitch_deg: 0.0
    frame_convention: legacy_y_up
    vfov_deg: 60.0
    use_default_viewer_intrinsics: false
    base_move_speed: 1.0
)");

    const auto reloaded = catalog.reload_scene("editable_scene");
    expect_true(reloaded.ok, "reload file-backed scene");
    const rt::scene::SceneDefinition* after = catalog.find_scene("editable_scene");
    expect_true(after != nullptr, "editable scene present after reload");
    expect_true(after->metadata.label == "Reloaded Scene", "editable scene label updated");
    expect_true(catalog.default_cpu_render_preset("editable_scene")->samples_per_pixel == 32, "editable scene spp updated");
}

void test_rescan_discovers_new_scene_files() {
    const fs::path root = fs::temp_directory_path() / "scene_file_catalog_rescan";
    fs::remove_all(root);

    write_text_file(root / "first" / "scene.yaml", R"(format_version: 1
scene:
  id: first_scene
  label: First Scene
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
)");

    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory(root);
    expect_true(catalog.find_scene("first_scene") != nullptr, "first scene discovered");
    expect_true(catalog.find_scene("second_scene") == nullptr, "second scene absent before rescan");

    write_text_file(root / "second" / "scene.yaml", R"(format_version: 1
scene:
  id: second_scene
  label: Second Scene
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
)");

    catalog.scan_directory(root);
    expect_true(catalog.find_scene("second_scene") != nullptr, "second scene discovered after rescan");
}

void test_failed_scan_preserves_existing_builtin_fallback() {
    const fs::path root = fs::temp_directory_path() / "scene_file_catalog_bad_scan";
    fs::remove_all(root);

    write_text_file(root / "broken" / "scene.yaml", R"(format_version: 1
scene:
  id: broken_scene
  label: Broken Scene
  materials: nope
)");

    rt::scene::SceneFileCatalog catalog;
    bool scan_threw = false;
    try {
        catalog.scan_directory(root);
    } catch (...) {
        scan_threw = true;
    }
    expect_true(scan_threw, "scan_directory surfaces file-backed scene errors");
    expect_true(catalog.find_scene("final_room") != nullptr, "builtin fallback preserved after failed scan");
}

}  // namespace

int main() {
    test_scan_includes_builtin_and_file_backed_scenes();
    test_reload_scene_picks_up_file_changes();
    test_rescan_discovers_new_scene_files();
    test_failed_scan_preserves_existing_builtin_fallback();
    return 0;
}
