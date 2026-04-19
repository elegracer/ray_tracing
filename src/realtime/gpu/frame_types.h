#pragma once

#include <cstdint>
#include <vector>

namespace rt {

struct DirectionDebugFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
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

}  // namespace rt
