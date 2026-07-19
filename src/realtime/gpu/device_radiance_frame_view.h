#pragma once

#include <cuda_runtime.h>

namespace rt {

struct DeviceRadianceFrameView {
    const float4* beauty_rgba = nullptr;
    int width = 0;
    int height = 0;
    cudaStream_t stream = nullptr;
};

} // namespace rt
