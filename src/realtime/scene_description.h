#pragma once

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

struct ImageTextureDesc {
    std::string path;
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

using MaterialDesc = std::variant<LambertianMaterial, MetalMaterial, DielectricMaterial, DiffuseLightMaterial>;

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

struct PackedScene {
    int texture_count = 0;
    int material_count = 0;
    int sphere_count = 0;
    int quad_count = 0;
    std::vector<TextureDesc> textures;
    std::vector<MaterialDesc> materials;
    std::vector<SpherePrimitive> spheres;
    std::vector<QuadPrimitive> quads;
};

class SceneDescription {
   public:
    int add_texture(const TextureDesc& texture);
    int add_material(const MaterialDesc& material);
    void add_sphere(const SpherePrimitive& sphere);
    void add_quad(const QuadPrimitive& quad);
    PackedScene pack() const;

   private:
    std::vector<TextureDesc> textures_;
    std::vector<MaterialDesc> materials_;
    std::vector<SpherePrimitive> spheres_;
    std::vector<QuadPrimitive> quads_;
};

}  // namespace rt
