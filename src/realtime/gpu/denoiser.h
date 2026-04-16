#pragma once

#include "realtime/gpu/launch_params.h"

namespace rt {

class OptixDenoiserWrapper {
   public:
    void initialize(int width, int height);
    void run(RadianceFrame& frame);

   private:
    int width_ = 0;
    int height_ = 0;

    void allocate_state(int width, int height);
    void denoise_in_place(std::vector<float>& beauty, const std::vector<float>& albedo,
        const std::vector<float>& normal, int width, int height);
};

}  // namespace rt
