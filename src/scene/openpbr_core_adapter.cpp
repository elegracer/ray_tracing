#include "scene/openpbr_core_adapter.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rt::scene {
namespace {

OpenPbrVec3 to_openpbr_vec3(const Eigen::Vector3d& value) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
        static_cast<float>(value.z())};
}

void require_production_core_support(const SceneOpenPbrSurface& material) {
    if (material.displacement.type != SceneMaterialXDisplacementType::none) {
        throw std::invalid_argument(
            "OpenPBR production core does not yet support MaterialX displacement");
    }
    if (material.subsurface_weight > 0.0 || material.fuzz_weight > 0.0 || material.coat_weight > 0.0
        || material.thin_film_weight > 0.0) {
        throw std::invalid_argument(
            "OpenPBR production core does not yet support active advanced lobes");
    }
    if (material.transmission_scatter.squaredNorm() > 0.0
        || material.transmission_scatter_anisotropy != 0.0
        || material.transmission_dispersion_scale > 0.0) {
        throw std::invalid_argument(
            "OpenPBR production core does not yet support scattering or dispersion");
    }
    if (material.geometry_normal_default_geomprop != "Nworld"
        || material.geometry_tangent_default_geomprop != "Tworld") {
        throw std::invalid_argument(
            "OpenPBR production core requires the default geometry normal and tangent inputs");
    }
}

OpenPbrSourceColorSpace compile_source_color_space(SceneColorSpace color_space) {
    switch (color_space) {
        case SceneColorSpace::raw: return OpenPbrSourceColorSpace::raw;
        case SceneColorSpace::linear_srgb: return OpenPbrSourceColorSpace::linear_srgb;
        case SceneColorSpace::srgb_texture: return OpenPbrSourceColorSpace::srgb_texture;
        case SceneColorSpace::acescg:
            throw std::invalid_argument(
                "OpenPBR production core does not yet support ACEScg source conversion");
    }
    throw std::invalid_argument("OpenPBR production core received an unknown source color space");
}

OpenPbrColorTextureBinding* find_supported_color_binding(OpenPbrColorTextureBindings& bindings,
    std::string_view input_name) {
    if (input_name == "base_color") {
        return &bindings.base_color;
    }
    if (input_name == "specular_color") {
        return &bindings.specular_color;
    }
    if (input_name == "transmission_color") {
        return &bindings.transmission_color;
    }
    if (input_name == "emission_color") {
        return &bindings.emission_color;
    }
    return nullptr;
}

void compile_color_connections(OpenPbrCompiledMaterial& compiled,
    const SceneOpenPbrSurface& material, const SceneIRv2& scene,
    std::size_t compatibility_texture_count) {
    for (const SceneMaterialConnection& connection : material.connections) {
        OpenPbrColorTextureBinding* binding =
            find_supported_color_binding(compiled.color_textures, connection.input_name);
        if (binding == nullptr || connection.input_type != SceneMaterialValueType::color3
            || connection.channel != SceneTextureChannel::rgb) {
            throw std::invalid_argument(
                "OpenPBR production core supports RGB connections only for base_color, "
                "specular_color, transmission_color, and emission_color");
        }

        const ScenePrim* texture_prim = scene.find_prim(connection.texture_path);
        if (texture_prim == nullptr || !texture_prim->texture) {
            throw std::invalid_argument(
                "OpenPBR connected input does not resolve to a SceneIR v2 texture");
        }
        if (!texture_prim->compatibility_source_index) {
            throw std::invalid_argument(
                "OpenPBR connected texture requires a compatibility texture index");
        }
        const std::size_t texture_index = *texture_prim->compatibility_source_index;
        if (texture_index >= compatibility_texture_count) {
            throw std::invalid_argument(
                "OpenPBR connected texture compatibility index is out of range");
        }
        binding->texture_index = static_cast<int>(texture_index);
        binding->source_color_space =
            compile_source_color_space(texture_prim->texture->color_space);
    }
}

} // namespace

OpenPbrCompiledMaterial compile_openpbr_core_material(const SceneOpenPbrSurface& material) {
    require_production_core_support(material);
    if (!material.connections.empty()) {
        throw std::invalid_argument(
            "connected OpenPBR inputs require a SceneIR v2 texture compilation context");
    }
    return OpenPbrCompiledMaterial {
        .parameters =
            OpenPbrCoreMaterial {
                .base_weight = static_cast<float>(material.base_weight),
                .base_color = to_openpbr_vec3(material.base_color),
                .base_diffuse_roughness = static_cast<float>(material.base_diffuse_roughness),
                .base_metalness = static_cast<float>(material.base_metalness),
                .specular_weight = static_cast<float>(material.specular_weight),
                .specular_color = to_openpbr_vec3(material.specular_color),
                .specular_roughness = static_cast<float>(material.specular_roughness),
                .specular_ior = static_cast<float>(material.specular_ior),
                .specular_roughness_anisotropy =
                    static_cast<float>(material.specular_roughness_anisotropy),
                .transmission_weight = static_cast<float>(material.transmission_weight),
                .transmission_color = to_openpbr_vec3(material.transmission_color),
                .transmission_depth = static_cast<float>(material.transmission_depth),
                .emission_luminance = static_cast<float>(material.emission_luminance),
                .emission_color = to_openpbr_vec3(material.emission_color),
                .geometry_opacity = static_cast<float>(material.geometry_opacity),
                .geometry_thin_walled = material.geometry_thin_walled ? 1 : 0,
            },
    };
}

std::vector<std::optional<OpenPbrCompiledMaterial>> compile_openpbr_core_material_table(
    const SceneIRv2& scene, std::size_t compatibility_material_count,
    std::size_t compatibility_texture_count) {
    require_valid_scene_ir_v2(scene);

    std::vector<std::optional<OpenPbrCompiledMaterial>> table(compatibility_material_count);
    std::vector<bool> seen(compatibility_material_count, false);
    for (const ScenePrim& prim : scene.prims()) {
        if (prim.kind != ScenePrimKind::material || !prim.compatibility_source_index) {
            continue;
        }
        const std::size_t index = *prim.compatibility_source_index;
        if (index >= compatibility_material_count) {
            throw std::invalid_argument("SceneIR v2 material compatibility index is out of range");
        }
        if (seen[index]) {
            throw std::invalid_argument("SceneIR v2 material compatibility index is duplicated");
        }
        if (!prim.material) {
            throw std::invalid_argument("SceneIR v2 material prim has no material payload");
        }

        seen[index] = true;
        if (const auto* surface = std::get_if<SceneOpenPbrSurface>(&*prim.material)) {
            SceneOpenPbrSurface constants = *surface;
            constants.connections.clear();
            OpenPbrCompiledMaterial compiled = compile_openpbr_core_material(constants);
            compile_color_connections(compiled, *surface, scene, compatibility_texture_count);
            table[index] = compiled;
        }
    }

    for (std::size_t index = 0; index < seen.size(); ++index) {
        if (!seen[index]) {
            throw std::invalid_argument("SceneIR v2 is missing a compatibility material for index "
                                        + std::to_string(index));
        }
    }
    return table;
}

} // namespace rt::scene
