#pragma once

#include "realtime/gpu/device_radiance_frame_view.h"

#include <GL/gl.h>

struct cudaGraphicsResource;

namespace rt::viewer {

// Call before GLFW initializes so hybrid-GPU Linux systems create the GL context on CUDA's GPU.
void configure_cuda_gl_interop_environment();

enum class CudaGlPresentStatus {
    ok,
    invalid_device_view,
    resolution_mismatch,
};

class CudaGlTexturePresenter {
public:
    CudaGlTexturePresenter(GLuint texture, int width, int height);
    ~CudaGlTexturePresenter();

    CudaGlTexturePresenter(const CudaGlTexturePresenter&) = delete;
    CudaGlTexturePresenter& operator=(const CudaGlTexturePresenter&) = delete;
    CudaGlTexturePresenter(CudaGlTexturePresenter&&) = delete;
    CudaGlTexturePresenter& operator=(CudaGlTexturePresenter&&) = delete;

    CudaGlPresentStatus present(const DeviceRadianceFrameView& frame);

private:
    GLuint texture_ = 0;
    GLuint pixel_buffer_ = 0;
    int width_ = 0;
    int height_ = 0;
    cudaGraphicsResource* cuda_resource_ = nullptr;
};

} // namespace rt::viewer
