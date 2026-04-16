#pragma once

#include "realtime/gpu/denoiser.h"
#include "realtime/gpu/optix_renderer.h"

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
    RealtimeFrameSet render_smoke_frame(int active_cameras);
    RealtimeFrameSet render_smoke_frame_with_pose_jump(int active_cameras);

   private:
    std::array<int, 4> history_lengths_{};
    OptixRenderer renderer_;
    OptixDenoiserWrapper denoiser_;
};

}  // namespace rt
