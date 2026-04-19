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
      model: pinhole32
      width: 640
      height: 360
      fx: 494.54593550183205
      fy: 494.54593550183205
      cx: 320.0
      cy: 180.0
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
    camera:
      model: pinhole32
      width: 640
      height: 480
      fx: 362.60044646757626
      fy: 362.60044646757626
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
    expect_true(loaded.cpu_presets.front().camera.camera.model == rt::CameraModelType::pinhole32, "cpu camera model");
    expect_true(loaded.cpu_presets.front().camera.camera.width == 640, "cpu camera width");
    expect_true(loaded.realtime_preset.has_value(), "realtime preset");
    expect_true(loaded.realtime_preset->frame_convention == rt::viewer::ViewerFrameConvention::legacy_y_up,
                "frame convention");
    expect_true(loaded.realtime_preset->camera.model == rt::CameraModelType::pinhole32, "realtime camera model");
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

void test_duplicate_medium_ids_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_medium";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: duplicate_media
  label: Duplicate Media
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    fog:
      type: isotropic
      albedo: white
  shapes:
    volume:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  media:
    smoke:
      shape: volume
      material: fog
      density: 0.01
    smoke:
      shape: volume
      material: fog
      density: 0.02
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate medium id: smoke");
}

void test_includes_are_merged_and_tracked() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_include";
    fs::remove_all(root);
    const fs::path include_file = root / "common" / "materials.yaml";
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(include_file, R"(scene:
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte:
      type: diffuse
      albedo: white
)");
    write_text_file(scene_file, R"(format_version: 1
includes:
  - common/materials.yaml
scene:
  id: include_room
  label: Include Room
  background: [0.0, 0.0, 0.0]
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
    expect_true(loaded.scene_ir.textures().size() == 1, "included texture count");
    expect_true(loaded.scene_ir.materials().size() == 1, "included material count");
    expect_true(loaded.scene_ir.surface_instances().size() == 1, "instance count");
    expect_true(loaded.dependencies.size() == 2, "include dependency count");
    expect_true(loaded.dependencies[0] == scene_file.lexically_normal().string(), "scene file dependency");
    expect_true(loaded.dependencies[1] == include_file.lexically_normal().string(), "include dependency");
}

void test_includes_can_provide_cpu_and_realtime_presets() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_include_presets";
    fs::remove_all(root);
    const fs::path include_file = root / "common" / "presets.yaml";
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(include_file, R"(cpu_presets:
  preview:
    samples_per_pixel: 32
    camera:
      model: pinhole32
      width: 320
      height: 180
      fx: 247.27296775091602
      fy: 247.27296775091602
      cx: 160.0
      cy: 90.0
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
      image_width: 320
realtime:
  default_view:
    initial_body_pose:
      position: [1.0, 2.0, 3.0]
      yaw_deg: 10.0
      pitch_deg: -5.0
    frame_convention: world_z_up
    camera:
      model: pinhole32
      width: 640
      height: 480
      fx: 342.75552161810754
      fy: 342.75552161810754
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
    base_move_speed: 4.0
)");
    write_text_file(scene_file, R"(format_version: 1
includes:
  - common/presets.yaml
scene:
  id: include_presets
  label: Include Presets
)");

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(scene_file);
    expect_true(loaded.metadata.supports_cpu_render, "cpu preset support");
    expect_true(loaded.metadata.supports_realtime, "realtime preset support");
    expect_true(loaded.cpu_presets.size() == 1, "cpu preset count");
    expect_true(loaded.cpu_presets.front().scene_id == "include_presets", "included cpu preset scene id");
    expect_true(loaded.cpu_presets.front().preset_id == "preview", "included cpu preset id");
    expect_true(loaded.cpu_presets.front().samples_per_pixel == 32, "included cpu preset spp");
    expect_true(loaded.cpu_presets.front().camera.camera.width == 320, "included cpu preset width");
    expect_true(loaded.realtime_preset.has_value(), "included realtime preset");
    expect_true(loaded.realtime_preset->frame_convention == rt::viewer::ViewerFrameConvention::world_z_up,
        "included realtime convention");
    expect_true(loaded.realtime_preset->base_move_speed == 4.0, "included realtime speed");
}

