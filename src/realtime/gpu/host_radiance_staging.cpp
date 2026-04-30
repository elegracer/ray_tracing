#include "realtime/gpu/host_radiance_staging.h"

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

namespace rt {
namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)

void free_host_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFreeHost(ptr);
    }
}

void free_host_staging(HostRadianceStaging& staging) {
    free_host_ptr(staging.depth);
    free_host_ptr(staging.albedo);
    free_host_ptr(staging.normal);
    free_host_ptr(staging.beauty);
    staging = HostRadianceStaging {};
}

}  // namespace

HostRadianceStagingPool::~HostRadianceStagingPool() {
    reset();
}

HostRadianceStaging& HostRadianceStagingPool::buffer_for(int width, int height) {
    for (HostRadianceStaging& staging : buffers_) {
        if (staging.width == width && staging.height == height) {
            return staging;
        }
    }

    HostRadianceStaging staging {};
    staging.width = width;
    staging.height = height;
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&staging.beauty), pixel_count * sizeof(float4)));
    try {
        RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&staging.normal), pixel_count * sizeof(float4)));
        RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&staging.albedo), pixel_count * sizeof(float4)));
        RT_CUDA_CHECK(cudaMallocHost(reinterpret_cast<void**>(&staging.depth), pixel_count * sizeof(float)));
    } catch (...) {
        free_host_staging(staging);
        throw;
    }

    buffers_.push_back(staging);
    return buffers_.back();
}

void HostRadianceStagingPool::reset() {
    for (HostRadianceStaging& staging : buffers_) {
        free_host_staging(staging);
    }
    buffers_.clear();
}

RadianceFrame make_radiance_frame(const HostRadianceStaging& staging) {
    return make_radiance_frame(staging.width, staging.height, staging.beauty, staging.normal, staging.albedo,
        staging.depth);
}

}  // namespace rt
