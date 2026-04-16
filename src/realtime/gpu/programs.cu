#include <cuda_runtime.h>

#include <cstdint>
#include <stdexcept>

namespace rt {

namespace {

__global__ void direction_debug_kernel(std::uint8_t* rgba, int width, int height) {
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= width || y >= height) {
        return;
    }

    const int pixel_index = y * width + x;
    rgba[4 * pixel_index + 0] = static_cast<std::uint8_t>((255 * x) / max(width, 1));
    rgba[4 * pixel_index + 1] = static_cast<std::uint8_t>((255 * y) / max(height, 1));
    rgba[4 * pixel_index + 2] = 128;
    rgba[4 * pixel_index + 3] = 255;
}

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA kernel launch failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

}  // namespace

void launch_direction_debug_kernel(std::uint8_t* rgba, int width, int height, cudaStream_t stream) {
    const dim3 block_size(16, 16, 1);
    const dim3 grid_size(
        static_cast<unsigned int>((width + static_cast<int>(block_size.x) - 1) / static_cast<int>(block_size.x)),
        static_cast<unsigned int>((height + static_cast<int>(block_size.y) - 1) / static_cast<int>(block_size.y)),
        1);
    direction_debug_kernel<<<grid_size, block_size, 0, stream>>>(rgba, width, height);
    throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
}

}  // namespace rt
