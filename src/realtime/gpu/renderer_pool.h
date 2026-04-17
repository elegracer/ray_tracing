#pragma once

#include "realtime/gpu/optix_renderer.h"

#include <vector>

namespace rt {

struct CameraRenderResult {
    int camera_index = 0;
    ProfiledRadianceFrame profiled;
};

class RendererPool {
   public:
    explicit RendererPool(int renderer_count);

    void prepare_scene(const PackedScene& scene);
    std::vector<CameraRenderResult> render_frame(
        const PackedCameraRig& rig, const RenderProfile& profile, int active_cameras);

   private:
    std::vector<OptixRenderer> renderers_;
};

}  // namespace rt
