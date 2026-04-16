#pragma once

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

}  // namespace rt
