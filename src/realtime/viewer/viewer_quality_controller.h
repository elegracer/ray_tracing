#pragma once

#include "realtime/gpu/frame_types.h"
#include "realtime/render_profile.h"
#include "realtime/viewer/body_pose.h"

#include <string>
#include <string_view>
#include <vector>

namespace rt::viewer {

enum class ViewerQualityMode {
    preview,
    converge,
};

class ViewerQualityController {
public:
    ViewerQualityController(RenderProfile preview_profile, RenderProfile converge_profile);

    void begin_frame(std::string_view scene_id, const BodyPose& pose);
    RadianceFrame resolve_frame(int camera_index, const RadianceFrame& raw_frame);
    void reset_all();

    ViewerQualityMode active_mode() const;
    const RenderProfile& active_profile() const;
    int history_length(int camera_index) const;

private:
    struct CameraHistory {
        int width = 0;
        int height = 0;
        int history_length = 0;
        std::vector<float> beauty_rgba;
    };

    bool pose_exceeded_reset_threshold(const BodyPose& pose) const;
    void clear_histories();
    CameraHistory& history_for(int camera_index);
    const CameraHistory& history_for(int camera_index) const;

    RenderProfile preview_profile_;
    RenderProfile converge_profile_;
    ViewerQualityMode active_mode_ = ViewerQualityMode::preview;
    std::string current_scene_id_;
    BodyPose last_pose_ {};
    bool has_last_pose_ = false;
    int stable_frame_count_ = 0;
    std::vector<CameraHistory> histories_;
};

}  // namespace rt::viewer
