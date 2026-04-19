#include "realtime/viewer/viewer_quality_controller.h"

#include <cmath>

namespace rt::viewer {

namespace {

double pose_translation_delta(const BodyPose& a, const BodyPose& b) {
    return (a.position - b.position).norm();
}

double pose_rotation_delta_deg(const BodyPose& a, const BodyPose& b) {
    return std::max(std::abs(a.yaw_deg - b.yaw_deg), std::abs(a.pitch_deg - b.pitch_deg));
}

}  // namespace

ViewerQualityController::ViewerQualityController(RenderProfile preview_profile, RenderProfile converge_profile)
    : preview_profile_(preview_profile), converge_profile_(converge_profile), histories_(4) {}

void ViewerQualityController::begin_frame(std::string_view scene_id, const BodyPose& pose) {
    const bool scene_changed = current_scene_id_ != scene_id;
    const bool pose_reset = has_last_pose_ && pose_exceeded_reset_threshold(pose);

    if (!has_last_pose_ || scene_changed || pose_reset) {
        clear_histories();
        stable_frame_count_ = 0;
        active_mode_ = ViewerQualityMode::preview;
    } else {
        ++stable_frame_count_;
        active_mode_ = stable_frame_count_ >= 1 ? ViewerQualityMode::converge : ViewerQualityMode::preview;
    }

    current_scene_id_ = std::string(scene_id);
    last_pose_ = pose;
    has_last_pose_ = true;
}

RadianceFrame ViewerQualityController::resolve_frame(int camera_index, const RadianceFrame& raw_frame) {
    CameraHistory& history = history_for(camera_index);
    if (history.width != raw_frame.width || history.height != raw_frame.height) {
        history = {};
    }

    if (active_mode_ == ViewerQualityMode::preview) {
        history = {};
        return raw_frame;
    }

    history.width = raw_frame.width;
    history.height = raw_frame.height;
    history.beauty_rgba = raw_frame.beauty_rgba;
    if (history.history_length == 0) {
        history.history_length = 1;
    } else {
        ++history.history_length;
    }

    return raw_frame;
}

void ViewerQualityController::reset_all() {
    clear_histories();
    active_mode_ = ViewerQualityMode::preview;
    current_scene_id_.clear();
    last_pose_ = {};
    has_last_pose_ = false;
    stable_frame_count_ = 0;
}

ViewerQualityMode ViewerQualityController::active_mode() const {
    return active_mode_;
}

const RenderProfile& ViewerQualityController::active_profile() const {
    return active_mode_ == ViewerQualityMode::preview ? preview_profile_ : converge_profile_;
}

int ViewerQualityController::history_length(int camera_index) const {
    return history_for(camera_index).history_length;
}

bool ViewerQualityController::pose_exceeded_reset_threshold(const BodyPose& pose) const {
    const RenderProfile& profile = active_profile();
    return pose_translation_delta(last_pose_, pose) > profile.accumulation_reset_translation
        || pose_rotation_delta_deg(last_pose_, pose) > profile.accumulation_reset_rotation_deg;
}

void ViewerQualityController::clear_histories() {
    for (CameraHistory& history : histories_) {
        history = {};
    }
}

ViewerQualityController::CameraHistory& ViewerQualityController::history_for(int camera_index) {
    return histories_.at(static_cast<std::size_t>(camera_index));
}

const ViewerQualityController::CameraHistory& ViewerQualityController::history_for(int camera_index) const {
    return histories_.at(static_cast<std::size_t>(camera_index));
}

}  // namespace rt::viewer
