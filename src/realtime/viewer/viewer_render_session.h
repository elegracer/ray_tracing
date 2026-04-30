#pragma once

#include "realtime/gpu/frame_types.h"
#include "realtime/render_profile.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/viewer_quality_controller.h"

#include <functional>
#include <string_view>
#include <vector>

namespace rt::viewer {

struct ViewerRawFrame {
    int camera_index = 0;
    RadianceFrame frame;
};

struct ViewerDisplayFrame {
    int camera_index = 0;
    RadianceFrame frame;
    ResolvedBeautyFrameView display_frame;
};

struct ViewerRenderBatch {
    ViewerQualityMode mode = ViewerQualityMode::preview;
    RenderProfile profile;
    std::vector<ViewerDisplayFrame> frames;
};

using ResetAccumulationFn = std::function<void()>;
using RenderViewerFramesFn = std::function<std::vector<ViewerRawFrame>(const RenderProfile&)>;

class ViewerRenderSession {
public:
    ViewerRenderSession(RenderProfile preview_profile, RenderProfile converge_profile);

    ViewerRenderBatch render_frame(std::string_view scene_id, const BodyPose& pose,
        const ResetAccumulationFn& reset_accumulation, const RenderViewerFramesFn& render_frames);
    void reset_all();

private:
    ViewerQualityController quality_controller_;
};

}  // namespace rt::viewer
