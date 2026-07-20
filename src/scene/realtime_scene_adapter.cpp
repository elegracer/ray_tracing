#include "scene/realtime_scene_adapter.h"

#include "scene/analytic_light_compiler.h"
#include "scene/openpbr_core_adapter.h"
#include "scene/scene_ir_validator.h"

#include <Eigen/LU>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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

void add_box_quads(rt::SceneDescription& out, int material_index, const BoxShape& box,
    const Transform& transform) {
    const Eigen::Vector3d p000 {box.min_corner.x(), box.min_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p001 {box.min_corner.x(), box.min_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p010 {box.min_corner.x(), box.max_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p011 {box.min_corner.x(), box.max_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p100 {box.max_corner.x(), box.min_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p101 {box.max_corner.x(), box.min_corner.y(), box.max_corner.z()};
    const Eigen::Vector3d p110 {box.max_corner.x(), box.max_corner.y(), box.min_corner.z()};
    const Eigen::Vector3d p111 {box.max_corner.x(), box.max_corner.y(), box.max_corner.z()};

    const auto add_face = [&](const Eigen::Vector3d& origin, const Eigen::Vector3d& edge_u,
                              const Eigen::Vector3d& edge_v) {
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

Eigen::Vector3d transform_point(const Eigen::Matrix4d& transform, const Eigen::Vector3d& point) {
    return transform.topLeftCorner<3, 3>() * point + transform.topRightCorner<3, 1>();
}

Eigen::Vector3d transform_normal(const Eigen::Matrix4d& transform, const Eigen::Vector3d& normal) {
    const Eigen::Matrix3d linear = transform.topLeftCorner<3, 3>();
    const Eigen::Vector3d transformed = linear.inverse().transpose() * normal;
    const double length = transformed.norm();
    if (!std::isfinite(length) || length <= 1e-12) {
        throw std::invalid_argument("SceneIR v2 contains a singular or zero-length mesh normal");
    }
    return transformed / length;
}

const ScenePrimvar* find_primvar(const SceneMeshGeometry& mesh, std::string_view preferred_name,
    ScenePrimvarRole role) {
    const auto preferred =
        std::find_if(mesh.primvars.begin(), mesh.primvars.end(), [&](const ScenePrimvar& primvar) {
            return primvar.name == preferred_name && primvar.role == role;
        });
    if (preferred != mesh.primvars.end()) {
        return &*preferred;
    }
    const auto fallback = std::find_if(mesh.primvars.begin(), mesh.primvars.end(),
        [&](const ScenePrimvar& primvar) { return primvar.role == role; });
    return fallback == mesh.primvars.end() ? nullptr : &*fallback;
}

std::size_t primvar_domain_index(const ScenePrimvar& primvar, std::size_t face_index,
    std::size_t corner_index, std::size_t point_index) {
    switch (primvar.interpolation) {
        case ScenePrimvarInterpolation::constant: return 0;
        case ScenePrimvarInterpolation::uniform: return face_index;
        case ScenePrimvarInterpolation::varying:
        case ScenePrimvarInterpolation::vertex: return point_index;
        case ScenePrimvarInterpolation::face_varying: return corner_index;
    }
    throw std::invalid_argument("unknown SceneIR v2 primvar interpolation");
}

std::size_t primvar_value_index(const ScenePrimvar& primvar, std::size_t domain_index) {
    if (primvar.element_size != 1) {
        throw std::invalid_argument("realtime mesh adapter requires scalar primvar elements");
    }
    if (primvar.indices.empty()) {
        return domain_index;
    }
    if (domain_index >= primvar.indices.size() || primvar.indices[domain_index] < 0) {
        throw std::invalid_argument("SceneIR v2 primvar index is out of range");
    }
    return static_cast<std::size_t>(primvar.indices[domain_index]);
}

Eigen::Vector2d sample_vec2_primvar(const ScenePrimvar& primvar, std::size_t face_index,
    std::size_t corner_index, std::size_t point_index) {
    const std::size_t index = primvar_value_index(primvar,
        primvar_domain_index(primvar, face_index, corner_index, point_index));
    return std::visit(
        [&](const auto& values) -> Eigen::Vector2d {
            using Values = std::decay_t<decltype(values)>;
            if constexpr (std::is_same_v<Values, std::vector<Eigen::Vector2f>>
                          || std::is_same_v<Values, std::vector<Eigen::Vector2d>>) {
                if (index >= values.size()) {
                    throw std::invalid_argument("SceneIR v2 vec2 primvar value is out of range");
                }
                return values[index].template cast<double>();
            }
            throw std::invalid_argument(
                "realtime texcoord primvar requires float2 or double2 values");
        },
        primvar.values);
}

Eigen::Vector3d sample_vec3_primvar(const ScenePrimvar& primvar, std::size_t face_index,
    std::size_t corner_index, std::size_t point_index) {
    const std::size_t index = primvar_value_index(primvar,
        primvar_domain_index(primvar, face_index, corner_index, point_index));
    return std::visit(
        [&](const auto& values) -> Eigen::Vector3d {
            using Values = std::decay_t<decltype(values)>;
            if constexpr (std::is_same_v<Values, std::vector<Eigen::Vector3f>>
                          || std::is_same_v<Values, std::vector<Eigen::Vector3d>>) {
                if (index >= values.size()) {
                    throw std::invalid_argument("SceneIR v2 vec3 primvar value is out of range");
                }
                return values[index].template cast<double>();
            }
            throw std::invalid_argument(
                "realtime normal primvar requires float3 or double3 values");
        },
        primvar.values);
}

std::string resolved_texture_path(const ScenePrim& prim) {
    for (const SceneAssetReference& asset : prim.asset_references) {
        if (!asset.resolved_path.empty()) {
            return asset.resolved_path;
        }
        if (!asset.evaluated_path.empty()) {
            return asset.evaluated_path;
        }
        if (!asset.authored_path.empty()) {
            return asset.authored_path;
        }
    }
    throw std::invalid_argument("SceneIR v2 image texture has no resolved asset: " + prim.path);
}

rt::TextureDesc adapt_v2_texture(const ScenePrim& prim,
    const std::unordered_map<std::string, int>& texture_indices) {
    const SceneTexture& texture = *prim.texture;
    switch (texture.node) {
        case SceneTextureNode::constant_color:
            return rt::ConstantColorTextureDesc {.color = texture.value};
        case SceneTextureNode::checkerboard: {
            const auto even = texture_indices.find(texture.even_texture_path);
            const auto odd = texture_indices.find(texture.odd_texture_path);
            if (even == texture_indices.end() || odd == texture_indices.end()) {
                throw std::invalid_argument("SceneIR v2 checker texture input is unresolved");
            }
            return rt::CheckerTextureDesc {
                .scale = texture.scale,
                .even_texture = even->second,
                .odd_texture = odd->second,
            };
        }
        case SceneTextureNode::image: {
            const auto address_mode = [](SceneTextureAddressMode mode) {
                switch (mode) {
                    case SceneTextureAddressMode::constant: return rt::TextureAddressMode::constant;
                    case SceneTextureAddressMode::clamp: return rt::TextureAddressMode::clamp;
                    case SceneTextureAddressMode::periodic: return rt::TextureAddressMode::periodic;
                    case SceneTextureAddressMode::mirror: return rt::TextureAddressMode::mirror;
                }
                throw std::invalid_argument("unknown SceneIR v2 texture address mode");
            };
            const auto filter_type = [](SceneTextureFilterType filter) {
                switch (filter) {
                    case SceneTextureFilterType::closest: return rt::TextureFilterType::closest;
                    case SceneTextureFilterType::linear: return rt::TextureFilterType::linear;
                    case SceneTextureFilterType::cubic: return rt::TextureFilterType::cubic;
                }
                throw std::invalid_argument("unknown SceneIR v2 texture filter type");
            };
            return rt::ImageTextureDesc {
                .path = resolved_texture_path(prim),
                .u_address_mode = address_mode(texture.u_address_mode),
                .v_address_mode = address_mode(texture.v_address_mode),
                .filter_type = filter_type(texture.filter_type),
            };
        }
        case SceneTextureNode::noise3d: return rt::NoiseTextureDesc {.scale = texture.scale};
    }
    throw std::invalid_argument("unknown SceneIR v2 texture node");
}

int resolve_material_index(const std::unordered_map<std::string, int>& material_indices,
    std::string_view path) {
    const auto material = material_indices.find(std::string {path});
    if (material == material_indices.end()) {
        throw std::invalid_argument(
            "SceneIR v2 surface material is not realtime-compatible: " + std::string {path});
    }
    return material->second;
}

std::vector<int> face_material_indices(const SceneMeshGeometry& mesh, int fallback,
    const std::unordered_map<std::string, int>& material_indices) {
    std::vector<int> result(mesh.face_vertex_counts.size(), fallback);
    for (const SceneMaterialSubset& subset : mesh.material_subsets) {
        const int material = resolve_material_index(material_indices, subset.material_path);
        for (const std::int32_t face : subset.face_indices) {
            if (face < 0 || static_cast<std::size_t>(face) >= result.size()) {
                throw std::invalid_argument("SceneIR v2 material subset face is out of range");
            }
            result[static_cast<std::size_t>(face)] = material;
        }
    }
    return result;
}

void add_v2_mesh(rt::SceneDescription& out, const SceneMeshGeometry& mesh,
    const Eigen::Matrix4d& world, int fallback_material,
    const std::unordered_map<std::string, int>& material_indices) {
    const ScenePrimvar* normals = find_primvar(mesh, "normals", ScenePrimvarRole::normal);
    const ScenePrimvar* texcoords = find_primvar(mesh, "st", ScenePrimvarRole::texcoord);
    const std::vector<int> materials =
        face_material_indices(mesh, fallback_material, material_indices);
    const bool reverse_winding = (mesh.orientation == SceneMeshOrientation::left_handed)
                                 != (world.topLeftCorner<3, 3>().determinant() < 0.0);

    std::size_t face_offset = 0;
    for (std::size_t face = 0; face < mesh.face_vertex_counts.size(); ++face) {
        const std::size_t count = static_cast<std::size_t>(mesh.face_vertex_counts[face]);
        for (std::size_t corner = 1; corner + 1 < count; ++corner) {
            std::array<std::size_t, 3> corners {face_offset, face_offset + corner,
                face_offset + corner + 1};
            if (reverse_winding) {
                std::swap(corners[1], corners[2]);
            }
            std::array<std::size_t, 3> points {};
            for (std::size_t i = 0; i < points.size(); ++i) {
                const std::int32_t point = mesh.face_vertex_indices[corners[i]];
                if (point < 0 || static_cast<std::size_t>(point) >= mesh.points.size()) {
                    throw std::invalid_argument("SceneIR v2 mesh point index is out of range");
                }
                points[i] = static_cast<std::size_t>(point);
            }

            rt::TrianglePrimitive triangle {
                .material_index = materials[face],
                .p0 = transform_point(world, mesh.points[points[0]]),
                .p1 = transform_point(world, mesh.points[points[1]]),
                .p2 = transform_point(world, mesh.points[points[2]]),
            };
            if (normals != nullptr) {
                triangle.n0 = transform_normal(world,
                    sample_vec3_primvar(*normals, face, corners[0], points[0]));
                triangle.n1 = transform_normal(world,
                    sample_vec3_primvar(*normals, face, corners[1], points[1]));
                triangle.n2 = transform_normal(world,
                    sample_vec3_primvar(*normals, face, corners[2], points[2]));
                triangle.has_vertex_normals = true;
            }
            if (texcoords != nullptr) {
                triangle.uv0 = sample_vec2_primvar(*texcoords, face, corners[0], points[0]);
                triangle.uv1 = sample_vec2_primvar(*texcoords, face, corners[1], points[1]);
                triangle.uv2 = sample_vec2_primvar(*texcoords, face, corners[2], points[2]);
                triangle.has_texcoords = true;
            }
            out.add_triangle(triangle);
        }
        face_offset += count;
    }
}

void add_v2_sphere(rt::SceneDescription& out, const SceneSphereGeometry& sphere,
    const Eigen::Matrix4d& world, int material_index) {
    const Eigen::Matrix3d linear = world.topLeftCorner<3, 3>();
    const double sx = linear.col(0).norm();
    const double sy = linear.col(1).norm();
    const double sz = linear.col(2).norm();
    const double scale = (sx + sy + sz) / 3.0;
    const double tolerance = 1e-6 * std::max(1.0, scale);
    if (std::abs(sx - scale) > tolerance || std::abs(sy - scale) > tolerance
        || std::abs(sz - scale) > tolerance) {
        throw std::invalid_argument("realtime sphere adapter requires a uniform SceneIR v2 scale");
    }
    out.add_sphere(rt::SpherePrimitive {
        .material_index = material_index,
        .center = transform_point(world, sphere.center),
        .radius = sphere.radius * scale,
        .dynamic = false,
    });
}

} // namespace

rt::SceneDescription adapt_to_realtime_impl(const SceneIR& scene,
    const std::vector<std::optional<OpenPbrCompiledMaterial>>* openpbr_materials) {
    validate_scene_ir(scene);

    rt::SceneDescription out;

    for (const scene::TextureDesc& texture : scene.textures()) {
        out.add_texture(adapt_texture(texture));
    }
    for (std::size_t material_index = 0; material_index < scene.materials().size();
         ++material_index) {
        const scene::MaterialDesc& material = scene.materials()[material_index];
        if (openpbr_materials != nullptr && (*openpbr_materials)[material_index]) {
            if (std::holds_alternative<scene::IsotropicVolumeMaterial>(material)) {
                throw std::invalid_argument(
                    "SceneIR v2 surface material cannot replace a compatibility volume");
            }
            out.add_material(rt::OpenPbrMaterialDesc {
                .compiled = *(*openpbr_materials)[material_index],
            });
            continue;
        }
        if (openpbr_materials != nullptr
            && !std::holds_alternative<scene::IsotropicVolumeMaterial>(material)) {
            throw std::invalid_argument("SceneIR v2 compatibility surface must compile to OpenPBR");
        }
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
                    throw std::invalid_argument(
                        "quad boundaries are unsupported for homogeneous media");
                } else if constexpr (std::is_same_v<T, TriangleMeshShape>) {
                    throw std::invalid_argument(
                        "triangle mesh boundaries are unsupported for homogeneous media");
                } else {
                    static_assert(std::is_same_v<T, void>, "unsupported medium shape");
                }
            },
            shape);
        out.add_medium(packed);
    }

    return out;
}

rt::SceneDescription adapt_to_realtime(const SceneIR& scene) {
    return adapt_to_realtime_impl(scene, nullptr);
}

rt::SceneDescription adapt_to_realtime_openpbr(const SceneIR& compatibility_scene,
    const SceneIRv2& scene_v2) {
    const auto materials = compile_openpbr_core_material_table(scene_v2,
        compatibility_scene.materials().size(), compatibility_scene.textures().size());
    rt::SceneDescription result = adapt_to_realtime_impl(compatibility_scene, &materials);
    for (const AnalyticLightDesc& light : compile_analytic_lights(scene_v2)) {
        result.add_analytic_light(light);
    }
    return result;
}

rt::SceneDescription adapt_scene_ir_v2_to_realtime(const SceneIRv2& scene_v2) {
    require_valid_scene_ir_v2(scene_v2);
    rt::SceneDescription result;

    std::unordered_map<std::string, int> texture_indices;
    for (const ScenePrim& prim : scene_v2.prims()) {
        if (prim.kind == ScenePrimKind::texture && prim.texture) {
            texture_indices.emplace(prim.path, static_cast<int>(texture_indices.size()));
        }
    }
    for (const ScenePrim& prim : scene_v2.prims()) {
        if (prim.kind != ScenePrimKind::texture || !prim.texture) {
            continue;
        }
        const int actual = result.add_texture(adapt_v2_texture(prim, texture_indices));
        if (actual != texture_indices.at(prim.path)) {
            throw std::logic_error("SceneIR v2 realtime texture table lost stable ordering");
        }
    }

    std::unordered_map<std::string, int> material_indices;
    for (const ScenePrim& prim : scene_v2.prims()) {
        if (prim.kind != ScenePrimKind::material || !prim.material) {
            continue;
        }
        const auto* surface = std::get_if<SceneOpenPbrSurface>(&*prim.material);
        if (surface == nullptr) {
            continue;
        }
        const int index = result.add_material(rt::OpenPbrMaterialDesc {
            .compiled = compile_openpbr_core_material(*surface, scene_v2, texture_indices),
        });
        material_indices.emplace(prim.path, index);
    }

    const double time_code = scene_v2.stage_metadata().start_time_code.value_or(0.0);
    for (const ScenePrim& prim : scene_v2.prims()) {
        if (prim.kind != ScenePrimKind::surface || !compute_scene_visibility(scene_v2, prim.path)) {
            continue;
        }
        const ScenePurpose purpose = compute_scene_purpose(scene_v2, prim.path);
        if (purpose == ScenePurpose::proxy || purpose == ScenePurpose::guide) {
            continue;
        }
        const ScenePrim* prototype =
            prim.prototype_path.empty() ? &prim : scene_v2.find_prim(prim.prototype_path);
        if (prototype == nullptr || !prototype->geometry) {
            throw std::invalid_argument(
                "SceneIR v2 surface has no resolved geometry: " + prim.path);
        }
        const int material = resolve_material_index(material_indices, prim.material_path);
        const Eigen::Matrix4d world = compute_scene_world_transform(scene_v2, prim.path, time_code);
        std::visit(
            [&](const auto& geometry) {
                using T = std::decay_t<decltype(geometry)>;
                if constexpr (std::is_same_v<T, SceneSphereGeometry>) {
                    add_v2_sphere(result, geometry, world, material);
                } else if constexpr (std::is_same_v<T, SceneMeshGeometry>) {
                    add_v2_mesh(result, geometry, world, material, material_indices);
                }
            },
            *prototype->geometry);
    }

    for (const AnalyticLightDesc& light : compile_analytic_lights(scene_v2)) {
        result.add_analytic_light(light);
    }
    return result;
}

} // namespace rt::scene
