#pragma once

#include "realtime/gpu/frame_types.h"

#include <vector>
#include <vector_types.h>

namespace rt {

std::vector<float> unpack_float4_rgba(const float4* pixels, std::size_t pixel_count);
double compute_frame_average_luminance(const std::vector<float>& rgba);
RadianceFrame make_empty_radiance_frame(int width, int height);
RadianceFrame make_radiance_frame(
    int width, int height, const float4* beauty, const float4* normal, const float4* albedo, const float* depth);

}  // namespace rt
