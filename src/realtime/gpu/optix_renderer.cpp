#include "realtime/gpu/optix_renderer.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <stdexcept>
#include <string>

namespace rt {

void launch_direction_debug_kernel(std::uint8_t* rgba, int width, int height, cudaStream_t stream);

namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

void throw_cuda_driver_error(CUresult result, const char* expr) {
    if (result != CUDA_SUCCESS) {
        const char* name = nullptr;
        const char* message = nullptr;
        cuGetErrorName(result, &name);
        cuGetErrorString(result, &message);
        throw std::runtime_error(std::string("CUDA driver failure at ") + expr + ": "
            + (name != nullptr ? name : "unknown") + " / " + (message != nullptr ? message : "unknown"));
    }
}

void throw_optix_error(OptixResult result, const char* expr) {
    if (result != OPTIX_SUCCESS) {
        throw std::runtime_error(std::string("OptiX failure at ") + expr + ": " + std::to_string(result));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)
#define RT_CUDA_DRV_CHECK(expr) throw_cuda_driver_error((expr), #expr)
#define RT_OPTIX_CHECK(expr) throw_optix_error((expr), #expr)

void context_log_cb(unsigned int level, const char* tag, const char* message, void*) {
    (void)level;
    (void)tag;
    (void)message;
}

}  // namespace

OptixRenderer::OptixRenderer() {
    initialize_optix();
    create_direction_debug_pipeline();
}

OptixRenderer::~OptixRenderer() {
    if (optix_context_ != nullptr) {
        optixDeviceContextDestroy(optix_context_);
    }
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
    }
}

void OptixRenderer::initialize_optix() {
    RT_CUDA_DRV_CHECK(cuInit(0));
    RT_CUDA_CHECK(cudaSetDevice(0));
    RT_CUDA_CHECK(cudaFree(nullptr));
    RT_CUDA_DRV_CHECK(cuCtxGetCurrent(&cu_context_));
    if (cu_context_ == nullptr) {
        throw std::runtime_error("CUDA context is null after initialization");
    }

    RT_OPTIX_CHECK(optixInit());

    OptixDeviceContextOptions options {};
    options.logCallbackFunction = &context_log_cb;
    options.logCallbackLevel = 4;
    RT_OPTIX_CHECK(optixDeviceContextCreate(cu_context_, &options, &optix_context_));

    RT_CUDA_CHECK(cudaStreamCreate(&stream_));
}

void OptixRenderer::create_direction_debug_pipeline() {
    if (optix_context_ == nullptr) {
        throw std::runtime_error("OptiX context is not initialized");
    }
}

void OptixRenderer::launch_direction_debug(const PackedCameraRig&, std::uint8_t* rgba, int width, int height) {
    launch_direction_debug_kernel(rgba, width, height, stream_);
}

DirectionDebugFrame OptixRenderer::render_direction_debug(const PackedCameraRig& rig) {
    DirectionDebugFrame frame {};
    frame.width = rig.cameras[0].width;
    frame.height = rig.cameras[0].height;
    frame.rgba.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4U, 0);

    std::uint8_t* device_rgba = nullptr;
    const std::size_t byte_count = frame.rgba.size() * sizeof(std::uint8_t);
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_rgba), byte_count));

    try {
        launch_direction_debug(rig, device_rgba, frame.width, frame.height);
        RT_CUDA_CHECK(cudaMemcpyAsync(frame.rgba.data(), device_rgba, byte_count, cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
        RT_CUDA_CHECK(cudaFree(device_rgba));
    } catch (...) {
        cudaFree(device_rgba);
        throw;
    }

    return frame;
}

}  // namespace rt
