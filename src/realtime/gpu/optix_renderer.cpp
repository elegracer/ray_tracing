#include "realtime/gpu/optix_renderer.h"

#include "realtime/gpu/cuda_event_timer.h"
#include "realtime/gpu/direction_debug_renderer.h"
#include "realtime/gpu/packed_scene_preparation.h"
#include "realtime/gpu/radiance_frame_assembly.h"
#include "realtime/gpu/radiance_launch_setup.h"
#include "realtime/gpu/render_request_validation.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace rt {

void launch_radiance_kernel(const LaunchParams& params, cudaStream_t stream);
void launch_resolve_kernel(const LaunchParams& params, cudaStream_t stream);

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

int checked_primitive_count(std::size_t count, const char* label) {
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string("scene ") + label + " count exceeds int range");
    }
    return static_cast<int>(count);
}

struct PrimitiveCounts {
    int sphere_count = 0;
    int quad_count = 0;
    int triangle_count = 0;
    int medium_count = 0;
};

PrimitiveCounts primitive_counts(const PackedScene& scene) {
    return PrimitiveCounts {
        .sphere_count = checked_primitive_count(scene.spheres.size(), "sphere"),
        .quad_count = checked_primitive_count(scene.quads.size(), "quad"),
        .triangle_count = checked_primitive_count(scene.triangles.size(), "triangle"),
        .medium_count = checked_primitive_count(scene.media.size(), "medium"),
    };
}

}  // namespace

OptixRenderer::OptixRenderer() {
    initialize_optix();
    create_direction_debug_pipeline();
}

OptixRenderer::~OptixRenderer() {
    free_device_resources();
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

DirectionDebugFrame OptixRenderer::render_direction_debug(const PackedCameraRig& rig, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");

    return render_direction_debug_frame(rig.cameras[static_cast<std::size_t>(camera_index)], stream_);
}

void OptixRenderer::upload_scene(const PackedScene& scene) {
    scene_prepared_ = false;
    scene_buffers_.upload(prepare_gpu_scene(scene));
    uploaded_scene_ = scene;
}

void OptixRenderer::free_device_resources() {
    frame_buffers_.reset_frame();
    frame_buffers_.reset_history();
    scene_buffers_.reset();
    host_staging_.reset();
    uploaded_scene_ = PackedScene {};
    scene_prepared_ = false;
}

void OptixRenderer::reset_accumulation() {
    frame_buffers_.reset_accumulation();
}

void OptixRenderer::build_or_refit_accels(const PackedScene& scene) {
    const PrimitiveCounts counts = primitive_counts(scene);
    if (counts.sphere_count == 0 && counts.quad_count == 0 && counts.triangle_count == 0 && counts.medium_count == 0) {
        throw std::runtime_error("render_radiance requires at least one primitive");
    }
    build_geometry_accels(scene);
}

void OptixRenderer::build_geometry_accels(const PackedScene& scene) {
    const PrimitiveCounts counts = primitive_counts(scene);
    sphere_gas_count_ = counts.sphere_count;
    quad_gas_count_ = counts.quad_count;
    triangle_gas_count_ = counts.triangle_count;
    tlas_instance_count_ = counts.sphere_count + counts.quad_count + counts.triangle_count;
}

void OptixRenderer::launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index,
    RadianceTiming* timing) {
    launch_radiance_pipeline(uploaded_scene_, rig, profile, camera_index, timing);
}

void OptixRenderer::launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index, RadianceTiming* timing) {
    const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];
    frame_buffers_.resize_frame(camera.width, camera.height);
    LaunchParams params = make_radiance_launch_params(
        scene, scene_buffers_.view(), rig, profile, camera_index, launch_sample_stream_++, frame_buffers_.frame(),
        frame_buffers_.history_state());
    const std::size_t pixel_count = static_cast<std::size_t>(params.width) * static_cast<std::size_t>(params.height);

    const auto launch = [&]() {
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.beauty, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.normal, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.albedo, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.depth, 0, pixel_count * sizeof(float), stream_));
        launch_radiance_kernel(params, stream_);
        frame_buffers_.resize_history(params.width, params.height);
        params.history = frame_buffers_.history();
        launch_resolve_kernel(params, stream_);
        frame_buffers_.apply_history_state(capture_launch_history(params));
        frame_buffers_.copy_frame_to_history(stream_);
    };

    if (timing == nullptr) {
        launch();
    } else {
        CudaEventTimer timer(stream_);
        timer.record_start();
        launch();
        timing->render_ms = timer.record_stop_and_elapsed_ms();
    }
    uploaded_scene_ = scene;
    last_width_ = params.width;
    last_height_ = params.height;
    last_camera_index_ = camera_index;
    last_profile_ = profile;
}

