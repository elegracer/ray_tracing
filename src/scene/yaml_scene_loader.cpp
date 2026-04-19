#include "scene/yaml_scene_loader.h"

#include "yaml-cpp/yaml.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rt::scene {
namespace {

using IdTable = std::unordered_map<std::string, int>;

std::string scene_error_prefix(const std::filesystem::path& scene_file) {
    return scene_file.string() + ": ";
}

std::runtime_error scene_error(const std::filesystem::path& scene_file, std::string_view message) {
    return std::runtime_error(scene_error_prefix(scene_file) + std::string(message));
}

void ensure_map(const YAML::Node& node, std::string_view field_name) {
    if (node.IsDefined() && !node.IsMap()) {
        throw std::runtime_error(std::string(field_name) + " must be a map");
    }
}

void ensure_unique_id(const IdTable& ids, const std::string& id, std::string_view kind) {
    if (ids.find(id) != ids.end()) {
        throw std::runtime_error("duplicate " + std::string(kind) + " id: " + id);
    }
}

Eigen::Vector3d parse_vec3(const YAML::Node& node) {
    if (!node || !node.IsSequence() || node.size() != 3) {
        throw std::runtime_error("expected a 3-element vector");
    }
    return Eigen::Vector3d {node[0].as<double>(), node[1].as<double>(), node[2].as<double>()};
}

Eigen::Matrix3d parse_mat3(const YAML::Node& node) {
    if (!node || !node.IsSequence() || node.size() != 3) {
        throw std::runtime_error("expected a 3x3 matrix");
    }

    Eigen::Matrix3d matrix = Eigen::Matrix3d::Identity();
    for (int row = 0; row < 3; ++row) {
        const YAML::Node row_node = node[row];
        if (!row_node.IsSequence() || row_node.size() != 3) {
            throw std::runtime_error("expected a 3x3 matrix");
        }
        for (int col = 0; col < 3; ++col) {
            matrix(row, col) = row_node[col].as<double>();
        }
    }
    return matrix;
}

int require_id(const IdTable& ids, const std::string& id, std::string_view kind) {
    const auto it = ids.find(id);
    if (it == ids.end()) {
        throw std::runtime_error("unknown " + std::string(kind) + " id: " + id);
    }
    return it->second;
}

Transform parse_transform(const YAML::Node& node) {
    Transform transform = Transform::identity();
    if (!node) {
        return transform;
    }
    ensure_map(node, "transform");
    if (const YAML::Node translation = node["translation"]) {
        transform.translation = parse_vec3(translation);
    }
    if (const YAML::Node rotation = node["rotation"]) {
        transform.rotation = parse_mat3(rotation);
    }
    return transform;
}

void parse_textures(const YAML::Node& textures_node, const std::filesystem::path& scene_directory, SceneDefinition& out,
    IdTable& texture_ids) {
    if (!textures_node) {
        return;
    }

    for (const auto& texture_entry : textures_node) {
        const std::string id = texture_entry.first.as<std::string>();
        ensure_unique_id(texture_ids, id, "texture");
        const YAML::Node texture_node = texture_entry.second;
        const std::string type = texture_node["type"].as<std::string>();

        if (type == "constant") {
            texture_ids.emplace(id, out.scene_ir.add_texture(ConstantColorTextureDesc {.color = parse_vec3(texture_node["color"])}));
            continue;
        }
        if (type == "checker") {
            texture_ids.emplace(id, out.scene_ir.add_texture(CheckerTextureDesc {
                .scale = texture_node["scale"] ? texture_node["scale"].as<double>() : 1.0,
                .even_texture = require_id(texture_ids, texture_node["even"].as<std::string>(), "texture"),
                .odd_texture = require_id(texture_ids, texture_node["odd"].as<std::string>(), "texture"),
            }));
            continue;
        }
        if (type == "image") {
            std::filesystem::path texture_path = texture_node["path"].as<std::string>();
            if (texture_path.is_relative()) {
                texture_path = scene_directory / texture_path;
            }
            texture_path = texture_path.lexically_normal();
            out.dependencies.push_back(texture_path.string());
            texture_ids.emplace(id, out.scene_ir.add_texture(ImageTextureDesc {.path = texture_path.string()}));
            continue;
        }
        if (type == "noise") {
            texture_ids.emplace(id, out.scene_ir.add_texture(NoiseTextureDesc {
                .scale = texture_node["scale"] ? texture_node["scale"].as<double>() : 1.0,
            }));
            continue;
        }

        throw std::runtime_error("unsupported texture type: " + type);
    }
}

void parse_materials(const YAML::Node& materials_node, SceneIR& scene_ir, const IdTable& texture_ids, IdTable& material_ids) {
    if (!materials_node) {
        return;
    }

    for (const auto& material_entry : materials_node) {
        const std::string id = material_entry.first.as<std::string>();
        ensure_unique_id(material_ids, id, "material");
        const YAML::Node material_node = material_entry.second;
        const std::string type = material_node["type"].as<std::string>();

        if (type == "diffuse") {
            material_ids.emplace(id, scene_ir.add_material(DiffuseMaterial {
                .albedo_texture = require_id(texture_ids, material_node["albedo"].as<std::string>(), "texture"),
            }));
            continue;
        }
        if (type == "metal") {
            material_ids.emplace(id, scene_ir.add_material(MetalMaterial {
                .albedo_texture = require_id(texture_ids, material_node["albedo"].as<std::string>(), "texture"),
                .fuzz = material_node["fuzz"] ? material_node["fuzz"].as<double>() : 0.0,
            }));
            continue;
        }
        if (type == "dielectric") {
            material_ids.emplace(id, scene_ir.add_material(DielectricMaterial {
                .ior = material_node["ior"] ? material_node["ior"].as<double>() : 1.0,
            }));
            continue;
        }
        if (type == "emissive") {
            material_ids.emplace(id, scene_ir.add_material(EmissiveMaterial {
                .emission_texture = require_id(texture_ids, material_node["emission"].as<std::string>(), "texture"),
            }));
            continue;
        }
        if (type == "isotropic") {
            material_ids.emplace(id, scene_ir.add_material(IsotropicVolumeMaterial {
                .albedo_texture = require_id(texture_ids, material_node["albedo"].as<std::string>(), "texture"),
            }));
            continue;
        }

        throw std::runtime_error("unsupported material type: " + type);
    }
}

void parse_shapes(const YAML::Node& shapes_node, SceneIR& scene_ir, IdTable& shape_ids) {
    if (!shapes_node) {
        return;
    }

    for (const auto& shape_entry : shapes_node) {
        const std::string id = shape_entry.first.as<std::string>();
        ensure_unique_id(shape_ids, id, "shape");
        const YAML::Node shape_node = shape_entry.second;
        const std::string type = shape_node["type"].as<std::string>();

        if (type == "sphere") {
            shape_ids.emplace(id, scene_ir.add_shape(SphereShape {
                .center = parse_vec3(shape_node["center"]),
                .radius = shape_node["radius"].as<double>(),
            }));
            continue;
        }
        if (type == "quad") {
            shape_ids.emplace(id, scene_ir.add_shape(QuadShape {
                .origin = parse_vec3(shape_node["origin"]),
                .edge_u = parse_vec3(shape_node["edge_u"]),
                .edge_v = parse_vec3(shape_node["edge_v"]),
            }));
            continue;
        }
        if (type == "box") {
            const YAML::Node min_corner = shape_node["min_corner"] ? shape_node["min_corner"] : shape_node["min"];
            const YAML::Node max_corner = shape_node["max_corner"] ? shape_node["max_corner"] : shape_node["max"];
            shape_ids.emplace(id, scene_ir.add_shape(BoxShape {
                .min_corner = parse_vec3(min_corner),
                .max_corner = parse_vec3(max_corner),
            }));
            continue;
        }

        throw std::runtime_error("unsupported shape type: " + type);
    }
}

void parse_instances(const YAML::Node& instances_node, SceneIR& scene_ir, const IdTable& shape_ids, const IdTable& material_ids) {
    if (!instances_node) {
        return;
    }

    for (const YAML::Node& instance_node : instances_node) {
        scene_ir.add_instance(SurfaceInstance {
            .shape_index = require_id(shape_ids, instance_node["shape"].as<std::string>(), "shape"),
            .material_index = require_id(material_ids, instance_node["material"].as<std::string>(), "material"),
            .transform = parse_transform(instance_node["transform"]),
        });
    }
}

void parse_media(const YAML::Node& media_node, SceneIR& scene_ir, const IdTable& shape_ids, const IdTable& material_ids) {
    if (!media_node) {
        return;
    }

    for (const YAML::Node& medium_node : media_node) {
        scene_ir.add_medium(MediumInstance {
            .shape_index = require_id(shape_ids, medium_node["shape"].as<std::string>(), "shape"),
            .material_index = require_id(material_ids, medium_node["material"].as<std::string>(), "material"),
            .density = medium_node["density"].as<double>(),
            .transform = parse_transform(medium_node["transform"]),
        });
    }
}

CpuCameraPreset parse_camera_preset(const YAML::Node& node) {
    CpuCameraPreset camera;
    if (!node) {
        return camera;
    }
    ensure_map(node, "camera");
    if (const YAML::Node aspect_ratio = node["aspect_ratio"]) {
        camera.aspect_ratio = aspect_ratio.as<double>();
    }
    if (const YAML::Node image_width = node["image_width"]) {
        camera.image_width = image_width.as<int>();
    }
    if (const YAML::Node max_depth = node["max_depth"]) {
        camera.max_depth = max_depth.as<int>();
    }
    if (const YAML::Node vfov = node["vfov"]) {
        camera.vfov = vfov.as<double>();
    }
    if (const YAML::Node lookfrom = node["lookfrom"]) {
        camera.lookfrom = parse_vec3(lookfrom);
    }
    if (const YAML::Node lookat = node["lookat"]) {
        camera.lookat = parse_vec3(lookat);
    }
    if (const YAML::Node vup = node["vup"]) {
        camera.vup = parse_vec3(vup);
    }
    if (const YAML::Node defocus_angle = node["defocus_angle"]) {
        camera.defocus_angle = defocus_angle.as<double>();
    }
    if (const YAML::Node focus_dist = node["focus_dist"]) {
        camera.focus_dist = focus_dist.as<double>();
    }
    return camera;
}

void parse_cpu_presets(const YAML::Node& cpu_presets_node, const std::string& scene_id,
    std::vector<SceneDefinitionCpuRenderPreset>& cpu_presets) {
    if (!cpu_presets_node) {
        return;
    }

    for (const auto& preset_entry : cpu_presets_node) {
        SceneDefinitionCpuRenderPreset preset;
        preset.scene_id = scene_id;
        preset.preset_id = preset_entry.first.as<std::string>();
        const YAML::Node preset_node = preset_entry.second;
        if (const YAML::Node samples_per_pixel = preset_node["samples_per_pixel"]) {
            preset.samples_per_pixel = samples_per_pixel.as<int>();
        }
        preset.camera = parse_camera_preset(preset_node["camera"]);
        cpu_presets.push_back(std::move(preset));
    }
}

viewer::ViewerFrameConvention parse_frame_convention(const YAML::Node& node) {
    if (!node) {
        return viewer::default_viewer_frame_convention();
    }

    const std::string value = node.as<std::string>();
    if (value == "world_z_up") {
        return viewer::ViewerFrameConvention::world_z_up;
    }
    if (value == "legacy_y_up" || value == "world_y_up") {
        return viewer::ViewerFrameConvention::legacy_y_up;
    }
    throw std::runtime_error("unsupported frame convention: " + value);
}

RealtimeViewPreset parse_realtime_preset(const YAML::Node& node) {
    RealtimeViewPreset preset;
    if (!node) {
        return preset;
    }
    ensure_map(node, "default_view");

    if (const YAML::Node initial_body_pose = node["initial_body_pose"]) {
        ensure_map(initial_body_pose, "initial_body_pose");
        if (const YAML::Node position = initial_body_pose["position"]) {
            preset.initial_body_pose.position = parse_vec3(position);
        }
        if (const YAML::Node yaw_deg = initial_body_pose["yaw_deg"]) {
            preset.initial_body_pose.yaw_deg = yaw_deg.as<double>();
        }
        if (const YAML::Node pitch_deg = initial_body_pose["pitch_deg"]) {
            preset.initial_body_pose.pitch_deg = pitch_deg.as<double>();
        }
    }
    preset.frame_convention = parse_frame_convention(node["frame_convention"]);
    if (const YAML::Node vfov_deg = node["vfov_deg"]) {
        preset.vfov_deg = vfov_deg.as<double>();
    }
    if (const YAML::Node use_default_viewer_intrinsics = node["use_default_viewer_intrinsics"]) {
        preset.use_default_viewer_intrinsics = use_default_viewer_intrinsics.as<bool>();
    }
    if (const YAML::Node base_move_speed = node["base_move_speed"]) {
        preset.base_move_speed = base_move_speed.as<double>();
    }
    return preset;
}

}  // namespace

