#pragma once

#include "realtime/camera_rig.h"
#include "realtime/gpu/launch_params.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <optix.h>

namespace rt {

class OptixRenderer {
   public:
    OptixRenderer();
    ~OptixRenderer();

    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig);

   private:
    void initialize_optix();
    void create_direction_debug_pipeline();
    void launch_direction_debug(const PackedCameraRig& rig, std::uint8_t* rgba, int width, int height);

    CUcontext cu_context_ = nullptr;
    cudaStream_t stream_ = nullptr;
    OptixDeviceContext optix_context_ = nullptr;
};

}  // namespace rt
