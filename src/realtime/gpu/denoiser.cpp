#include "realtime/gpu/denoiser.h"

#include <algorithm>

namespace rt {

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
}

}  // namespace rt
