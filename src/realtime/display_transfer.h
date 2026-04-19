#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rt {

inline float linear_to_display(float value) {
    return std::clamp(std::sqrt(std::max(value, 0.0f)), 0.0f, 0.999f);
}

inline std::uint8_t linear_to_display_u8(float value) {
    return static_cast<std::uint8_t>(std::lround(linear_to_display(value) * 255.0f));
}

}  // namespace rt
