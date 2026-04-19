#include "scene/yaml_scene_loader.h"

#include "scene/obj_mtl_importer.h"

#include "yaml-cpp/yaml.h"

#include <type_traits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace rt::scene {
namespace {

using IdTable = std::unordered_map<std::string, int>;
using StringSet = std::unordered_set<std::string>;

std::string scene_error_prefix(const std::filesystem::path& scene_file) {
    return scene_file.string() + ": ";
}

std::runtime_error scene_error(const std::filesystem::path& scene_file, std::string_view message) {
    return std::runtime_error(scene_error_prefix(scene_file) + std::string(message));
}

bool has_file_error_prefix(std::string_view message) {
    const std::size_t separator = message.find(": ");
    if (separator == std::string_view::npos) {
        return false;
    }
    const std::string_view prefix = message.substr(0, separator);
    return prefix.find('/') != std::string_view::npos || prefix.find('\\') != std::string_view::npos;
}

void ensure_map(const YAML::Node& node, std::string_view field_name) {
    if (node.IsDefined() && !node.IsMap()) {
        throw std::runtime_error(std::string(field_name) + " must be a map");
    }
}

void ensure_sequence(const YAML::Node& node, std::string_view field_name) {
    if (node.IsDefined() && !node.IsSequence()) {
        throw std::runtime_error(std::string(field_name) + " must be a sequence");
    }
}

void ensure_unique_id(const IdTable& ids, const std::string& id, std::string_view kind) {
    if (ids.find(id) != ids.end()) {
        throw std::runtime_error("duplicate " + std::string(kind) + " id: " + id);
    }
}

void append_unique_dependency(std::vector<std::string>& dependencies, StringSet& seen, const std::filesystem::path& path) {
    const std::string normalized = path.lexically_normal().string();
    if (seen.insert(normalized).second) {
        dependencies.push_back(normalized);
    }
}

Eigen::Vector3d parse_vec3(const YAML::Node& node) {
    if (!node || !node.IsSequence() || node.size() != 3) {
        throw std::runtime_error("expected a 3-element vector");
    }
    return Eigen::Vector3d {node[0].as<double>(), node[1].as<double>(), node[2].as<double>()};
}

Eigen::Vector2d parse_vec2(const YAML::Node& node) {
    if (!node || !node.IsSequence() || node.size() != 2) {
        throw std::runtime_error("expected a 2-element vector");
    }
    return Eigen::Vector2d {node[0].as<double>(), node[1].as<double>()};
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

Eigen::Isometry3d parse_isometry(const YAML::Node& node, std::string_view field_name) {
    if (!node) {
        throw std::runtime_error(std::string(field_name) + " must be a map");
    }
    ensure_map(node, field_name);

    Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
    if (const YAML::Node translation = node["translation"]) {
        transform.translation() = parse_vec3(translation);
    }
    if (const YAML::Node rotation = node["rotation"]) {
        transform.linear() = parse_mat3(rotation);
    }
    return transform;
}

CameraModelType parse_camera_model(const YAML::Node& node) {
    if (!node) {
        throw std::runtime_error("camera.model is required");
    }

    const std::string value = node.as<std::string>();
    if (value == "pinhole32") {
        return CameraModelType::pinhole32;
    }
    if (value == "equi62_lut1d") {
        return CameraModelType::equi62_lut1d;
    }
    throw std::runtime_error("unsupported camera model: " + value);
}

CameraSpec parse_camera_spec(const YAML::Node& node) {
    if (!node) {
        throw std::runtime_error("camera is required");
    }
    ensure_map(node, "camera");

    if (node["vfov"] || node["vfov_deg"] || node["use_default_viewer_intrinsics"]) {
        throw std::runtime_error("legacy camera fields are not supported; use explicit camera model and intrinsics");
    }

    CameraSpec spec;
    spec.model = parse_camera_model(node["model"]);

    if (!node["width"] || !node["height"] || !node["fx"] || !node["fy"] || !node["cx"] || !node["cy"]) {
        throw std::runtime_error("camera requires width, height, fx, fy, cx, and cy");
    }
    spec.width = node["width"].as<int>();
    spec.height = node["height"].as<int>();
    spec.fx = node["fx"].as<double>();
    spec.fy = node["fy"].as<double>();
    spec.cx = node["cx"].as<double>();
    spec.cy = node["cy"].as<double>();
    spec.T_bc = parse_isometry(node["T_bc"], "camera.T_bc");

    if (const YAML::Node pinhole32 = node["pinhole32"]) {
        ensure_map(pinhole32, "camera.pinhole32");
        if (const YAML::Node k1 = pinhole32["k1"]) {
            spec.pinhole32.k1 = k1.as<double>();
        }
        if (const YAML::Node k2 = pinhole32["k2"]) {
            spec.pinhole32.k2 = k2.as<double>();
        }
        if (const YAML::Node k3 = pinhole32["k3"]) {
            spec.pinhole32.k3 = k3.as<double>();
        }
        if (const YAML::Node p1 = pinhole32["p1"]) {
            spec.pinhole32.p1 = p1.as<double>();
        }
        if (const YAML::Node p2 = pinhole32["p2"]) {
            spec.pinhole32.p2 = p2.as<double>();
        }
    }

    if (const YAML::Node equi62 = node["equi62_lut1d"]) {
        ensure_map(equi62, "camera.equi62_lut1d");
        if (const YAML::Node radial = equi62["radial"]) {
            ensure_sequence(radial, "camera.equi62_lut1d.radial");
            if (radial.size() != spec.equi62_lut1d.radial.size()) {
                throw std::runtime_error("camera.equi62_lut1d.radial must have 6 entries");
            }
            for (std::size_t i = 0; i < spec.equi62_lut1d.radial.size(); ++i) {
                spec.equi62_lut1d.radial[i] = radial[i].as<double>();
            }
        }
        if (const YAML::Node tangential = equi62["tangential"]) {
            spec.equi62_lut1d.tangential = parse_vec2(tangential);
        }
    }

    return spec;
}

void parse_textures(const YAML::Node& textures_node, const std::filesystem::path& scene_directory, SceneDefinition& out,
    IdTable& texture_ids, StringSet& dependency_set) {
    if (!textures_node) {
        return;
    }
    ensure_map(textures_node, "scene.textures");

    for (const auto& texture_entry : textures_node) {
        const std::string id = texture_entry.first.as<std::string>();
        ensure_unique_id(texture_ids, id, "texture");
        const YAML::Node texture_node = texture_entry.second;
        ensure_map(texture_node, "texture");
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
            append_unique_dependency(out.dependencies, dependency_set, texture_path);
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
    ensure_map(materials_node, "scene.materials");

    for (const auto& material_entry : materials_node) {
        const std::string id = material_entry.first.as<std::string>();
        ensure_unique_id(material_ids, id, "material");
        const YAML::Node material_node = material_entry.second;
        ensure_map(material_node, "material");
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
    ensure_map(shapes_node, "scene.shapes");

    for (const auto& shape_entry : shapes_node) {
        const std::string id = shape_entry.first.as<std::string>();
        ensure_unique_id(shape_ids, id, "shape");
        const YAML::Node shape_node = shape_entry.second;
        ensure_map(shape_node, "shape");
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
    ensure_sequence(instances_node, "scene.instances");

    for (const YAML::Node& instance_node : instances_node) {
        ensure_map(instance_node, "instance");
        scene_ir.add_instance(SurfaceInstance {
            .shape_index = require_id(shape_ids, instance_node["shape"].as<std::string>(), "shape"),
            .material_index = require_id(material_ids, instance_node["material"].as<std::string>(), "material"),
            .transform = parse_transform(instance_node["transform"]),
        });
    }
}

void parse_media(const YAML::Node& media_node, SceneIR& scene_ir, const IdTable& shape_ids, const IdTable& material_ids,
    IdTable& medium_ids) {
    if (!media_node) {
        return;
    }
    ensure_map(media_node, "scene.media");

    for (const auto& medium_entry : media_node) {
        const std::string id = medium_entry.first.as<std::string>();
        ensure_unique_id(medium_ids, id, "medium");
        medium_ids.emplace(id, 0);
        const YAML::Node medium_node = medium_entry.second;
        ensure_map(medium_node, "medium");
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
        throw std::runtime_error("camera is required");
    }
    camera.camera = parse_camera_spec(node);
    if (const YAML::Node aspect_ratio = node["aspect_ratio"]) {
        camera.aspect_ratio = aspect_ratio.as<double>();
    }
    if (const YAML::Node image_width = node["image_width"]) {
        camera.image_width = image_width.as<int>();
    }
    if (const YAML::Node max_depth = node["max_depth"]) {
        camera.max_depth = max_depth.as<int>();
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
    std::vector<SceneDefinitionCpuRenderPreset>& cpu_presets, IdTable& preset_ids) {
    if (!cpu_presets_node) {
        return;
    }
    ensure_map(cpu_presets_node, "cpu_presets");

    for (const auto& preset_entry : cpu_presets_node) {
        const std::string preset_id = preset_entry.first.as<std::string>();
        ensure_unique_id(preset_ids, preset_id, "cpu preset");
        preset_ids.emplace(preset_id, 0);

        SceneDefinitionCpuRenderPreset preset;
        preset.scene_id = scene_id;
        preset.preset_id = preset_id;
        const YAML::Node preset_node = preset_entry.second;
        ensure_map(preset_node, "cpu preset");
        if (const YAML::Node samples_per_pixel = preset_node["samples_per_pixel"]) {
            preset.samples_per_pixel = samples_per_pixel.as<int>();
        }
        if (!preset_node["camera"]) {
            throw std::runtime_error("cpu preset camera is required");
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
        throw std::runtime_error("default_view is required");
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
    if (!node["camera"]) {
        throw std::runtime_error("realtime default_view.camera is required");
    }
    preset.camera = parse_camera_spec(node["camera"]);
    if (const YAML::Node base_move_speed = node["base_move_speed"]) {
        preset.base_move_speed = base_move_speed.as<double>();
    }
    return preset;
}

void parse_realtime_section(const YAML::Node& realtime_node, std::optional<RealtimeViewPreset>& realtime_preset) {
    if (realtime_node.IsDefined() && !realtime_node.IsMap()) {
        throw std::runtime_error("realtime must be a map");
    }
    if (!realtime_node.IsDefined()) {
        return;
    }

    const YAML::Node default_view = realtime_node["default_view"];
    if (!default_view.IsDefined()) {
        return;
    }
    if (realtime_preset.has_value()) {
        throw std::runtime_error("duplicate realtime preset");
    }
    realtime_preset = parse_realtime_preset(default_view);
}

TextureDesc remap_texture_desc(const TextureDesc& texture_desc) {
    return texture_desc;
}

MaterialDesc remap_material_desc(const MaterialDesc& material_desc, int texture_offset) {
    return std::visit(
        [texture_offset](const auto& material) -> MaterialDesc {
            using T = std::decay_t<decltype(material)>;
            if constexpr (std::is_same_v<T, DiffuseMaterial>) {
                return DiffuseMaterial {.albedo_texture = material.albedo_texture + texture_offset};
            } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                return MetalMaterial {
                    .albedo_texture = material.albedo_texture + texture_offset,
                    .fuzz = material.fuzz,
                };
            } else if constexpr (std::is_same_v<T, DielectricMaterial>) {
                return material;
            } else if constexpr (std::is_same_v<T, EmissiveMaterial>) {
                return EmissiveMaterial {.emission_texture = material.emission_texture + texture_offset};
            } else if constexpr (std::is_same_v<T, IsotropicVolumeMaterial>) {
                return IsotropicVolumeMaterial {.albedo_texture = material.albedo_texture + texture_offset};
            } else {
                static_assert(!sizeof(T*), "unsupported material type");
            }
        },
        material_desc);
}

ShapeDesc remap_shape_desc(const ShapeDesc& shape_desc) {
    return shape_desc;
}

void append_imported_scene_ir(SceneIR& dst, const SceneIR& src) {
    const int texture_offset = static_cast<int>(dst.textures().size());
    const int material_offset = static_cast<int>(dst.materials().size());
    const int shape_offset = static_cast<int>(dst.shapes().size());

    for (const TextureDesc& texture : src.textures()) {
        dst.add_texture(remap_texture_desc(texture));
    }
    for (const MaterialDesc& material : src.materials()) {
        dst.add_material(remap_material_desc(material, texture_offset));
    }
    for (const ShapeDesc& shape : src.shapes()) {
        dst.add_shape(remap_shape_desc(shape));
    }
    for (const SurfaceInstance& instance : src.surface_instances()) {
        dst.add_instance(SurfaceInstance {
            .shape_index = instance.shape_index + shape_offset,
            .material_index = instance.material_index + material_offset,
            .transform = instance.transform,
        });
    }
    for (const MediumInstance& medium : src.media()) {
        dst.add_medium(MediumInstance {
            .shape_index = medium.shape_index + shape_offset,
            .material_index = medium.material_index + material_offset,
            .density = medium.density,
            .transform = medium.transform,
        });
    }
}

void parse_imports(const YAML::Node& imports_node, const std::filesystem::path& scene_directory, SceneDefinition& out,
    StringSet& dependency_set) {
    if (!imports_node) {
        return;
    }
    ensure_map(imports_node, "imports");

    for (const auto& import_entry : imports_node) {
        const YAML::Node import_node = import_entry.second;
        ensure_map(import_node, "import");
        const std::string type = import_node["type"].as<std::string>();
        if (type != "obj_mtl") {
            throw std::runtime_error("unsupported import type: " + type);
        }
        if (import_node["mtl"]) {
            throw std::runtime_error("explicit mtl overrides are not supported");
        }

        std::filesystem::path obj_path = import_node["obj"].as<std::string>();
        if (obj_path.is_relative()) {
            obj_path = scene_directory / obj_path;
        }
        obj_path = obj_path.lexically_normal();

        const ObjImportResult imported = import_obj_mtl(obj_path);
        append_imported_scene_ir(out.scene_ir, imported.scene_ir);
        for (const std::string& dependency : imported.dependencies) {
            append_unique_dependency(out.dependencies, dependency_set, dependency);
        }
    }
}

void load_scene_file(const std::filesystem::path& scene_file, bool require_format_version, bool parse_metadata,
    SceneDefinition& out, IdTable& texture_ids, IdTable& material_ids, IdTable& shape_ids, IdTable& medium_ids,
    IdTable& preset_ids, StringSet& dependency_set, StringSet& active_files) {
    const std::filesystem::path normalized_scene = scene_file.lexically_normal();
    try {
        if (!active_files.insert(normalized_scene.string()).second) {
            throw std::runtime_error("include cycle detected");
        }

        const YAML::Node root = YAML::LoadFile(normalized_scene.string());
        ensure_map(root, "root");
        if (const YAML::Node format_version = root["format_version"]) {
            if (format_version.as<int>() != 1) {
                throw std::runtime_error("unsupported format_version");
            }
        } else if (require_format_version) {
            throw std::runtime_error("unsupported format_version");
        }

        append_unique_dependency(out.dependencies, dependency_set, normalized_scene);

        const YAML::Node scene_node = root["scene"];
        ensure_map(scene_node, "scene");
        if (parse_metadata) {
            if (!scene_node.IsDefined()) {
                throw std::runtime_error("scene must be a map");
            }
            out.metadata.id = scene_node["id"].as<std::string>();
            out.metadata.label = scene_node["label"].as<std::string>();
            if (const YAML::Node background = scene_node["background"]) {
                out.metadata.background = parse_vec3(background);
            }
        }

        const YAML::Node includes_node = root["includes"];
        ensure_sequence(includes_node, "includes");
        for (const YAML::Node& include_node : includes_node) {
            if (!include_node.IsScalar()) {
                throw std::runtime_error("include must be a scalar path");
            }
            std::filesystem::path include_path = include_node.as<std::string>();
            if (include_path.is_relative()) {
                include_path = normalized_scene.parent_path() / include_path;
            }
            load_scene_file(include_path.lexically_normal(), false, false, out, texture_ids, material_ids, shape_ids,
                medium_ids, preset_ids, dependency_set, active_files);
        }

        if (scene_node.IsDefined()) {
            parse_textures(scene_node["textures"], normalized_scene.parent_path(), out, texture_ids, dependency_set);
            parse_materials(scene_node["materials"], out.scene_ir, texture_ids, material_ids);
            parse_shapes(scene_node["shapes"], out.scene_ir, shape_ids);
            parse_instances(scene_node["instances"], out.scene_ir, shape_ids, material_ids);
            parse_media(scene_node["media"], out.scene_ir, shape_ids, material_ids, medium_ids);
        }
        parse_imports(root["imports"], normalized_scene.parent_path(), out, dependency_set);
        parse_cpu_presets(root["cpu_presets"], out.metadata.id, out.cpu_presets, preset_ids);
        parse_realtime_section(root["realtime"], out.realtime_preset);

        active_files.erase(normalized_scene.string());
    } catch (const YAML::Exception& ex) {
        active_files.erase(normalized_scene.string());
        throw scene_error(normalized_scene, ex.what());
    } catch (const std::exception& ex) {
        active_files.erase(normalized_scene.string());
        if (has_file_error_prefix(ex.what())) {
            throw;
        }
        throw scene_error(normalized_scene, ex.what());
    }
}

}  // namespace

SceneDefinition load_scene_definition(const std::filesystem::path& scene_file) {
    try {
        SceneDefinition out;
        IdTable texture_ids;
        IdTable material_ids;
        IdTable shape_ids;
        IdTable medium_ids;
        IdTable preset_ids;
        StringSet dependency_set;
        StringSet active_files;
        load_scene_file(scene_file, true, true, out, texture_ids, material_ids, shape_ids, medium_ids, preset_ids,
            dependency_set, active_files);
        out.metadata.supports_cpu_render = !out.cpu_presets.empty();
        out.metadata.supports_realtime = out.realtime_preset.has_value();
        return out;
    } catch (const YAML::Exception& ex) {
        throw scene_error(scene_file, ex.what());
    } catch (const std::exception& ex) {
        if (has_file_error_prefix(ex.what())) {
            throw;
        }
        throw scene_error(scene_file, ex.what());
    }
}

}  // namespace rt::scene
