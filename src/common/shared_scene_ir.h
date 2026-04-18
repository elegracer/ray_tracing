#pragma once

#include <Eigen/Core>

#include <string>
#include <variant>
#include <vector>

namespace rt {

struct ConstantColorTextureDesc {
    Eigen::Vector3d color;
};

struct CheckerTextureDesc {
    double scale = 1.0;
    int even_texture_index = -1;
    int odd_texture_index = -1;
};

struct ImageTextureDesc {
    std::string path;
};

struct NoiseTextureDesc {
    double scale = 1.0;
};

using TextureDesc =
    std::variant<ConstantColorTextureDesc, CheckerTextureDesc, ImageTextureDesc, NoiseTextureDesc>;

struct DiffuseMaterialDesc {
    int albedo_texture_index = -1;
};

struct MetalMaterialDesc {
    int albedo_texture_index = -1;
    double fuzz = 0.0;
};

struct DielectricMaterialDesc {
    double ior = 1.0;
};

struct EmissiveMaterialDesc {
    int emission_texture_index = -1;
};

struct IsotropicVolumeMaterialDesc {
    int albedo_texture_index = -1;
};

using SharedMaterialDesc = std::variant<DiffuseMaterialDesc, MetalMaterialDesc, DielectricMaterialDesc,
    EmissiveMaterialDesc, IsotropicVolumeMaterialDesc>;

struct SphereShapeDesc {
    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    double radius = 0.0;
};

struct QuadShapeDesc {
    Eigen::Vector3d origin = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_u = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_v = Eigen::Vector3d::Zero();
};

struct BoxShapeDesc {
    Eigen::Vector3d min_corner = Eigen::Vector3d::Zero();
    Eigen::Vector3d max_corner = Eigen::Vector3d::Zero();
};

using ShapeDesc = std::variant<SphereShapeDesc, QuadShapeDesc, BoxShapeDesc>;

struct TransformDesc {
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    Eigen::Vector3d rotation_xyz_degrees = Eigen::Vector3d::Zero();
};

struct SurfaceInstanceDesc {
    int shape_index = -1;
    int material_index = -1;
    TransformDesc transform {};
};

struct MediumInstanceDesc {
    int shape_index = -1;
    int material_index = -1;
    double density = 0.0;
    TransformDesc transform {};
};

struct SceneIR {
    std::vector<TextureDesc> textures;
    std::vector<SharedMaterialDesc> materials;
    std::vector<ShapeDesc> shapes;
    std::vector<SurfaceInstanceDesc> surface_instances;
    std::vector<MediumInstanceDesc> medium_instances;
};

}  // namespace rt
