#include "realtime/gpu/denoiser.h"

#include <algorithm>
#include <cmath>

namespace rt {

namespace {

double compute_average_luminance(const std::vector<float>& rgba) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

}  // namespace

void OptixDenoiserWrapper::allocate_state(int width, int height) {
    width_ = width;
    height_ = height;
}

void OptixDenoiserWrapper::initialize(int width, int height) {
    allocate_state(width, height);
}

void OptixDenoiserWrapper::denoise_in_place(std::vector<float>& beauty, const std::vector<float>& albedo,
    const std::vector<float>& normal, int width, int height) {
    (void)albedo;
    (void)normal;
    (void)width;
    (void)height;
    for (float& value : beauty) {
        value = std::min(value, 1.0f);
    }
}

void OptixDenoiserWrapper::run(RadianceFrame& frame) {
    if (frame.width == 0 || frame.height == 0) {
        return;
    }
    if (width_ != frame.width || height_ != frame.height) {
        initialize(frame.width, frame.height);
    }
    denoise_in_place(frame.beauty_rgba, frame.albedo_rgba, frame.normal_rgba, frame.width, frame.height);
    frame.average_luminance = compute_average_luminance(frame.beauty_rgba);
}

}  // namespace rt
