#include "scene/openpbr_core_adapter.h"

namespace rt::scene {
namespace {

OpenPbrVec3 to_openpbr_vec3(const Eigen::Vector3d& value) {
    return {static_cast<float>(value.x()), static_cast<float>(value.y()),
        static_cast<float>(value.z())};
}

} // namespace

OpenPbrCoreMaterial compile_openpbr_core_material(const SceneOpenPbrSurface& material) {
    return OpenPbrCoreMaterial {
        .base_weight = static_cast<float>(material.base_weight),
        .base_color = to_openpbr_vec3(material.base_color),
        .base_diffuse_roughness = static_cast<float>(material.base_diffuse_roughness),
        .base_metalness = static_cast<float>(material.base_metalness),
        .specular_weight = static_cast<float>(material.specular_weight),
        .specular_color = to_openpbr_vec3(material.specular_color),
        .specular_roughness = static_cast<float>(material.specular_roughness),
        .specular_ior = static_cast<float>(material.specular_ior),
        .specular_roughness_anisotropy = static_cast<float>(material.specular_roughness_anisotropy),
        .transmission_weight = static_cast<float>(material.transmission_weight),
        .transmission_color = to_openpbr_vec3(material.transmission_color),
        .transmission_depth = static_cast<float>(material.transmission_depth),
        .emission_luminance = static_cast<float>(material.emission_luminance),
        .emission_color = to_openpbr_vec3(material.emission_color),
        .geometry_opacity = static_cast<float>(material.geometry_opacity),
        .geometry_thin_walled = material.geometry_thin_walled ? 1 : 0,
    };
}

} // namespace rt::scene
