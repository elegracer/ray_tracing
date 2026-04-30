#include "realtime/viewer/viewer_render_session.h"

#include <utility>

namespace rt::viewer {

ViewerRenderSession::ViewerRenderSession(RenderProfile preview_profile, RenderProfile converge_profile)
    : quality_controller_(preview_profile, converge_profile) {}

ViewerRenderBatch ViewerRenderSession::render_frame(std::string_view scene_id, const BodyPose& pose,
    const ResetAccumulationFn& reset_accumulation, const RenderViewerFramesFn& render_frames) {
    quality_controller_.begin_frame(scene_id, pose);
    if (quality_controller_.active_mode() == ViewerQualityMode::preview) {
        reset_accumulation();
    }

    const RenderProfile profile = quality_controller_.active_profile();
    std::vector<ViewerRawFrame> raw_frames = render_frames(profile);

    ViewerRenderBatch batch {
        .mode = quality_controller_.active_mode(),
        .profile = profile,
    };
    batch.frames.reserve(raw_frames.size());
    for (ViewerRawFrame& raw_frame : raw_frames) {
        ViewerDisplayFrame display_frame {
            .camera_index = raw_frame.camera_index,
            .frame = std::move(raw_frame.frame),
        };
        display_frame.display_frame =
            quality_controller_.resolve_beauty_view(display_frame.camera_index, display_frame.frame);
        batch.frames.push_back(std::move(display_frame));
    }
    return batch;
}

void ViewerRenderSession::reset_all() {
    quality_controller_.reset_all();
}

}  // namespace rt::viewer
