#pragma once

#include <Eigen/Core>

#include <cstdint>
#include <vector>

namespace rt {

struct DirectionDebugFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct LaunchParams {
    std::uint8_t* output_rgba = nullptr;
    int camera_index = 0;
    int width = 0;
    int height = 0;
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
