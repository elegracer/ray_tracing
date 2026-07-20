#include "realtime/gpu/optix_renderer.h"

#include "realtime/gpu/cuda_event_timer.h"
#include "realtime/gpu/direction_debug_renderer.h"
#include "realtime/gpu/packed_scene_preparation.h"
#include "realtime/gpu/radiance_frame_assembly.h"
#include "realtime/gpu/radiance_launch_setup.h"
#include "realtime/gpu/render_request_validation.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace rt {

void upload_launch_params(LaunchParams* device_params, const LaunchParams& params,
    cudaStream_t stream);
void launch_radiance_kernel(const LaunchParams& params, const LaunchParams* device_params,
    cudaStream_t stream);
void launch_resolve_kernel(const LaunchParams& params, const LaunchParams* device_params,
    cudaStream_t stream);

namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(
            std::string("CUDA runtime failure at ") + expr + ": " + cudaGetErrorString(error));
    }
}

void throw_cuda_driver_error(CUresult result, const char* expr) {
    if (result != CUDA_SUCCESS) {
        const char* name = nullptr;
        const char* message = nullptr;
        cuGetErrorName(result, &name);
        cuGetErrorString(result, &message);
        throw std::runtime_error(std::string("CUDA driver failure at ") + expr + ": "
                                 + (name != nullptr ? name : "unknown") + " / "
                                 + (message != nullptr ? message : "unknown"));
    }
}

void throw_optix_error(OptixResult result, const char* expr) {
    if (result != OPTIX_SUCCESS) {
        throw std::runtime_error(
            std::string("OptiX failure at ") + expr + ": " + std::to_string(result));
    }
}

#define RT_CUDA_CHECK(expr)     throw_cuda_error((expr), #expr)
#define RT_CUDA_DRV_CHECK(expr) throw_cuda_driver_error((expr), #expr)
#define RT_OPTIX_CHECK(expr)    throw_optix_error((expr), #expr)

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

} // namespace

AccelerationUpdateStats SharedGpuSceneState::prepare(const PackedScene& scene) {
    const auto begin = std::chrono::steady_clock::now();
    const int surface_count = checked_primitive_count(scene.spheres.size(), "sphere")
                              + checked_primitive_count(scene.quads.size(), "quad")
                              + checked_primitive_count(scene.triangles.size(), "triangle");
    if (surface_count == 0) {
        throw std::runtime_error("render_radiance requires at least one surface primitive");
    }
    const GpuPreparedScene prepared = prepare_gpu_scene(scene);
    last_acceleration_update_ = acceleration_.update(prepared);
    buffers_.upload(prepared, acceleration_, last_acceleration_update_.kind);
    const auto end = std::chrono::steady_clock::now();
    last_acceleration_update_.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - begin).count();
    return last_acceleration_update_;
}

DeviceSceneView SharedGpuSceneState::view() const {
    return buffers_.view();
}

const AccelerationUpdateStats& SharedGpuSceneState::last_acceleration_update() const {
    return last_acceleration_update_;
}

OptixRenderer::OptixRenderer(std::shared_ptr<SharedGpuSceneState> shared_scene)
    : shared_scene_(
          shared_scene ? std::move(shared_scene) : std::make_shared<SharedGpuSceneState>()) {
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
    RT_CUDA_CHECK(
        cudaMalloc(reinterpret_cast<void**>(&device_launch_params_), sizeof(LaunchParams)));
    launch_parameter_diagnostics_.allocation_count = 1;
}

void OptixRenderer::create_direction_debug_pipeline() {
    if (optix_context_ == nullptr) {
        throw std::runtime_error("OptiX context is not initialized");
    }
}

DirectionDebugFrame OptixRenderer::render_direction_debug(const PackedCameraRig& rig,
    int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");

    return render_direction_debug_frame(rig.cameras[static_cast<std::size_t>(camera_index)],
        stream_);
}

void OptixRenderer::upload_scene(const PackedScene& scene) {
    scene_prepared_ = false;
    shared_scene_->prepare(scene);
    uploaded_scene_ = scene;
}

void OptixRenderer::free_device_resources() {
    denoiser_.shutdown();
    frame_buffers_.reset_frame();
    frame_buffers_.reset_history();
    host_staging_.reset();
    if (device_launch_params_ != nullptr) {
        cudaFree(device_launch_params_);
        device_launch_params_ = nullptr;
    }
    uploaded_scene_ = PackedScene {};
    scene_prepared_ = false;
}