void test_obj_imports_are_rebased_and_merged() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_obj_import";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    const fs::path obj_file = root / "models" / "triangle.obj";
    const fs::path mtl_file = root / "models" / "triangle.mtl";
    write_text_file(obj_file,
        "mtllib triangle.mtl\n"
        "usemtl matte\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    write_text_file(mtl_file, "newmtl matte\nKd 0.8 0.7 0.6\n");
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: imported_triangle
  label: Imported Triangle
imports:
  triangle_mesh:
    type: obj_mtl
    obj: models/triangle.obj
)");

    const rt::scene::SceneDefinition loaded = rt::scene::load_scene_definition(scene_file);
    expect_true(loaded.scene_ir.shapes().size() == 1, "imported shape count");
    expect_true(std::holds_alternative<rt::scene::TriangleMeshShape>(loaded.scene_ir.shapes().front()),
        "imported shape type");
    expect_true(loaded.scene_ir.materials().size() == 1, "imported material count");
    expect_true(loaded.scene_ir.surface_instances().size() == 1, "imported instance count");
    expect_true(loaded.dependencies.size() == 3, "import dependencies");
    expect_true(loaded.dependencies[1] == obj_file.lexically_normal().string(), "obj dependency");
    expect_true(loaded.dependencies[2] == mtl_file.lexically_normal().string(), "mtl dependency");
}

void test_duplicate_medium_ids_across_includes_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_medium_include";
    fs::remove_all(root);
    const fs::path include_file = root / "common" / "media.yaml";
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(include_file, R"(scene:
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    fog:
      type: isotropic
      albedo: white
  shapes:
    volume:
      type: sphere
      center: [0.0, 0.0, 0.0]
      radius: 1.0
  media:
    smoke:
      shape: volume
      material: fog
      density: 0.01
)");
    write_text_file(scene_file, R"(format_version: 1
includes:
  - common/media.yaml
scene:
  id: duplicate_include_media
  label: Duplicate Include Media
  media:
    smoke:
      shape: volume
      material: fog
      density: 0.02
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate medium id: smoke");
}

void test_include_errors_are_attributed_to_the_included_file() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_include_error_path";
    fs::remove_all(root);
    const fs::path include_file = root / "common" / "bad_materials.yaml";
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(include_file, R"(scene:
  materials: nope
)");
    write_text_file(scene_file, R"(format_version: 1
includes:
  - common/bad_materials.yaml
scene:
  id: include_error
  label: Include Error
)");

    const std::string error = require_error([&]() { rt::scene::load_scene_definition(scene_file); });
    const std::string expected_prefix = include_file.lexically_normal().string() + ": ";
    expect_true(error.rfind(expected_prefix, 0) == 0, "include error prefix");
    expect_true(error.find("scene.materials must be a map") != std::string::npos, "include error body");
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

void test_missing_camera_model_is_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_missing_camera_model";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: missing_model
  label: Missing Model
cpu_presets:
  default:
    camera:
      width: 640
      height: 360
      fx: 494.54593550183205
      fy: 494.54593550183205
      cx: 320.0
      cy: 180.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "camera.model is required");
}

void test_legacy_camera_fields_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_legacy_camera_fields";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: legacy_camera
  label: Legacy Camera
cpu_presets:
  default:
    camera:
      model: pinhole32
      width: 640
      height: 360
      fx: 494.54593550183205
      fy: 494.54593550183205
      cx: 320.0
      cy: 180.0
      T_bc:
        translation: [0.0, 0.0, 0.0]
        rotation:
          - [1.0, 0.0, 0.0]
          - [0.0, 1.0, 0.0]
          - [0.0, 0.0, 1.0]
      vfov: 40.0
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); },
        "legacy camera fields are not supported");
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

