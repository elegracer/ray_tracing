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

class OptixRenderer {
   public:
    OptixRenderer();
    ~OptixRenderer();

    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig);
    RadianceFrame render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);

   private:
    void initialize_optix();
    void create_direction_debug_pipeline();
    void launch_direction_debug(const PackedCameraRig& rig, std::uint8_t* rgba, int width, int height);
    void upload_scene(const PackedScene& scene);
    void build_or_refit_accels(const PackedScene& scene);
    void launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index);
    RadianceFrame download_radiance_frame(int camera_index) const;
    void build_geometry_accels(const PackedScene& scene);
    void launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    RadianceFrame download_camera_frame(int camera_index) const;
    int last_launch_width(int camera_index) const;
    int last_launch_height(int camera_index) const;
    std::vector<float> download_beauty(int camera_index) const;
    std::vector<float> download_normal(int camera_index) const;
    std::vector<float> download_albedo(int camera_index) const;
    std::vector<float> download_depth(int camera_index) const;
    double compute_average_luminance(const std::vector<float>& rgba) const;

    CUcontext cu_context_ = nullptr;
    cudaStream_t stream_ = nullptr;
    OptixDeviceContext optix_context_ = nullptr;
    PackedScene uploaded_scene_{};
    int last_width_ = 0;
    int last_height_ = 0;
    int last_camera_index_ = 0;
    int sphere_gas_count_ = 0;
    int quad_gas_count_ = 0;
    int tlas_instance_count_ = 0;
    RenderProfile last_profile_{};
};

}  // namespace rt