void OptixRenderer::reset_accumulation() {
    frame_buffers_.reset_accumulation();
    denoiser_.reset_history();
}

void OptixRenderer::reset_sequence(std::uint32_t sample_stream) {
    launch_sample_stream_ = sample_stream;
    reset_accumulation();
}

RestirDiagnostics OptixRenderer::restir_diagnostics() const {
    RestirDiagnostics diagnostics {};
    const std::size_t pixel_count = static_cast<std::size_t>(frame_buffers_.frame_width())
                                    * static_cast<std::size_t>(frame_buffers_.frame_height());
    diagnostics.pixel_count = static_cast<int>(pixel_count);
    const RestirReservoir* device_reservoirs = frame_buffers_.frame().restir_reservoirs;
    if (pixel_count == 0 || device_reservoirs == nullptr) {
        return diagnostics;
    }

    std::vector<RestirReservoir> reservoirs(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(reservoirs.data(), device_reservoirs,
        pixel_count * sizeof(RestirReservoir), cudaMemcpyDeviceToHost));
    for (const RestirReservoir& reservoir : reservoirs) {
        if (reservoir.valid == 0) {
            continue;
        }
        ++diagnostics.active_reservoir_count;
        if (reservoir.temporal_candidate_count > 0) {
            ++diagnostics.temporal_reuse_count;
        }
        diagnostics.max_candidate_count =
            std::max(diagnostics.max_candidate_count, reservoir.candidate_count);
        diagnostics.max_age = std::max(diagnostics.max_age, reservoir.age);
    }
    return diagnostics;
}

LaunchParameterDiagnostics OptixRenderer::launch_parameter_diagnostics() const {
    return launch_parameter_diagnostics_;
}

const AccelerationUpdateStats& OptixRenderer::acceleration_diagnostics() const {
    return shared_scene_->last_acceleration_update();
}

void OptixRenderer::launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile,
    int camera_index, RadianceTiming* timing) {
    launch_radiance_pipeline(uploaded_scene_, rig, profile, camera_index, timing);
}

