#pragma once

#include <Eigen/Core>

#include <string>
#include <variant>
#include <vector>

namespace rt::scene {

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

struct DiffuseMaterial {
    int albedo_texture = -1;
};

struct MetalMaterial {
    int albedo_texture = -1;
    double fuzz = 0.0;
};

struct DielectricMaterial {
    double ior = 1.0;
};

struct EmissiveMaterial {
    int emission_texture = -1;
};

struct IsotropicVolumeMaterial {
    int albedo_texture = -1;
};

using MaterialDesc =
    std::variant<DiffuseMaterial, MetalMaterial, DielectricMaterial, EmissiveMaterial, IsotropicVolumeMaterial>;

struct SphereShape {
    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    double radius = 1.0;
};

struct QuadShape {
    Eigen::Vector3d origin = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_u = Eigen::Vector3d::Zero();
    Eigen::Vector3d edge_v = Eigen::Vector3d::Zero();
};

struct BoxShape {
    Eigen::Vector3d min_corner = Eigen::Vector3d::Zero();
    Eigen::Vector3d max_corner = Eigen::Vector3d::Zero();
};

struct TriangleMeshShape {
    std::vector<Eigen::Vector3d> positions;
    std::vector<Eigen::Vector3i> triangles;
};

using ShapeDesc = std::variant<SphereShape, QuadShape, BoxShape, TriangleMeshShape>;

struct Transform {
    Eigen::Vector3d translation = Eigen::Vector3d::Zero();
    Eigen::Matrix3d rotation = Eigen::Matrix3d::Identity();

    static Transform identity();
};

struct SurfaceInstance {
    int shape_index = -1;
    int material_index = -1;
    Transform transform = Transform::identity();
};

struct MediumInstance {
    int shape_index = -1;
    int material_index = -1;
    double density = 0.0;
    Transform transform = Transform::identity();
};

class SceneIR {
   public:
    int add_texture(const TextureDesc& texture);
    int add_material(const MaterialDesc& material);
    int add_shape(const ShapeDesc& shape);
    void add_instance(const SurfaceInstance& instance);
    void add_medium(const MediumInstance& medium);

    const std::vector<TextureDesc>& textures() const;
    const std::vector<MaterialDesc>& materials() const;
    const std::vector<ShapeDesc>& shapes() const;
    const std::vector<SurfaceInstance>& surface_instances() const;
    const std::vector<MediumInstance>& media() const;

   private:
    std::vector<TextureDesc> textures_;
    std::vector<MaterialDesc> materials_;
    std::vector<ShapeDesc> shapes_;
    std::vector<SurfaceInstance> surface_instances_;
    std::vector<MediumInstance> media_;
};

}  // namespace rt::scene
