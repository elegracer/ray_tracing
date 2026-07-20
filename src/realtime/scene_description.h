#pragma once

#include "common/analytic_light.h"
#include "common/openpbr_core.h"

#include <Eigen/Core>

#include <string>
#include <variant>
#include <vector>

namespace rt {

struct ConstantColorTextureDesc {
    Eigen::Vector3d color = Eigen::Vector3d::Zero();
};

struct CheckerTextureDesc {
    double scale = 1.0;
    int even_texture = -1;
    int odd_texture = -1;
};

enum class TextureAddressMode {
    constant,
    clamp,
    periodic,
    mirror,
};

enum class TextureFilterType {
    closest,
    linear,
    cubic,
};

struct ImageTextureDesc {
    std::string path;
    TextureAddressMode u_address_mode = TextureAddressMode::clamp;
    TextureAddressMode v_address_mode = TextureAddressMode::clamp;
    TextureFilterType filter_type = TextureFilterType::closest;
};

struct NoiseTextureDesc {
    double scale = 1.0;
};

using TextureDesc =
    std::variant<ConstantColorTextureDesc, CheckerTextureDesc, ImageTextureDesc, NoiseTextureDesc>;

struct LambertianMaterial {
    Eigen::Vector3d albedo = Eigen::Vector3d::Zero();
    int albedo_texture = -1;
};

struct MetalMaterial {
    Eigen::Vector3d albedo = Eigen::Vector3d::Zero();
    double fuzz = 0.0;
    int albedo_texture = -1;
};

struct DielectricMaterial {
    double ior = 1.0;
};

struct DiffuseLightMaterial {
    Eigen::Vector3d emission = Eigen::Vector3d::Zero();
    int emission_texture = -1;
};

struct IsotropicVolumeMaterial {
    int albedo_texture = -1;
};

struct OpenPbrMaterialDesc {
    OpenPbrCompiledMaterial compiled;
};

using MaterialDesc = std::variant<LambertianMaterial, MetalMaterial, DielectricMaterial,
    DiffuseLightMaterial, IsotropicVolumeMaterial, OpenPbrMaterialDesc>;

struct SpherePrimitive {
    int material_index;
    Eigen::Vector3d center;
    double radius;
    bool dynamic;
};

struct QuadPrimitive {
    int material_index;
    Eigen::Vector3d origin;
    Eigen::Vector3d edge_u;
    Eigen::Vector3d edge_v;
    bool dynamic;
};

struct TrianglePrimitive {
    int material_index = -1;
    Eigen::Vector3d p0 = Eigen::Vector3d::Zero();
    Eigen::Vector3d p1 = Eigen::Vector3d::Zero();
    Eigen::Vector3d p2 = Eigen::Vector3d::Zero();
    Eigen::Vector3d n0 = Eigen::Vector3d::Zero();
    Eigen::Vector3d n1 = Eigen::Vector3d::Zero();
    Eigen::Vector3d n2 = Eigen::Vector3d::Zero();
    Eigen::Vector2d uv0 = Eigen::Vector2d::Zero();
    Eigen::Vector2d uv1 = Eigen::Vector2d::Zero();
    Eigen::Vector2d uv2 = Eigen::Vector2d::Zero();
    bool has_vertex_normals = false;
    bool has_texcoords = false;
    bool dynamic = false;
};

struct HomogeneousMediumPrimitive {
    int material_index = -1;
    double density = 0.0;
    int boundary_type = 0;
    Eigen::Vector3d local_center_or_min = Eigen::Vector3d::Zero();
    Eigen::Vector3d local_max = Eigen::Vector3d::Zero();
    double radius = 0.0;
    Eigen::Matrix3d world_to_local_rotation = Eigen::Matrix3d::Identity();
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
};

struct PackedScene {
    int texture_count = 0;
    int material_count = 0;
    int sphere_count = 0;
    int quad_count = 0;
    int triangle_count = 0;
    int medium_count = 0;
    int analytic_light_count = 0;
    Eigen::Vector3d background = Eigen::Vector3d::Zero();
    std::vector<TextureDesc> textures;
    std::vector<MaterialDesc> materials;
    std::vector<SpherePrimitive> spheres;
    std::vector<QuadPrimitive> quads;
    std::vector<TrianglePrimitive> triangles;
    std::vector<HomogeneousMediumPrimitive> media;
    std::vector<AnalyticLightDesc> analytic_lights;
};

class SceneDescription {
public:
    int add_texture(const TextureDesc& texture);
    int add_material(const MaterialDesc& material);
    void add_sphere(const SpherePrimitive& sphere);
    void add_quad(const QuadPrimitive& quad);
    void add_triangle(const TrianglePrimitive& triangle);
    void add_medium(const HomogeneousMediumPrimitive& medium);
    void add_analytic_light(const AnalyticLightDesc& light);
    const std::vector<TrianglePrimitive>& triangles() const;
    PackedScene pack() const;

    Eigen::Vector3d background = Eigen::Vector3d::Zero();

private:
    std::vector<TextureDesc> textures_;
    std::vector<MaterialDesc> materials_;
    std::vector<SpherePrimitive> spheres_;
    std::vector<QuadPrimitive> quads_;
    std::vector<TrianglePrimitive> triangles_;
    std::vector<HomogeneousMediumPrimitive> media_;
    std::vector<AnalyticLightDesc> analytic_lights_;
};

} // namespace rt
