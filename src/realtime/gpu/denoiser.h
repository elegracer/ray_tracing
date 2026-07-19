#pragma once

#include "realtime/gpu/launch_params.h"

#include <cuda_runtime.h>
#include <optix.h>

#include <cstddef>

namespace rt {

class OptixDenoiserWrapper {
public:
    OptixDenoiserWrapper() = default;
    ~OptixDenoiserWrapper();

    OptixDenoiserWrapper(const OptixDenoiserWrapper&) = delete;
    OptixDenoiserWrapper& operator=(const OptixDenoiserWrapper&) = delete;
    OptixDenoiserWrapper(OptixDenoiserWrapper&&) = delete;
    OptixDenoiserWrapper& operator=(OptixDenoiserWrapper&&) = delete;

    const float4* run(OptixDeviceContext context, cudaStream_t stream,
        const DeviceFrameBuffers& frame, int width, int height, float* elapsed_ms = nullptr);
    void reset_history();
    void shutdown();

private:
    void initialize(OptixDeviceContext context, cudaStream_t stream, int width, int height);

    OptixDenoiser denoiser_ = nullptr;
    OptixDeviceContext context_ = nullptr;
    cudaStream_t stream_ = nullptr;
    CUdeviceptr state_ = 0;
    CUdeviceptr scratch_ = 0;
    CUdeviceptr output_ = 0;
    CUdeviceptr previous_output_ = 0;
    CUdeviceptr previous_internal_ = 0;
    CUdeviceptr output_internal_ = 0;
    CUdeviceptr hdr_intensity_ = 0;
    std::size_t state_size_ = 0;
    std::size_t scratch_size_ = 0;
    std::size_t internal_pixel_size_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool has_previous_ = false;
};

} // namespace rt
