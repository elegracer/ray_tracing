#pragma once

#include "realtime/camera_rig.h"

#include <Eigen/Core>

#include <cstdint>
#include <vector>
#include <vector_types.h>

namespace rt {

struct PackedSphere;
struct PackedQuad;
struct MaterialSample;

struct DirectionDebugFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct DeviceFrameBuffers {
    float4* beauty = nullptr;
    float4* normal = nullptr;
    float4* albedo = nullptr;
    float* depth = nullptr;
};

struct DeviceSceneView {
    PackedSphere* spheres = nullptr;
    PackedQuad* quads = nullptr;
    MaterialSample* materials = nullptr;
    int sphere_count = 0;
    int quad_count = 0;
    int material_count = 0;
};

struct LaunchParams {
    DeviceFrameBuffers frame {};
    DeviceSceneView scene {};
    PackedCameraRig rig {};
    int camera_index = 0;
    int width = 0;
    int height = 0;
    int samples_per_pixel = 1;
    int max_bounces = 4;
    int rr_start_bounce = 3;
    int mode = 0;
};

struct RadianceFrame {
    int width = 0;
    int height = 0;
    double average_luminance = 0.0;
    std::vector<float> beauty_rgba;
    std::vector<float> normal_rgba;
    std::vector<float> albedo_rgba;
    std::vector<float> depth;
};

struct PackedSphere {
    Eigen::Vector3f center;
    float radius;
    int material_index;
};

struct PackedQuad {
    Eigen::Vector3f origin;
    float pad0 = 0.0f;
    Eigen::Vector3f edge_u;
    float pad1 = 0.0f;
    Eigen::Vector3f edge_v;
    int material_index;
};

struct MaterialSample {
    Eigen::Vector3f albedo = Eigen::Vector3f::Zero();
    Eigen::Vector3f emission = Eigen::Vector3f::Zero();
    float ior = 1.0f;
    float fuzz = 0.0f;
    int type = 0;
};

}  // namespace rt
