#pragma once

#include "realtime/gpu/launch_params.h"
#include "realtime/scene_description.h"

#include <Eigen/Core>

#include <vector>

namespace rt {

struct GpuPreparedScene {
    Eigen::Vector3f background = Eigen::Vector3f::Zero();
    std::vector<PackedSphere> spheres;
    std::vector<PackedQuad> quads;
    std::vector<PackedTriangle> triangles;
    std::vector<PackedMedium> media;
    std::vector<PackedTexture> textures;
    std::vector<Eigen::Vector3f> image_texels;
    std::vector<MaterialSample> materials;
};

GpuPreparedScene prepare_gpu_scene(const PackedScene& scene);

}  // namespace rt
