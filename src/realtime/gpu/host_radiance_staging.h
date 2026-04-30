#pragma once

#include "realtime/gpu/radiance_frame_assembly.h"

#include <deque>
#include <vector_types.h>

namespace rt {

struct HostRadianceStaging {
    int width = 0;
    int height = 0;
    float4* beauty = nullptr;
    float4* normal = nullptr;
    float4* albedo = nullptr;
    float* depth = nullptr;
};

class HostRadianceStagingPool {
   public:
    HostRadianceStagingPool() = default;
    ~HostRadianceStagingPool();

    HostRadianceStagingPool(const HostRadianceStagingPool&) = delete;
    HostRadianceStagingPool& operator=(const HostRadianceStagingPool&) = delete;
    HostRadianceStagingPool(HostRadianceStagingPool&&) = delete;
    HostRadianceStagingPool& operator=(HostRadianceStagingPool&&) = delete;

    HostRadianceStaging& buffer_for(int width, int height);
    void reset();

   private:
    std::deque<HostRadianceStaging> buffers_;
};

RadianceFrame make_radiance_frame(const HostRadianceStaging& staging);

}  // namespace rt