void test_malformed_container_sections_are_rejected() {
    struct Case {
        const char* name;
        const char* contents;
        const char* expected_error;
    };

    const Case cases[] = {
        {
            "bad_cpu_presets",
            R"(format_version: 1
scene:
  id: malformed_cpu_presets
  label: Malformed CPU Presets
cpu_presets: nope
)",
            "cpu_presets must be a map",
        },
        {
            "bad_textures",
            R"(format_version: 1
scene:
  id: malformed_textures
  label: Malformed Textures
  textures: nope
)",
            "scene.textures must be a map",
        },
        {
            "bad_materials",
            R"(format_version: 1
scene:
  id: malformed_materials
  label: Malformed Materials
  materials: nope
)",
            "scene.materials must be a map",
        },
        {
            "bad_shapes",
            R"(format_version: 1
scene:
  id: malformed_shapes
  label: Malformed Shapes
  shapes: nope
)",
            "scene.shapes must be a map",
        },
        {
            "bad_instances",
            R"(format_version: 1
scene:
  id: malformed_instances
  label: Malformed Instances
  instances: nope
)",
            "scene.instances must be a sequence",
        },
        {
            "bad_media",
            R"(format_version: 1
scene:
  id: malformed_media
  label: Malformed Media
  media: nope
)",
            "scene.media must be a map",
        },
    };

    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_sections";
    fs::remove_all(root);
    fs::create_directories(root);

    for (const Case& test_case : cases) {
        const fs::path scene_file = root / test_case.name / "scene.yaml";
        write_text_file(scene_file, test_case.contents);
        expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, test_case.expected_error);
    }
}

void test_malformed_section_entries_are_rejected() {
    struct Case {
        const char* name;
        const char* contents;
        const char* expected_error;
    };

    const Case cases[] = {
        {
            "bad_texture_entry",
            R"(format_version: 1
scene:
  id: malformed_texture_entry
  label: Malformed Texture Entry
  textures:
    white: nope
)",
            "texture must be a map",
        },
        {
            "bad_material_entry",
            R"(format_version: 1
scene:
  id: malformed_material_entry
  label: Malformed Material Entry
  textures:
    white:
      type: constant
      color: [1.0, 1.0, 1.0]
  materials:
    matte: nope
)",
            "material must be a map",
        },
        {
            "bad_shape_entry",
            R"(format_version: 1
scene:
  id: malformed_shape_entry
  label: Malformed Shape Entry
  shapes:
    ball: nope
)",
            "shape must be a map",
        },
        {
            "bad_instance_entry",
            R"(format_version: 1
scene:
  id: malformed_instance_entry
  label: Malformed Instance Entry
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
    - nope
)",
            "instance must be a map",
        },
    };

    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_entries";
    fs::remove_all(root);
    fs::create_directories(root);

    for (const Case& test_case : cases) {
        const fs::path scene_file = root / test_case.name / "scene.yaml";
        write_text_file(scene_file, test_case.contents);
        expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, test_case.expected_error);
    }
}

void test_duplicate_cpu_preset_ids_are_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_duplicate_cpu_preset";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: duplicate_presets
  label: Duplicate Presets
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
  default:
    samples_per_pixel: 64
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
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "duplicate cpu preset id: default");
}

void test_malformed_cpu_preset_entry_is_rejected() {
    const fs::path root = fs::temp_directory_path() / "yaml_scene_loader_bad_cpu_preset_entry";
    fs::remove_all(root);
    const fs::path scene_file = root / "scene.yaml";
    write_text_file(scene_file, R"(format_version: 1
scene:
  id: malformed_cpu_preset_entry
  label: Malformed CPU Preset Entry
cpu_presets:
  default: nope
)");

    expect_throws_contains([&]() { rt::scene::load_scene_definition(scene_file); }, "cpu preset must be a map");
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
    test_duplicate_medium_ids_are_rejected();
    test_includes_are_merged_and_tracked();
    test_includes_can_provide_cpu_and_realtime_presets();
    test_obj_imports_are_rebased_and_merged();
    test_duplicate_medium_ids_across_includes_are_rejected();
    test_include_errors_are_attributed_to_the_included_file();
    test_malformed_optional_transform_is_rejected();
    test_malformed_optional_camera_is_rejected();
    test_missing_camera_model_is_rejected();
    test_legacy_camera_fields_are_rejected();
    test_malformed_optional_default_view_is_rejected();
    test_malformed_container_sections_are_rejected();
    test_malformed_section_entries_are_rejected();
    test_duplicate_cpu_preset_ids_are_rejected();
    test_malformed_cpu_preset_entry_is_rejected();
    test_scene_errors_are_only_prefixed_once();
    return 0;
}
