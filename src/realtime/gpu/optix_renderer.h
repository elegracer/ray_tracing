#pragma once

#include "realtime/camera_rig.h"
#include "realtime/gpu/launch_params.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>

#include <vector>

namespace rt {

struct RadianceTiming {
    float render_ms = 0.0f;
    float download_ms = 0.0f;
};

struct ProfiledRadianceFrame {
    RadianceFrame frame;
    RadianceTiming timing;
};

class OptixRenderer {
   public:
    OptixRenderer();
    ~OptixRenderer();

    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig);
    void prepare_scene(const PackedScene& scene);
    RadianceFrame render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    ProfiledRadianceFrame render_prepared_radiance(
        const PackedCameraRig& rig, const RenderProfile& profile, int camera_index);
    ProfiledRadianceFrame render_radiance_profiled(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);

   private:
    struct HostRadianceStaging {
        int width = 0;
        int height = 0;
        float4* beauty = nullptr;
        float4* normal = nullptr;
        float4* albedo = nullptr;
        float* depth = nullptr;
    };

    void initialize_optix();
    void create_direction_debug_pipeline();
    void launch_direction_debug(const PackedCameraRig& rig, std::uint8_t* rgba, int width, int height);
    void allocate_frame_buffers(int width, int height);
    void upload_scene(const PackedScene& scene);
    void free_device_resources();
    void free_staging_buffers();
    void build_or_refit_accels(const PackedScene& scene);
    void launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index,
        RadianceTiming* timing = nullptr);
    RadianceFrame download_radiance_frame(int camera_index) const;
    RadianceFrame download_radiance_frame_profiled(int camera_index, RadianceTiming* timing);
    void build_geometry_accels(const PackedScene& scene);
    void launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index, RadianceTiming* timing = nullptr);
    RadianceFrame download_camera_frame(int camera_index) const;
    HostRadianceStaging& staging_buffer_for(int width, int height);
    int last_launch_width(int camera_index) const;
    int last_launch_height(int camera_index) const;
    std::vector<float> download_beauty() const;
    std::vector<float> download_normal() const;
    std::vector<float> download_albedo() const;
    std::vector<float> download_depth() const;
    double compute_average_luminance(const std::vector<float>& rgba) const;

    CUcontext cu_context_ = nullptr;
    cudaStream_t stream_ = nullptr;
    OptixDeviceContext optix_context_ = nullptr;
    DeviceFrameBuffers device_frame_{};
    PackedSphere* device_spheres_ = nullptr;
    PackedQuad* device_quads_ = nullptr;
    PackedTexture* device_textures_ = nullptr;
    Eigen::Vector3f* device_image_texels_ = nullptr;
    MaterialSample* device_materials_ = nullptr;
    int device_image_texel_count_ = 0;
    int allocated_width_ = 0;
    int allocated_height_ = 0;
    PackedScene uploaded_scene_{};
    int last_width_ = 0;
    int last_height_ = 0;
    int last_camera_index_ = 0;
    int sphere_gas_count_ = 0;
    int quad_gas_count_ = 0;
    int tlas_instance_count_ = 0;
    RenderProfile last_profile_{};
    bool scene_prepared_ = false;
    std::vector<HostRadianceStaging> host_staging_buffers_{};
};

}  // namespace rt
