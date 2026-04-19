#include "realtime/scene_description.h"

#include <type_traits>

namespace rt {

namespace {

int ensure_constant_texture(std::vector<TextureDesc>& textures, int texture_index, const Eigen::Vector3d& color) {
    if (texture_index >= 0) {
        return texture_index;
    }
    textures.push_back(ConstantColorTextureDesc {.color = color});
    return static_cast<int>(textures.size()) - 1;
}

}  // namespace

int SceneDescription::add_texture(const TextureDesc& texture) {
    textures_.push_back(texture);
    return static_cast<int>(textures_.size()) - 1;
}

int SceneDescription::add_material(const MaterialDesc& material) {
    MaterialDesc normalized = material;
    std::visit(
        [&](auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, LambertianMaterial>) {
                value.albedo_texture = ensure_constant_texture(textures_, value.albedo_texture, value.albedo);
            } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                value.albedo_texture = ensure_constant_texture(textures_, value.albedo_texture, value.albedo);
            } else if constexpr (std::is_same_v<T, DiffuseLightMaterial>) {
                value.emission_texture =
                    ensure_constant_texture(textures_, value.emission_texture, value.emission);
            }
        },
        normalized);
    materials_.push_back(normalized);
    return static_cast<int>(materials_.size()) - 1;
}

void SceneDescription::add_sphere(const SpherePrimitive& sphere) {
    spheres_.push_back(sphere);
}

void SceneDescription::add_quad(const QuadPrimitive& quad) {
    quads_.push_back(quad);
}

void SceneDescription::add_triangle(const TrianglePrimitive& triangle) {
    triangles_.push_back(triangle);
}

void SceneDescription::add_medium(const HomogeneousMediumPrimitive& medium) {
    media_.push_back(medium);
}

const std::vector<TrianglePrimitive>& SceneDescription::triangles() const {
    return triangles_;
}

PackedScene SceneDescription::pack() const {
    return PackedScene {
        .texture_count = static_cast<int>(textures_.size()),
        .material_count = static_cast<int>(materials_.size()),
        .sphere_count = static_cast<int>(spheres_.size()),
        .quad_count = static_cast<int>(quads_.size()),
        .triangle_count = static_cast<int>(triangles_.size()),
        .medium_count = static_cast<int>(media_.size()),
        .background = background,
        .textures = textures_,
        .materials = materials_,
        .spheres = spheres_,
        .quads = quads_,
        .triangles = triangles_,
        .media = media_,
    };
}

}  // namespace rt
