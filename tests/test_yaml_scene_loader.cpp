#include "scene/yaml_scene_loader.h"

#include <Eigen/Core>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_basic";
    fs::create_directories(root);
    const fs::path scene_file = root / "scene.yaml";
    std::ofstream(scene_file) << R"(format_version: 1
scene:
  id: basic_room
  label: Basic Room
  background: [0.1, 0.2, 0.3]
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
    samples_per_pixel: 64
    camera:
      lookfrom: [0.0, 0.0, 5.0]
      lookat: [0.0, 0.0, 0.0]
      vfov: 40.0
      aspect_ratio: 1.7777778
      image_width: 640
      max_depth: 16
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
    vfov_deg: 67.0
    use_default_viewer_intrinsics: false
    base_move_speed: 2.5
)";

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(scene_file);
    expect_true(loaded.metadata.id == "basic_room", "scene id");
    expect_true(loaded.metadata.label == "Basic Room", "scene label");
    expect_true(loaded.metadata.background.isApprox(Eigen::Vector3d(0.1, 0.2, 0.3)), "background");
    expect_true(loaded.metadata.supports_cpu_render, "cpu support");
    expect_true(loaded.metadata.supports_realtime, "realtime support");
    expect_true(loaded.scene_ir.textures().size() == 1, "texture count");
    expect_true(loaded.scene_ir.materials().size() == 1, "material count");
    expect_true(loaded.scene_ir.shapes().size() == 1, "shape count");
    expect_true(loaded.scene_ir.surface_instances().size() == 1, "instance count");
    expect_true(loaded.cpu_presets.size() == 1, "cpu preset count");
    expect_true(loaded.cpu_presets.front().scene_id == "basic_room", "cpu preset scene id");
    expect_true(loaded.cpu_presets.front().preset_id == "default", "cpu preset id");
    expect_true(loaded.cpu_presets.front().samples_per_pixel == 64, "cpu preset spp");
    expect_true(loaded.realtime_preset.has_value(), "realtime preset");
    expect_true(loaded.realtime_preset->frame_convention == rt::viewer::ViewerFrameConvention::legacy_y_up,
                "frame convention");
    expect_true(loaded.dependencies.size() == 1, "dependency count");
    expect_true(loaded.dependencies.front() == scene_file.string(), "dependency path");
    return 0;
}
