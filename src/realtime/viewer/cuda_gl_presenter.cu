#include "realtime/viewer/cuda_gl_presenter.h"

#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace rt::viewer {
namespace {

void check_cuda(cudaError_t error, const char* expression) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA/OpenGL interop failure at ") + expression + ": "
                                 + cudaGetErrorString(error));
    }
}

#define RT_CUDA_GL_CHECK(expr) check_cuda((expr), #expr)

__device__ unsigned char linear_to_display_u8(float value) {
    const float display = fminf(sqrtf(fmaxf(value, 0.0f)), 0.999f);
    return static_cast<unsigned char>(lroundf(display * 255.0f));
}

__global__ void tone_map_to_rgba8(const float4* source, uchar4* destination,
    std::size_t pixel_count) {
    const std::size_t index = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (index >= pixel_count) {
        return;
    }

    const float4 linear = source[index];
    destination[index] = make_uchar4(linear_to_display_u8(linear.x), linear_to_display_u8(linear.y),
        linear_to_display_u8(linear.z), 255);
}

} // namespace

void configure_cuda_gl_interop_environment() {
#if defined(__linux__)
    (void)setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
    (void)setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
#endif
}

CudaGlTexturePresenter::CudaGlTexturePresenter(GLuint texture, int width, int height)
    : texture_(texture),
      width_(width),
      height_(height) {
    if (texture_ == 0 || width_ <= 0 || height_ <= 0) {
        throw std::runtime_error("CudaGlTexturePresenter requires a valid texture and resolution");
    }

    glGenBuffers(1, &pixel_buffer_);
    if (pixel_buffer_ == 0) {
        throw std::runtime_error("failed to allocate OpenGL pixel buffer");
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,
        static_cast<GLsizeiptr>(
            static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * sizeof(uchar4)),
        nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    const cudaError_t register_error = cudaGraphicsGLRegisterBuffer(&cuda_resource_, pixel_buffer_,
        cudaGraphicsRegisterFlagsWriteDiscard);
    if (register_error != cudaSuccess) {
        glDeleteBuffers(1, &pixel_buffer_);
        pixel_buffer_ = 0;
        check_cuda(register_error, "cudaGraphicsGLRegisterBuffer");
    }
}

CudaGlTexturePresenter::~CudaGlTexturePresenter() {
    if (cuda_resource_ != nullptr) {
        (void)cudaGraphicsUnregisterResource(cuda_resource_);
    }
    if (pixel_buffer_ != 0) {
        glDeleteBuffers(1, &pixel_buffer_);
    }
}

CudaGlPresentStatus CudaGlTexturePresenter::present(const DeviceRadianceFrameView& frame) {
    if (frame.beauty_rgba == nullptr || frame.stream == nullptr) {
        return CudaGlPresentStatus::invalid_device_view;
    }
    if (frame.width != width_ || frame.height != height_) {
        return CudaGlPresentStatus::resolution_mismatch;
    }

    RT_CUDA_GL_CHECK(cudaGraphicsMapResources(1, &cuda_resource_, frame.stream));

    void* mapped_pointer = nullptr;
    std::size_t mapped_size = 0;
    const cudaError_t pointer_error =
        cudaGraphicsResourceGetMappedPointer(&mapped_pointer, &mapped_size, cuda_resource_);
    if (pointer_error != cudaSuccess) {
        (void)cudaGraphicsUnmapResources(1, &cuda_resource_, frame.stream);
        check_cuda(pointer_error, "cudaGraphicsResourceGetMappedPointer");
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    const std::size_t required_size = pixel_count * sizeof(uchar4);
    if (mapped_pointer == nullptr || mapped_size < required_size) {
        (void)cudaGraphicsUnmapResources(1, &cuda_resource_, frame.stream);
        throw std::runtime_error("CUDA/OpenGL pixel buffer mapping is smaller than the frame");
    }

    constexpr int kBlockSize = 256;
    const int block_count =
        static_cast<int>((pixel_count + static_cast<std::size_t>(kBlockSize) - 1)
                         / static_cast<std::size_t>(kBlockSize));
    tone_map_to_rgba8<<<block_count, kBlockSize, 0, frame.stream>>>(frame.beauty_rgba,
        static_cast<uchar4*>(mapped_pointer), pixel_count);
    const cudaError_t launch_error = cudaGetLastError();
    if (launch_error != cudaSuccess) {
        (void)cudaGraphicsUnmapResources(1, &cuda_resource_, frame.stream);
        check_cuda(launch_error, "tone_map_to_rgba8 launch");
    }

    RT_CUDA_GL_CHECK(cudaGraphicsUnmapResources(1, &cuda_resource_, frame.stream));
    RT_CUDA_GL_CHECK(cudaStreamSynchronize(frame.stream));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_buffer_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return CudaGlPresentStatus::ok;
}

} // namespace rt::viewer
