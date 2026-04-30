#include "realtime/gpu/cuda_event_timer.h"

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

}  // namespace

CudaEventTimer::CudaEventTimer(cudaStream_t stream)
    : stream_(stream) {
    RT_CUDA_CHECK(cudaEventCreate(&start_));
    try {
        RT_CUDA_CHECK(cudaEventCreate(&stop_));
    } catch (...) {
        cudaEventDestroy(start_);
        start_ = nullptr;
        throw;
    }
}

CudaEventTimer::~CudaEventTimer() {
    if (stop_ != nullptr) {
        cudaEventDestroy(stop_);
    }
    if (start_ != nullptr) {
        cudaEventDestroy(start_);
    }
}

void CudaEventTimer::record_start() {
    RT_CUDA_CHECK(cudaEventRecord(start_, stream_));
}

float CudaEventTimer::record_stop_and_elapsed_ms() {
    RT_CUDA_CHECK(cudaEventRecord(stop_, stream_));
    RT_CUDA_CHECK(cudaEventSynchronize(stop_));
    float elapsed_ms = 0.0f;
    RT_CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, start_, stop_));
    return elapsed_ms;
}

}  // namespace rt
