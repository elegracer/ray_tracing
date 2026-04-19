#include "scene/cpu_scene_adapter.h"

#include "common/common.h"
#include "common/constant_medium.h"
#include "common/hittable.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"
#include "common/texture.h"
#include "common/triangle.h"

#include <Eigen/Geometry>

#include <cmath>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace rt::scene {
namespace {

template <typename T>
const T& checked_index(const std::vector<T>& items, const int index, const std::string& label) {
    if (index < 0 || static_cast<size_t>(index) >= items.size()) {
        throw std::out_of_range(label + " index out of range");
    }
    return items[static_cast<size_t>(index)];
}

Vec3d to_vec3(const Eigen::Vector3d& v) {
    return Vec3d {v.x(), v.y(), v.z()};
}

double extract_y_rotation_degrees(const Eigen::Matrix3d& rotation) {
    constexpr double kTol = 1e-6;
    if ((rotation - Eigen::Matrix3d::Identity()).cwiseAbs().maxCoeff() <= kTol) {
        return 0.0;
    }

    const bool is_supported =
        std::abs(rotation(0, 1)) <= kTol && std::abs(rotation(1, 0)) <= kTol
        && std::abs(rotation(1, 2)) <= kTol && std::abs(rotation(2, 1)) <= kTol
        && std::abs(rotation(1, 1) - 1.0) <= kTol;
    if (!is_supported) {
        throw std::invalid_argument("unsupported transform rotation for CPU adapter");
    }

    const double angle = std::atan2(rotation(0, 2), rotation(0, 0));
    if (std::abs(rotation(2, 0) + std::sin(angle)) > 1e-4
        || std::abs(rotation(2, 2) - std::cos(angle)) > 1e-4) {
        throw std::invalid_argument("unsupported transform rotation for CPU adapter");
    }
    return rad2deg(angle);
}

bool is_identity_transform(const Transform& transform) {
    constexpr double kTol = 1e-9;
    return transform.translation.norm() <= kTol
        && (transform.rotation - Eigen::Matrix3d::Identity()).cwiseAbs().maxCoeff() <= kTol;
}

pro::proxy<Hittable> apply_transform(pro::proxy<Hittable> object, const Transform& transform) {
    const double yaw_deg = extract_y_rotation_degrees(transform.rotation);
    if (std::abs(yaw_deg) > 1e-9) {
        object = pro::make_proxy_shared<Hittable, RotateY>(object, yaw_deg);
    }
    if (transform.translation.norm() > 1e-9) {
        object = pro::make_proxy_shared<Hittable, Translate>(object, to_vec3(transform.translation));
    }
    return object;
}

pro::proxy<Hittable> make_shape_hittable(const ShapeDesc& shape, const pro::proxy<Material>& material) {
    return std::visit(
        [&](const auto& desc) -> pro::proxy<Hittable> {
            using T = std::decay_t<decltype(desc)>;
            if constexpr (std::is_same_v<T, SphereShape>) {
                return pro::make_proxy_shared<Hittable, Sphere>(to_vec3(desc.center), desc.radius, material);
            } else if constexpr (std::is_same_v<T, QuadShape>) {
                return pro::make_proxy_shared<Hittable, Quad>(
                    to_vec3(desc.origin), to_vec3(desc.edge_u), to_vec3(desc.edge_v), material);
            } else if constexpr (std::is_same_v<T, BoxShape>) {
                return box(to_vec3(desc.min_corner), to_vec3(desc.max_corner), material);
            } else if constexpr (std::is_same_v<T, TriangleMeshShape>) {
                HittableList mesh_world;
                for (const Eigen::Vector3i& tri : desc.triangles) {
                    for (int vertex = 0; vertex < 3; ++vertex) {
                        if (tri[vertex] < 0 || tri[vertex] >= static_cast<int>(desc.positions.size())) {
                            throw std::out_of_range("triangle mesh vertex index out of range");
                        }
                    }
                    mesh_world.add(pro::make_proxy_shared<Hittable, Triangle>(
                        to_vec3(desc.positions[tri.x()]),
                        to_vec3(desc.positions[tri.y()]),
                        to_vec3(desc.positions[tri.z()]),
                        material));
                }
                return pro::make_proxy_shared<Hittable, HittableList>(mesh_world);
            } else {
                static_assert(std::is_same_v<T, void>, "unsupported shape type");
            }
        },
        shape);
}

}  // namespace

