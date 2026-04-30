#include "realtime/gpu/cuda_event_timer.h"
#include "test_support.h"

#include <cuda_runtime.h>

int main() {
    cudaStream_t stream = nullptr;
    cudaStreamCreate(&stream);

    {
        rt::CudaEventTimer timer(stream);
        timer.record_start();
        cudaFree(nullptr);
        const float elapsed_ms = timer.record_stop_and_elapsed_ms();
        expect_true(elapsed_ms >= 0.0f, "elapsed ms non-negative");
    }

    cudaStreamDestroy(stream);
    return 0;
}
