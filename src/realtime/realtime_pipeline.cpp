#include "realtime/realtime_pipeline.h"

#include <stdexcept>

namespace rt {

namespace {

void validate_active_cameras(int active_cameras) {
    if (active_cameras < 1 || active_cameras > 4) {
        throw std::runtime_error("realtime pipeline supports 1..4 active cameras");
    }
}

}  // namespace

RealtimeFrameSet RealtimePipeline::render_smoke_frame(int active_cameras) {
    validate_active_cameras(active_cameras);

    RealtimeFrameSet out {};
    out.frames.resize(static_cast<std::size_t>(active_cameras));
    for (int i = 0; i < active_cameras; ++i) {
        history_lengths_[i] += 1;
        out.frames[static_cast<std::size_t>(i)].history_length = history_lengths_[i];
    }
    return out;
}

RealtimeFrameSet RealtimePipeline::render_smoke_frame_with_pose_jump(int active_cameras) {
    validate_active_cameras(active_cameras);

    RealtimeFrameSet out {};
    out.frames.resize(static_cast<std::size_t>(active_cameras));
    for (int i = 0; i < active_cameras; ++i) {
        history_lengths_[i] = 1;
        out.frames[static_cast<std::size_t>(i)].history_length = history_lengths_[i];
    }
    return out;
}

}  // namespace rt