CpuSceneAdapterResult adapt_to_cpu(const SceneIR& scene) {
    const std::vector<TextureDesc>& texture_descs = scene.textures();
    std::vector<pro::proxy<Texture>> textures;
    textures.reserve(texture_descs.size());
    for (const TextureDesc& texture_desc : texture_descs) {
        const pro::proxy<Texture> texture = std::visit(
            [&](const auto& desc) -> pro::proxy<Texture> {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, ConstantColorTextureDesc>) {
                    return pro::make_proxy_shared<Texture, SolidColor>(to_vec3(desc.color));
                } else if constexpr (std::is_same_v<T, CheckerTextureDesc>) {
                    const pro::proxy<Texture>& even = checked_index(textures, desc.even_texture, "checker even texture");
                    const pro::proxy<Texture>& odd = checked_index(textures, desc.odd_texture, "checker odd texture");
                    return pro::make_proxy_shared<Texture, CheckerTexture>(desc.scale, even, odd);
                } else if constexpr (std::is_same_v<T, ImageTextureDesc>) {
                    return pro::make_proxy_shared<Texture, ImageTexture>(desc.path);
                } else if constexpr (std::is_same_v<T, NoiseTextureDesc>) {
                    return pro::make_proxy_shared<Texture, NoiseTexture>(desc.scale);
                } else {
                    static_assert(std::is_same_v<T, void>, "unsupported texture type");
                }
            },
            texture_desc);
        textures.push_back(texture);
    }

    const std::vector<MaterialDesc>& material_descs = scene.materials();
    std::vector<pro::proxy<Material>> materials;
    materials.reserve(material_descs.size());
    for (const MaterialDesc& material_desc : material_descs) {
        const pro::proxy<Material> material = std::visit(
            [&](const auto& desc) -> pro::proxy<Material> {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, DiffuseMaterial>) {
                    const pro::proxy<Texture>& albedo = checked_index(textures, desc.albedo_texture, "diffuse albedo texture");
                    return pro::make_proxy_shared<Material, Lambertion>(albedo);
                } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                    const TextureDesc& texture_desc = checked_index(texture_descs, desc.albedo_texture, "metal albedo texture");
                    const auto* constant = std::get_if<ConstantColorTextureDesc>(&texture_desc);
                    if (constant == nullptr) {
                        throw std::invalid_argument("metal material requires a constant-color texture");
                    }
                    return pro::make_proxy_shared<Material, Metal>(to_vec3(constant->color), desc.fuzz);
                } else if constexpr (std::is_same_v<T, DielectricMaterial>) {
                    return pro::make_proxy_shared<Material, Dielectric>(desc.ior);
                } else if constexpr (std::is_same_v<T, EmissiveMaterial>) {
                    const pro::proxy<Texture>& emission = checked_index(textures, desc.emission_texture, "emissive texture");
                    return pro::make_proxy_shared<Material, DiffuseLight>(emission);
                } else if constexpr (std::is_same_v<T, IsotropicVolumeMaterial>) {
                    const pro::proxy<Texture>& albedo = checked_index(textures, desc.albedo_texture, "isotropic albedo texture");
                    return pro::make_proxy_shared<Material, Isotropic>(albedo);
                } else {
                    static_assert(std::is_same_v<T, void>, "unsupported material type");
                }
            },
            material_desc);
        materials.push_back(material);
    }

    const std::vector<ShapeDesc>& shape_descs = scene.shapes();
    HittableList world;
    HittableList lights;

    for (const SurfaceInstance& instance : scene.surface_instances()) {
        const ShapeDesc& shape_desc = checked_index(shape_descs, instance.shape_index, "surface shape");
        const MaterialDesc& material_desc = checked_index(material_descs, instance.material_index, "surface material");
        const pro::proxy<Material>& material = checked_index(materials, instance.material_index, "surface material");

        pro::proxy<Hittable> object = make_shape_hittable(shape_desc, material);
        object = apply_transform(object, instance.transform);
        world.add(object);

        if (std::holds_alternative<EmissiveMaterial>(material_desc) && is_identity_transform(instance.transform)) {
            lights.add(object);
        }
    }

    const pro::proxy<Material> empty_material = pro::make_proxy_shared<Material, EmptyMaterial>();
    for (const MediumInstance& medium : scene.media()) {
        if (medium.density <= 0.0) {
            throw std::invalid_argument("medium density must be positive");
        }

        const ShapeDesc& shape_desc = checked_index(shape_descs, medium.shape_index, "medium shape");
        const MaterialDesc& material_desc = checked_index(material_descs, medium.material_index, "medium material");
        const auto* isotropic = std::get_if<IsotropicVolumeMaterial>(&material_desc);
        if (isotropic == nullptr) {
            throw std::invalid_argument("medium requires isotropic volume material");
        }
        if (std::holds_alternative<TriangleMeshShape>(shape_desc)) {
            throw std::invalid_argument("triangle mesh boundaries are unsupported for homogeneous media");
        }

        pro::proxy<Hittable> boundary = make_shape_hittable(shape_desc, empty_material);
        boundary = apply_transform(boundary, medium.transform);

        const pro::proxy<Texture>& albedo =
            checked_index(textures, isotropic->albedo_texture, "isotropic albedo texture");
        world.add(pro::make_proxy_shared<Hittable, ConstantMedium>(boundary, medium.density, albedo));
    }

    CpuSceneAdapterResult result;
    result.world = pro::make_proxy_shared<Hittable, HittableList>(world);
    if (!lights.m_objects.empty()) {
        result.lights = pro::make_proxy_shared<Hittable, HittableList>(lights);
    }
    return result;
}

}  // namespace rt::scene
