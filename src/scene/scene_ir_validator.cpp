#include "scene/scene_ir_validator.h"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace rt::scene {
namespace {

template <typename T>
void validate_index(const std::vector<T>& items, int index, const std::string& label) {
    if (index < 0 || static_cast<std::size_t>(index) >= items.size()) {
        throw std::out_of_range(label + " index out of range");
    }
}

void validate_texture_refs(const std::vector<TextureDesc>& textures) {
    for (const TextureDesc& texture : textures) {
        std::visit(
            [&](const auto& desc) {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, CheckerTextureDesc>) {
                    validate_index(textures, desc.even_texture, "checker even texture");
                    validate_index(textures, desc.odd_texture, "checker odd texture");
                }
            },
            texture);
    }
}

void validate_material_refs(const std::vector<MaterialDesc>& materials, const std::vector<TextureDesc>& textures) {
    for (const MaterialDesc& material : materials) {
        std::visit(
            [&](const auto& desc) {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, DiffuseMaterial>) {
                    validate_index(textures, desc.albedo_texture, "diffuse albedo texture");
                } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                    validate_index(textures, desc.albedo_texture, "metal albedo texture");
                } else if constexpr (std::is_same_v<T, EmissiveMaterial>) {
                    validate_index(textures, desc.emission_texture, "emissive texture");
                } else if constexpr (std::is_same_v<T, IsotropicVolumeMaterial>) {
                    validate_index(textures, desc.albedo_texture, "isotropic albedo texture");
                }
            },
            material);
    }
}

void validate_triangle_mesh(const TriangleMeshShape& mesh) {
    for (const Eigen::Vector3i& tri : mesh.triangles) {
        for (int vertex = 0; vertex < 3; ++vertex) {
            if (tri[vertex] < 0 || tri[vertex] >= static_cast<int>(mesh.positions.size())) {
                throw std::out_of_range("triangle mesh vertex index out of range");
            }
        }
    }
}

void validate_shapes(const std::vector<ShapeDesc>& shapes) {
    for (const ShapeDesc& shape : shapes) {
        if (const auto* mesh = std::get_if<TriangleMeshShape>(&shape); mesh != nullptr) {
            validate_triangle_mesh(*mesh);
        }
    }
}

}  // namespace

void validate_scene_ir(const SceneIR& scene) {
    const std::vector<TextureDesc>& textures = scene.textures();
    const std::vector<MaterialDesc>& materials = scene.materials();
    const std::vector<ShapeDesc>& shapes = scene.shapes();

    validate_texture_refs(textures);
    validate_material_refs(materials, textures);
    validate_shapes(shapes);

    for (const SurfaceInstance& instance : scene.surface_instances()) {
        validate_index(shapes, instance.shape_index, "surface shape");
        validate_index(materials, instance.material_index, "surface material");
    }

    for (const MediumInstance& medium : scene.media()) {
        if (medium.density <= 0.0) {
            throw std::invalid_argument("medium density must be positive");
        }
        validate_index(shapes, medium.shape_index, "medium shape");
        validate_index(materials, medium.material_index, "medium material");
        const MaterialDesc& material = materials[static_cast<std::size_t>(medium.material_index)];
        if (!std::holds_alternative<IsotropicVolumeMaterial>(material)) {
            throw std::invalid_argument("medium requires isotropic volume material");
        }
    }
}

}  // namespace rt::scene