RadianceFrame OptixRenderer::download_radiance_frame(int camera_index) const {
    return download_camera_frame(camera_index);
}

RadianceFrame OptixRenderer::download_radiance_frame_profiled(int camera_index, RadianceTiming* timing) {
    RadianceFrame frame {};
    frame.width = last_launch_width(camera_index);
    frame.height = last_launch_height(camera_index);

    const std::size_t pixel_count = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.beauty == nullptr || device_frame.normal == nullptr
        || device_frame.albedo == nullptr || device_frame.depth == nullptr) {
        return frame;
    }

    HostRadianceStaging& staging = host_staging_.buffer_for(frame.width, frame.height);

    if (timing == nullptr) {
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.beauty, device_frame.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.normal, device_frame.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.albedo, device_frame.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(
            cudaMemcpyAsync(staging.depth, device_frame.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost,
                stream_));
        RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
    } else {
        CudaEventTimer timer(stream_);
        timer.record_start();
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.beauty, device_frame.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.normal, device_frame.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(
            staging.albedo, device_frame.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(
            cudaMemcpyAsync(staging.depth, device_frame.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost,
                stream_));
        timing->download_ms = timer.record_stop_and_elapsed_ms();
    }

    frame = make_radiance_frame(staging);

    return frame;
}

RadianceFrame OptixRenderer::download_camera_frame(int camera_index) const {
    const int width = last_launch_width(camera_index);
    const int height = last_launch_height(camera_index);
    RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
    std::vector<float> beauty = download_beauty();
    const double average_luminance = compute_frame_average_luminance(beauty);
    return RadianceFrame {
        .width = width,
        .height = height,
        .average_luminance = average_luminance,
        .beauty_rgba = std::move(beauty),
        .normal_rgba = download_normal(),
        .albedo_rgba = download_albedo(),
        .depth = download_depth(),
    };
}

int OptixRenderer::last_launch_width(int camera_index) const {
    (void)camera_index;
    return last_width_;
}

int OptixRenderer::last_launch_height(int camera_index) const {
    (void)camera_index;
    return last_height_;
}

std::vector<float> OptixRenderer::download_beauty() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.beauty == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_normal() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.normal == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_albedo() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.albedo == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_depth() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.depth == nullptr) {
        return {};
    }
    std::vector<float> depth(pixel_count, 0.0f);
    RT_CUDA_CHECK(
        cudaMemcpy(depth.data(), device_frame.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost));
    return depth;
}

void OptixRenderer::prepare_scene(const PackedScene& scene) {
    upload_scene(scene);
    build_or_refit_accels(scene);
    scene_prepared_ = true;
    launch_sample_stream_ = 0;
    reset_accumulation();
}

RadianceFrame OptixRenderer::render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    upload_scene(scene);
    build_or_refit_accels(scene);
    launch_radiance(rig, profile, camera_index);
    return download_radiance_frame(camera_index);
}

ProfiledRadianceFrame OptixRenderer::render_prepared_radiance(
    const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    if (!scene_prepared_) {
        throw std::runtime_error("render_prepared_radiance requires prepare_scene() first");
    }

    ProfiledRadianceFrame profiled {};
    launch_radiance(rig, profile, camera_index, &profiled.timing);
    profiled.frame = download_radiance_frame_profiled(camera_index, &profiled.timing);
    return profiled;
}

ProfiledRadianceFrame OptixRenderer::render_radiance_profiled(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    prepare_scene(scene);
    return render_prepared_radiance(rig, profile, camera_index);
}

}  // namespace rt
