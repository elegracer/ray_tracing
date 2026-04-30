#pragma once

#include <cuda_runtime.h>

namespace rt {

class CudaEventTimer {
   public:
    explicit CudaEventTimer(cudaStream_t stream);
    ~CudaEventTimer();

    CudaEventTimer(const CudaEventTimer&) = delete;
    CudaEventTimer& operator=(const CudaEventTimer&) = delete;
    CudaEventTimer(CudaEventTimer&&) = delete;
    CudaEventTimer& operator=(CudaEventTimer&&) = delete;

    void record_start();
    float record_stop_and_elapsed_ms();

   private:
    cudaStream_t stream_ = nullptr;
    cudaEvent_t start_ = nullptr;
    cudaEvent_t stop_ = nullptr;
};

}  // namespace rt