SceneDefinition load_scene_definition(const std::filesystem::path& scene_file) {
    try {
        const YAML::Node root = YAML::LoadFile(scene_file.string());
        if (const YAML::Node format_version = root["format_version"]; !format_version || format_version.as<int>() != 1) {
            throw std::runtime_error("unsupported format_version");
        }

        const YAML::Node scene_node = root["scene"];
        const YAML::Node realtime_node = root["realtime"];

        SceneDefinition out;
        out.metadata.id = scene_node["id"].as<std::string>();
        out.metadata.label = scene_node["label"].as<std::string>();
        if (const YAML::Node background = scene_node["background"]) {
            out.metadata.background = parse_vec3(background);
        }
        out.metadata.supports_cpu_render = root["cpu_presets"] && root["cpu_presets"].size() > 0;
        out.metadata.supports_realtime =
            realtime_node.IsDefined() && realtime_node.IsMap() && realtime_node["default_view"].IsDefined();
        out.dependencies.push_back(scene_file.lexically_normal().string());

        IdTable texture_ids;
        IdTable material_ids;
        IdTable shape_ids;
        parse_textures(scene_node["textures"], scene_file.parent_path(), out, texture_ids);
        parse_materials(scene_node["materials"], out.scene_ir, texture_ids, material_ids);
        parse_shapes(scene_node["shapes"], out.scene_ir, shape_ids);
        parse_instances(scene_node["instances"], out.scene_ir, shape_ids, material_ids);
        parse_media(scene_node["media"], out.scene_ir, shape_ids, material_ids);
        parse_cpu_presets(root["cpu_presets"], out.metadata.id, out.cpu_presets);
        if (realtime_node.IsDefined() && !realtime_node.IsMap()) {
            throw std::runtime_error("realtime must be a map");
        }
        if (realtime_node.IsDefined()) {
            const YAML::Node default_view = realtime_node["default_view"];
            if (default_view.IsDefined()) {
            out.realtime_preset = parse_realtime_preset(default_view);
            }
        }
        return out;
    } catch (const YAML::Exception& ex) {
        throw scene_error(scene_file, ex.what());
    } catch (const std::exception& ex) {
        if (std::string_view(ex.what()).rfind(scene_error_prefix(scene_file), 0) == 0) {
            throw;
        }
        throw scene_error(scene_file, ex.what());
    }
}

}  // namespace rt::scene
