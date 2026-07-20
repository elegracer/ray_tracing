#pragma once

#include "realtime/camera_rig.h"
#include "realtime/gpu/device_frame_buffers.h"
#include "realtime/gpu/device_radiance_frame_view.h"
#include "realtime/gpu/device_scene_buffers.h"
#include "realtime/gpu/denoiser.h"
#include "realtime/gpu/frame_types.h"
#include "realtime/gpu/gpu_scene_acceleration.h"
#include "realtime/gpu/host_radiance_staging.h"
#include "realtime/gpu/launch_params.h"
#include "realtime/gpu/radiance_launch_setup.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace rt {

struct RadianceTiming {
    float render_ms = 0.0f;
    float denoise_ms = 0.0f;
    float download_ms = 0.0f;
};

struct ProfiledRadianceFrame {
    RadianceFrame frame;
    RadianceTiming timing;
};

struct ProfiledDeviceRadianceFrame {
    DeviceRadianceFrameView frame;
    RadianceTiming timing;
};

struct RestirDiagnostics {
    int pixel_count = 0;
    int active_reservoir_count = 0;
    int temporal_reuse_count = 0;
    int max_candidate_count = 0;
    int max_age = 0;
};

struct LaunchParameterDiagnostics {
    std::uint64_t allocation_count = 0;
    std::uint64_t upload_count = 0;
};

class SharedGpuSceneState {
public:
    AccelerationUpdateStats prepare(const PackedScene& scene);
    DeviceSceneView view() const;
    const AccelerationUpdateStats& last_acceleration_update() const;

private:
    DeviceSceneBuffers buffers_;
    GpuSceneAcceleration acceleration_;
    AccelerationUpdateStats last_acceleration_update_ {};
};

class OptixRenderer {
public:
    explicit OptixRenderer(std::shared_ptr<SharedGpuSceneState> shared_scene = nullptr);
    ~OptixRenderer();

    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig, int camera_index = 0);
    void prepare_scene(const PackedScene& scene);
    void use_prepared_scene(const PackedScene& scene);
    void reset_accumulation();
    void reset_sequence(std::uint32_t sample_stream);
    RestirDiagnostics restir_diagnostics() const;
    LaunchParameterDiagnostics launch_parameter_diagnostics() const;
    const AccelerationUpdateStats& acceleration_diagnostics() const;
    RadianceFrame render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    // The view remains valid until this renderer launches another frame or releases its resources.
    ProfiledDeviceRadianceFrame render_prepared_device(const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    ProfiledRadianceFrame render_prepared_radiance(const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    ProfiledRadianceFrame render_radiance_profiled(const PackedScene& scene,
        const PackedCameraRig& rig, const RenderProfile& profile, int camera_index);

private:
    void initialize_optix();
    void create_direction_debug_pipeline();
    void upload_scene(const PackedScene& scene);
    void free_device_resources();
    void launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index,
        RadianceTiming* timing = nullptr);
    RadianceFrame download_radiance_frame(int camera_index) const;
    RadianceFrame download_radiance_frame_profiled(int camera_index, const float4* beauty_source,
        RadianceTiming* timing);
    void launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index, RadianceTiming* timing = nullptr);
    RadianceFrame download_camera_frame(int camera_index) const;
    int last_launch_width(int camera_index) const;
    int last_launch_height(int camera_index) const;
    std::vector<float> download_beauty() const;
    std::vector<float> download_normal() const;
    std::vector<float> download_albedo() const;
    std::vector<float> download_depth() const;

    CUcontext cu_context_ = nullptr;
    cudaStream_t stream_ = nullptr;
    OptixDeviceContext optix_context_ = nullptr;
    DeviceFrameBufferSet frame_buffers_;
    OptixDenoiserWrapper denoiser_;
    std::shared_ptr<SharedGpuSceneState> shared_scene_;
    LaunchParams* device_launch_params_ = nullptr;
    PackedScene uploaded_scene_ {};
    int last_width_ = 0;
    int last_height_ = 0;
    int last_camera_index_ = 0;
    RenderProfile last_profile_ {};
    bool scene_prepared_ = false;
    std::uint32_t launch_sample_stream_ = 0;
    LaunchParameterDiagnostics launch_parameter_diagnostics_ {};
    HostRadianceStagingPool host_staging_;
};

} // namespace rt
