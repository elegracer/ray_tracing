#pragma once

#include "realtime/gpu/denoiser.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"

#include <array>
#include <vector>

namespace rt {

struct RealtimeFrame {
    int history_length = 0;
    RadianceFrame radiance;
};

struct RealtimeFrameSet {
    std::vector<RealtimeFrame> frames;
};

class RealtimePipeline {
   public:
    RealtimeFrameSet render_profiled_smoke_frame(int active_cameras, const RenderProfile& profile);
    RealtimeFrameSet render_profiled_smoke_frame_with_pose_jump(int active_cameras, const RenderProfile& profile);

   private:
    RealtimeFrameSet render_profiled_smoke_frame_impl(int active_cameras, const RenderProfile& profile, bool pose_jump);
    std::array<int, 4> history_lengths_{};
    OptixRenderer renderer_;
    OptixDenoiserWrapper denoiser_;
};

}  // namespace rt
