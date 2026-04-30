#pragma once

#include "realtime/gpu/packed_scene_preparation.h"

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
    int sphere_count_ = 0;
    int quad_count_ = 0;
    int triangle_count_ = 0;
    int medium_count_ = 0;
    int texture_count_ = 0;
    int image_texel_count_ = 0;
    int material_count_ = 0;
};

}  // namespace rt
