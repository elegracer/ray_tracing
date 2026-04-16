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

struct DevicePinhole32Params {
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double k1 = 0.0;
    double k2 = 0.0;
    double k3 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
};

struct DeviceEqui62Lut1DParams {
    int width = 0;
    int height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double radial[6] {};
    double tangential[2] {};
    double lut[1024] {};
    double lut_step = 0.0;
};

struct DeviceActiveCamera {
    int width = 0;
    int height = 0;
    CameraModelType model = CameraModelType::pinhole32;
    double origin[3] {};
    double basis_x[3] {};
    double basis_y[3] {};
    double basis_z[3] {};
    DevicePinhole32Params pinhole {};
    DeviceEqui62Lut1DParams equi {};
};

struct LaunchParams {
    DeviceFrameBuffers frame {};
    DeviceSceneView scene {};
    DeviceActiveCamera active_camera {};
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
