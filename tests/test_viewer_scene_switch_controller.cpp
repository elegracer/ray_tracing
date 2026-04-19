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

void write_scene_file(const fs::path& scene_file, std::string_view scene_id) {
    write_text_file(scene_file, std::string(R"(format_version: 1
scene:
  id: )") + std::string(scene_id) + R"(
  label: Temporary Scene
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
      position: [0.0, 0.0, 2.0]
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
    base_move_speed: 1.0
)");
}

}  // namespace

int main() {
    rt::viewer::SceneSwitchController controller("final_room");
    expect_true(controller.current_scene_id() == "final_room", "initial scene");

    controller.request_scene("cornell_box");
    const rt::viewer::SceneSwitchResult cornell = controller.resolve_pending();
    expect_true(cornell.applied, "cornell box applied");
    expect_true(controller.current_scene_id() == "cornell_box", "cornell box becomes current scene");
    expect_true(cornell.reset_pose, "supported switch resets pose");

    controller.request_scene("smoke");
    const rt::viewer::SceneSwitchResult applied = controller.resolve_pending();
    expect_true(applied.applied, "smoke applied");
    expect_true(controller.current_scene_id() == "smoke", "current scene updated");
    expect_true(applied.reset_pose, "supported switch resets pose");

    controller.request_scene("imported_obj_smoke");
    const rt::viewer::SceneSwitchResult imported = controller.resolve_pending();
    expect_true(imported.applied, "file-backed imported scene applied");
    expect_true(controller.current_scene_id() == "imported_obj_smoke", "file-backed scene becomes current");
    expect_true(imported.reset_pose, "file-backed supported switch resets pose");

    const fs::path root = fs::temp_directory_path() / "viewer_scene_switch_rescan";
    fs::remove_all(root);
    const fs::path scene_file = root / "temporary" / "scene.yaml";
    write_scene_file(scene_file, "temporary_scene");
    rt::scene::global_scene_file_catalog().scan_directory(root);

    rt::viewer::SceneSwitchController temporary("temporary_scene");
    fs::remove(scene_file);
    const rt::viewer::SceneCatalogUpdateResult rescan_result =
        temporary.rescan_scene_directory(root.string());
    expect_true(!rescan_result.ok, "rescan reports missing current scene");
    expect_true(!temporary.last_error().empty(), "rescan stores error");
    expect_true(temporary.current_scene_id() == "temporary_scene", "current scene id unchanged on failed rescan");
    expect_true(rt::find_scene_catalog_entry("temporary_scene") != nullptr,
        "failed rescan restores previous catalog state");
    return 0;
}
