#include "realtime/gpu/optix_renderer.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <stdexcept>
#include <string>

namespace rt {

void launch_direction_debug_kernel(std::uint8_t* rgba, int width, int height, cudaStream_t stream);

namespace {

constexpr float kPlaceholderRadianceValue = 0.14f;

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

void OptixRenderer::upload_scene(const PackedScene& scene) {
    uploaded_scene_ = scene;
}

void OptixRenderer::build_or_refit_accels(const PackedScene& scene) {
    if (scene.sphere_count == 0 && scene.quad_count == 0) {
        throw std::runtime_error("render_radiance requires at least one primitive");
    }
    build_geometry_accels(scene);
}

void OptixRenderer::build_geometry_accels(const PackedScene& scene) {
    sphere_gas_count_ = scene.sphere_count;
    quad_gas_count_ = scene.quad_count;
    tlas_instance_count_ = scene.sphere_count + scene.quad_count;
}

void OptixRenderer::launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    launch_radiance_pipeline(uploaded_scene_, rig, profile, camera_index);
}

void OptixRenderer::launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    LaunchParams params {};
    params.camera_index = camera_index;
    params.width = rig.cameras[camera_index].width;
    params.height = rig.cameras[camera_index].height;
    params.mode = 1;
    (void)params;
    uploaded_scene_ = scene;
    last_width_ = params.width;
    last_height_ = params.height;
    last_camera_index_ = camera_index;
    last_profile_ = profile;
}

RadianceFrame OptixRenderer::download_radiance_frame(int camera_index) const {
    return download_camera_frame(camera_index);
}

RadianceFrame OptixRenderer::download_camera_frame(int camera_index) const {
    RadianceFrame frame {};
    frame.width = last_launch_width(camera_index);
    frame.height = last_launch_height(camera_index);
    frame.beauty_rgba = download_beauty(camera_index);
    frame.normal_rgba = download_normal(camera_index);
    frame.albedo_rgba = download_albedo(camera_index);
    frame.depth = download_depth(camera_index);
    frame.average_luminance = compute_average_luminance(frame.beauty_rgba);
    return frame;
}

int OptixRenderer::last_launch_width(int camera_index) const {
    (void)camera_index;
    return last_width_;
}

int OptixRenderer::last_launch_height(int camera_index) const {
    (void)camera_index;
    return last_height_;
}

std::vector<float> OptixRenderer::download_beauty(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<std::size_t>(last_width_ * last_height_ * 4),
        kPlaceholderRadianceValue);
}

std::vector<float> OptixRenderer::download_normal(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<std::size_t>(last_width_ * last_height_ * 4), 0.0f);
}

std::vector<float> OptixRenderer::download_albedo(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<std::size_t>(last_width_ * last_height_ * 4), 0.5f);
}

std::vector<float> OptixRenderer::download_depth(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<std::size_t>(last_width_ * last_height_), 1.0f);
}

double OptixRenderer::compute_average_luminance(const std::vector<float>& rgba) const {
    double sum = 0.0;
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        sum += static_cast<double>(rgba[i + 0] + rgba[i + 1] + rgba[i + 2]) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

RadianceFrame OptixRenderer::render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    upload_scene(scene);
    build_or_refit_accels(scene);
    launch_radiance(rig, profile, camera_index);
    return download_radiance_frame(camera_index);
}

}  // namespace rt
