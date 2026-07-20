#pragma once

#include "realtime/gpu/gpu_scene_acceleration.h"
#include "realtime/gpu/packed_scene_preparation.h"

#include <cstddef>

namespace rt {

class DeviceSceneBuffers {
public:
    DeviceSceneBuffers() = default;
    ~DeviceSceneBuffers();

    DeviceSceneBuffers(const DeviceSceneBuffers&) = delete;
    DeviceSceneBuffers& operator=(const DeviceSceneBuffers&) = delete;
    DeviceSceneBuffers(DeviceSceneBuffers&&) = delete;
    DeviceSceneBuffers& operator=(DeviceSceneBuffers&&) = delete;

    void upload(const GpuPreparedScene& scene);
    void upload(const GpuPreparedScene& scene, const GpuSceneAcceleration& acceleration,
        AccelerationUpdateKind update_kind);
    void reset();
    DeviceSceneView view() const;

private:
    PackedSphere* spheres_ = nullptr;
    PackedQuad* quads_ = nullptr;
    PackedTriangle* triangles_ = nullptr;
    PackedMedium* media_ = nullptr;
    PackedTexture* textures_ = nullptr;
    Eigen::Vector3f* image_texels_ = nullptr;
    MaterialSample* materials_ = nullptr;
    OpenPbrCompiledMaterial* openpbr_materials_ = nullptr;
    PackedLight* lights_ = nullptr;
    PackedAnalyticLight* analytic_lights_ = nullptr;
    PackedBvhNode* acceleration_nodes_ = nullptr;
    PackedPrimitiveRef* acceleration_references_ = nullptr;
    std::size_t sphere_capacity_ = 0;
    std::size_t quad_capacity_ = 0;
    std::size_t triangle_capacity_ = 0;
    std::size_t medium_capacity_ = 0;
    std::size_t texture_capacity_ = 0;
    std::size_t image_texel_capacity_ = 0;
    std::size_t material_capacity_ = 0;
    std::size_t openpbr_material_capacity_ = 0;
    std::size_t light_capacity_ = 0;
    std::size_t analytic_light_capacity_ = 0;
    std::size_t acceleration_node_capacity_ = 0;
    std::size_t acceleration_reference_capacity_ = 0;
    int sphere_count_ = 0;
    int quad_count_ = 0;
    int triangle_count_ = 0;
    int medium_count_ = 0;
    int texture_count_ = 0;
    int image_texel_count_ = 0;
    int material_count_ = 0;
    int openpbr_material_count_ = 0;
    int light_count_ = 0;
    int analytic_light_count_ = 0;
    int acceleration_node_count_ = 0;
    int acceleration_reference_count_ = 0;
};

} // namespace rt
