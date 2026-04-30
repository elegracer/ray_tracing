#include "scene/realtime_scene_adapter.h"

#include "scene/scene_ir_validator.h"

#include <stdexcept>
#include <type_traits>
#include <vector>

namespace rt::scene {
namespace {

Eigen::Vector3d transform_point(const Transform& transform, const Eigen::Vector3d& point) {
    return transform.rotation * point + transform.translation;
}

Eigen::Vector3d transform_vector(const Transform& transform, const Eigen::Vector3d& vector) {
    return transform.rotation * vector;
}

rt::TextureDesc adapt_texture(const scene::TextureDesc& texture) {
    return std::visit(
        [](const auto& desc) -> rt::TextureDesc {
            using T = std::decay_t<decltype(desc)>;
            if constexpr (std::is_same_v<T, ConstantColorTextureDesc>) {
                return rt::ConstantColorTextureDesc {.color = desc.color};
            } else if constexpr (std::is_same_v<T, CheckerTextureDesc>) {
                return rt::CheckerTextureDesc {
                    .scale = desc.scale,
                    .even_texture = desc.even_texture,
                    .odd_texture = desc.odd_texture,
                };
            } else if constexpr (std::is_same_v<T, ImageTextureDesc>) {
                return rt::ImageTextureDesc {.path = desc.path};
            } else if constexpr (std::is_same_v<T, NoiseTextureDesc>) {
                return rt::NoiseTextureDesc {.scale = desc.scale};
            } else {
                static_assert(std::is_same_v<T, void>, "unsupported texture type");
            }
        },
        texture);
}

rt::MaterialDesc adapt_material(const scene::MaterialDesc& material) {
    return std::visit(
        [](const auto& desc) -> rt::MaterialDesc {
            using T = std::decay_t<decltype(desc)>;
            if constexpr (std::is_same_v<T, DiffuseMaterial>) {
                return rt::LambertianMaterial {.albedo_texture = desc.albedo_texture};
            } else if constexpr (std::is_same_v<T, scene::MetalMaterial>) {
                return rt::MetalMaterial {.fuzz = desc.fuzz, .albedo_texture = desc.albedo_texture};
            } else if constexpr (std::is_same_v<T, scene::DielectricMaterial>) {
                return rt::DielectricMaterial {.ior = desc.ior};
            } else if constexpr (std::is_same_v<T, EmissiveMaterial>) {
                return rt::DiffuseLightMaterial {.emission_texture = desc.emission_texture};
            } else if constexpr (std::is_same_v<T, scene::IsotropicVolumeMaterial>) {
                return rt::IsotropicVolumeMaterial {.albedo_texture = desc.albedo_texture};
            } else {
                static_assert(std::is_same_v<T, void>, "unsupported material type");
            }
        },
        material);
}

void add_box_quads(rt::SceneDescription& out, int material_index, const BoxShape& box, const Transform& transform) {
    const Eigen::Vector3d p000 {box.min_corner.x(), box.min_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p001 {box.min_corner.x(), box.min_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p010 {box.min_corner.x(), box.max_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p011 {box.min_corner.x(), box.max_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p100 {box.max_corner.x(), box.min_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p101 {box.max_corner.x(), box.min_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p110 {box.max_corner.x(), box.max_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p111 {box.max_corner.x(), box.max_corner.y(), box.max_corner.z()};

    const auto add_face = [&](const Eigen::Vector3d& origin, const Eigen::Vector3d& edge_u, const Eigen::Vector3d& edge_v) {
        out.add_quad(QuadPrimitive {
            .material_index = material_index,
            .origin = transform_point(transform, origin),
            .edge_u = transform_vector(transform, edge_u),
            .edge_v = transform_vector(transform, edge_v),
            .dynamic = false,
        });
    };

    add_face(p001, p101 - p001, p011 - p001);
    add_face(p100, p000 - p100, p110 - p100);
    add_face(p000, p001 - p000, p010 - p000);
    add_face(p101, p100 - p101, p111 - p101);
    add_face(p010, p110 - p010, p011 - p010);
    add_face(p000, p100 - p000, p001 - p000);
}

}  // namespace

rt::SceneDescription adapt_to_realtime(const SceneIR& scene) {
    validate_scene_ir(scene);

    rt::SceneDescription out;

    for (const scene::TextureDesc& texture : scene.textures()) {
        out.add_texture(adapt_texture(texture));
    }
    for (const scene::MaterialDesc& material : scene.materials()) {
        out.add_material(adapt_material(material));
    }

    const std::vector<ShapeDesc>& shapes = scene.shapes();

    for (const SurfaceInstance& instance : scene.surface_instances()) {
        const ShapeDesc& shape = shapes[static_cast<std::size_t>(instance.shape_index)];
        std::visit(
            [&](const auto& desc) {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, SphereShape>) {
                    out.add_sphere(SpherePrimitive {
                        .material_index = instance.material_index,
                        .center = transform_point(instance.transform, desc.center),
                        .radius = desc.radius,
                        .dynamic = false,
                    });
                } else if constexpr (std::is_same_v<T, QuadShape>) {
                    out.add_quad(QuadPrimitive {
                        .material_index = instance.material_index,
                        .origin = transform_point(instance.transform, desc.origin),
                        .edge_u = transform_vector(instance.transform, desc.edge_u),
                        .edge_v = transform_vector(instance.transform, desc.edge_v),
                        .dynamic = false,
                    });
                } else if constexpr (std::is_same_v<T, BoxShape>) {
                    add_box_quads(out, instance.material_index, desc, instance.transform);
                } else if constexpr (std::is_same_v<T, TriangleMeshShape>) {
                    for (const Eigen::Vector3i& tri : desc.triangles) {
                        out.add_triangle(TrianglePrimitive {
                            .material_index = instance.material_index,
                            .p0 = transform_point(instance.transform, desc.positions[tri.x()]),
                            .p1 = transform_point(instance.transform, desc.positions[tri.y()]),
                            .p2 = transform_point(instance.transform, desc.positions[tri.z()]),
                            .dynamic = false,
                        });
                    }
                } else {
                    static_assert(std::is_same_v<T, void>, "unsupported surface shape");
                }
            },
            shape);
    }

    for (const MediumInstance& medium : scene.media()) {
        const ShapeDesc& shape = shapes[static_cast<std::size_t>(medium.shape_index)];

        rt::HomogeneousMediumPrimitive packed {};
        packed.material_index = medium.material_index;
        packed.density = medium.density;
        packed.translation = medium.transform.translation;
        packed.world_to_local_rotation = medium.transform.rotation.transpose();

        std::visit(
            [&](const auto& desc) {
                using T = std::decay_t<decltype(desc)>;
                if constexpr (std::is_same_v<T, SphereShape>) {
                    packed.boundary_type = 0;
                    packed.local_center_or_min = desc.center;
                    packed.radius = desc.radius;
                } else if constexpr (std::is_same_v<T, BoxShape>) {
                    packed.boundary_type = 1;
                    packed.local_center_or_min = desc.min_corner;
                    packed.local_max = desc.max_corner;
                } else if constexpr (std::is_same_v<T, QuadShape>) {
                    throw std::invalid_argument("quad boundaries are unsupported for homogeneous media");
                } else if constexpr (std::is_same_v<T, TriangleMeshShape>) {
                    throw std::invalid_argument("triangle mesh boundaries are unsupported for homogeneous media");
                } else {
                    static_assert(std::is_same_v<T, void>, "unsupported medium shape");
                }
            },
            shape);
        out.add_medium(packed);
    }

    return out;
}

}  // namespace rt::scene