void OptixRenderer::launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index, RadianceTiming* timing) {
    const PackedCamera& camera = rig.cameras[static_cast<std::size_t>(camera_index)];
    frame_buffers_.resize_frame(camera.width, camera.height);
    frame_buffers_.resize_history(camera.width, camera.height);
    LaunchParams params =
        make_radiance_launch_params(scene, shared_scene_->view(), rig, profile, camera_index,
            launch_sample_stream_++, frame_buffers_.frame(), frame_buffers_.history_state());
    const std::size_t pixel_count =
        static_cast<std::size_t>(params.width) * static_cast<std::size_t>(params.height);

    const auto launch = [&]() {
        RT_CUDA_CHECK(
            cudaMemsetAsync(params.frame.beauty, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(
            cudaMemsetAsync(params.frame.normal, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.denoiser_normal, 0, pixel_count * sizeof(float4),
            stream_));
        RT_CUDA_CHECK(
            cudaMemsetAsync(params.frame.albedo, 0, pixel_count * sizeof(float4), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.depth, 0, pixel_count * sizeof(float), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.flow, 0, pixel_count * sizeof(float2), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.flow_trustworthiness, 0,
            pixel_count * sizeof(float), stream_));
        RT_CUDA_CHECK(cudaMemsetAsync(params.frame.restir_reservoirs, 0,
            pixel_count * sizeof(RestirReservoir), stream_));
        upload_launch_params(device_launch_params_, params, stream_);
        ++launch_parameter_diagnostics_.upload_count;
        launch_radiance_kernel(params, device_launch_params_, stream_);
        launch_resolve_kernel(params, device_launch_params_, stream_);
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

RadianceFrame OptixRenderer::download_radiance_frame_profiled(int camera_index,
    const float4* beauty_source, RadianceTiming* timing) {
    RadianceFrame frame {};
    frame.width = last_launch_width(camera_index);
    frame.height = last_launch_height(camera_index);

    const std::size_t pixel_count =
        static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || beauty_source == nullptr || device_frame.normal == nullptr
        || device_frame.albedo == nullptr || device_frame.depth == nullptr) {
        return frame;
    }

    HostRadianceStaging& staging = host_staging_.buffer_for(frame.width, frame.height);

    if (timing == nullptr) {
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.beauty, beauty_source, pixel_count * sizeof(float4),
            cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.normal, device_frame.normal,
            pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.albedo, device_frame.albedo,
            pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.depth, device_frame.depth,
            pixel_count * sizeof(float), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
    } else {
        CudaEventTimer timer(stream_);
        timer.record_start();
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.beauty, beauty_source, pixel_count * sizeof(float4),
            cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.normal, device_frame.normal,
            pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.albedo, device_frame.albedo,
            pixel_count * sizeof(float4), cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaMemcpyAsync(staging.depth, device_frame.depth,
            pixel_count * sizeof(float), cudaMemcpyDeviceToHost, stream_));
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
    const std::size_t pixel_count =
        static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.beauty == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(host_pixels.data(), device_frame.beauty, pixel_count * sizeof(float4),
        cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_normal() const {
    const std::size_t pixel_count =
        static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.normal == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(host_pixels.data(), device_frame.normal, pixel_count * sizeof(float4),
        cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_albedo() const {
    const std::size_t pixel_count =
        static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.albedo == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(host_pixels.data(), device_frame.albedo, pixel_count * sizeof(float4),
        cudaMemcpyDeviceToHost));
    return unpack_float4_rgba(host_pixels.data(), pixel_count);
}

std::vector<float> OptixRenderer::download_depth() const {
    const std::size_t pixel_count =
        static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    const DeviceFrameBuffers& device_frame = frame_buffers_.frame();
    if (pixel_count == 0 || device_frame.depth == nullptr) {
        return {};
    }
    std::vector<float> depth(pixel_count, 0.0f);
    RT_CUDA_CHECK(cudaMemcpy(depth.data(), device_frame.depth, pixel_count * sizeof(float),
        cudaMemcpyDeviceToHost));
    return depth;
}

void OptixRenderer::prepare_scene(const PackedScene& scene) {
    upload_scene(scene);
    use_prepared_scene(scene);
}

void OptixRenderer::use_prepared_scene(const PackedScene& scene) {
    uploaded_scene_ = scene;
    scene_prepared_ = true;
    launch_sample_stream_ = 0;
    reset_accumulation();
}

RadianceFrame OptixRenderer::render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    reset_accumulation();
    upload_scene(scene);
    launch_radiance(rig, profile, camera_index);
    const DeviceFrameBuffers& frame = frame_buffers_.frame();
    const float4* beauty_source = frame.beauty;
    if (profile.enable_denoise) {
        beauty_source = denoiser_.run(optix_context_, stream_, frame,
            last_launch_width(camera_index), last_launch_height(camera_index));
    }
    return download_radiance_frame_profiled(camera_index, beauty_source, nullptr);
}

ProfiledDeviceRadianceFrame OptixRenderer::render_prepared_device(const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    if (!scene_prepared_) {
        throw std::runtime_error("render_prepared_device requires prepare_scene() first");
    }

    ProfiledDeviceRadianceFrame profiled {};
    launch_radiance(rig, profile, camera_index, &profiled.timing);
    const DeviceFrameBuffers& frame = frame_buffers_.frame();
    const float4* beauty_source = frame.beauty;
    if (profile.enable_denoise) {
        beauty_source =
            denoiser_.run(optix_context_, stream_, frame, last_launch_width(camera_index),
                last_launch_height(camera_index), &profiled.timing.denoise_ms);
    }
    profiled.frame = DeviceRadianceFrameView {
        .beauty_rgba = beauty_source,
        .width = last_launch_width(camera_index),
        .height = last_launch_height(camera_index),
        .stream = stream_,
    };
    return profiled;
}

ProfiledRadianceFrame OptixRenderer::render_prepared_radiance(const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    const ProfiledDeviceRadianceFrame device = render_prepared_device(rig, profile, camera_index);
    ProfiledRadianceFrame profiled {
        .timing = device.timing,
    };
    profiled.frame =
        download_radiance_frame_profiled(camera_index, device.frame.beauty_rgba, &profiled.timing);
    return profiled;
}

ProfiledRadianceFrame OptixRenderer::render_radiance_profiled(const PackedScene& scene,
    const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    validate_render_camera_request(rig, camera_index, "render_radiance");
    prepare_scene(scene);
    return render_prepared_radiance(rig, profile, camera_index);
}

} // namespace rt
