#pragma once

#include <Eigen/Core>

#include <variant>
#include <vector>

namespace rt {

struct LambertianMaterial {
    Eigen::Vector3d albedo;
};

struct MetalMaterial {
    Eigen::Vector3d albedo;
    double fuzz;
};

struct DielectricMaterial {
    double ior;
};

struct DiffuseLightMaterial {
    Eigen::Vector3d emission;
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
    int material_count = 0;
    int sphere_count = 0;
    int quad_count = 0;
    std::vector<MaterialDesc> materials;
    std::vector<SpherePrimitive> spheres;
    std::vector<QuadPrimitive> quads;
};

class SceneDescription {
   public:
    int add_material(const MaterialDesc& material);
    void add_sphere(const SpherePrimitive& sphere);
    void add_quad(const QuadPrimitive& quad);
    PackedScene pack() const;

   private:
    std::vector<MaterialDesc> materials_;
    std::vector<SpherePrimitive> spheres_;
    std::vector<QuadPrimitive> quads_;
};

}  // namespace rt
