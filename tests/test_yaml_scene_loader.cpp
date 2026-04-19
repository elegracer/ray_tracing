#include "scene/yaml_scene_loader.h"

#include <Eigen/Core>

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace fs = std::filesystem;

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void write_text_file(const fs::path& path, std::string_view contents) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << contents;
}

void expect_throws_contains(const std::function<void()>& fn, std::string_view expected_substring) {
    try {
        fn();
    } catch (const std::exception& ex) {
        expect_true(std::string_view(ex.what()).find(expected_substring) != std::string_view::npos, "error substring");
        return;
    }
    throw std::runtime_error("expected exception");
}

std::string require_error(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        return ex.what();
    }
    throw std::runtime_error("expected exception");
}

void test_basic_scene_load() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_basic";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
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
)");

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
}

void test_relative_image_paths_are_rebased_and_recorded() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_relative_image";
    fs::remove_all(root);
    const fs::path scene_dir = root / "scenes" / "demo";
    const fs::path scene_file = scene_dir / "scene.yaml";
    const fs::path image_file = scene_dir / "textures" / "earth.png";
    write_text_file(image_file, "not-an-image");
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: textured_room
  label: Textured Room
  textures:
    earth:
      type: image
      path: textures/earth.png
  materials:
    matte:
      type: diffuse
      albedo: earth
  shapes:
    ball:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  instances:
    - shape: ball
      material: matte
)");

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(scene_file);
    expect_true(loaded.scene_ir.textures().size() == 1, "relative image texture count");
    expect_true(std::holds_alternative<rt::scene::ImageTextureDesc>(loaded.scene_ir.textures().front()),
                "image texture variant");
    const auto& image = std::get<rt::scene::ImageTextureDesc>(loaded.scene_ir.textures().front());
    expect_true(image.path == image_file.lexically_normal().string(), "resolved image path");
    expect_true(loaded.dependencies.size() == 2, "dependency count with image");
    expect_true(loaded.dependencies[0] == scene_file.lexically_normal().string(), "scene dependency path");
    expect_true(loaded.dependencies[1] == image_file.lexically_normal().string(), "image dependency path");
}

void test_duplicate_texture_ids_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_texture";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: duplicates
  label: Duplicates
  textures:
    albedo:
      type: constant
      color: [1.0, 1.0, 1.0]
    albedo:
      type: constant
      color: [0.0, 0.0, 0.0]
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate texture id: albedo");
}

void test_duplicate_material_ids_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_material";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: duplicates
  label: Duplicates
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte:
      type: diffuse
      albedo: white
    matte:
      type: diffuse
      albedo: white
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate material id: matte");
}

void test_duplicate_shape_ids_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_shape";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: duplicates
  label: Duplicates
  shapes:
    ball:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
    ball:
      type: sphere
      center: [1.0, 0.0, 0.0]
      radius: 2.0
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate shape id: ball");
}

void test_malformed_optional_transform_is_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_transform";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: malformed_transform
  label: Malformed Transform
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
      transform: [1.0, 2.0, 3.0]
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "transform must be a map");
}

void test_malformed_optional_camera_is_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_camera";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: malformed_camera
  label: Malformed Camera
cpu_presets:
  default:
    camera: [1.0, 2.0, 3.0]
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "camera must be a map");
}

void test_malformed_optional_default_view_is_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_default_view";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: malformed_default_view
  label: Malformed Default View
realtime:
  default_view: [1.0, 2.0, 3.0]
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "default_view must be a map");
}

void test_scene_errors_are_only_prefixed_once() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_single_prefix";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 99
scene:
  id: bad_version
  label: Bad Version
)");

    const std::string error = require_error([&]() { rt::scene::load_scene_definition(scene_file); });
    const std::string prefix = scene_file.string() + ": ";
    expect_true(error == prefix + "unsupported format_version", "single prefix error text");
    expect_true(error.find(prefix + prefix) == std::string::npos, "no double prefix");
}

}  // namespace

int main() {
    test_basic_scene_load();
    test_relative_image_paths_are_rebased_and_recorded();
    test_duplicate_texture_ids_are_rejected();
    test_duplicate_material_ids_are_rejected();
    test_duplicate_shape_ids_are_rejected();
    test_malformed_optional_transform_is_rejected();
    test_malformed_optional_camera_is_rejected();
    test_malformed_optional_default_view_is_rejected();
    test_scene_errors_are_only_prefixed_once();
    return 0;
}
