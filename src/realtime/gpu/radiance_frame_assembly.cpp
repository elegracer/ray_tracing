#include "realtime/gpu/radiance_frame_assembly.h"

#include <algorithm>
#include <cmath>

namespace rt {

std::vector<float> unpack_float4_rgba(const float4* pixels, std::size_t pixel_count) {
    std::vector<float> rgba(pixel_count * 4U, 0.0f);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        rgba[i * 4U + 0] = pixels[i].x;
        rgba[i * 4U + 1] = pixels[i].y;
        rgba[i * 4U + 2] = pixels[i].z;
        rgba[i * 4U + 3] = pixels[i].w;
    }
    return rgba;
}

double compute_frame_average_luminance(const std::vector<float>& rgba) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

RadianceFrame make_empty_radiance_frame(int width, int height) {
    RadianceFrame frame {};
    frame.width = width;
    frame.height = height;
    return frame;
}

RadianceFrame make_radiance_frame(
    int width, int height, const float4* beauty, const float4* normal, const float4* albedo, const float* depth) {
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RadianceFrame frame = make_empty_radiance_frame(width, height);
    if (pixel_count == 0 || beauty == nullptr || normal == nullptr || albedo == nullptr || depth == nullptr) {
        return frame;
    }

    frame.beauty_rgba = unpack_float4_rgba(beauty, pixel_count);
    frame.normal_rgba = unpack_float4_rgba(normal, pixel_count);
    frame.albedo_rgba = unpack_float4_rgba(albedo, pixel_count);
    frame.depth.assign(depth, depth + pixel_count);
    frame.average_luminance = compute_frame_average_luminance(frame.beauty_rgba);
    return frame;
}

}  // namespace rt
