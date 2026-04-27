#include "realtime/viewer/viewer_quality_controller.h"

#include <algorithm>
#include <cmath>

namespace rt::viewer {

namespace {

double compute_average_luminance(const std::vector<float>& rgba) {
    double sum = 0.0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(std::max(0.0, static_cast<double>(rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

}  // namespace

ViewerQualityController::ViewerQualityController(RenderProfile preview_profile, RenderProfile converge_profile)
    : preview_profile_(preview_profile), converge_profile_(converge_profile), histories_(4) {}

void ViewerQualityController::begin_frame(std::string_view scene_id, const BodyPose& pose) {
    const bool scene_changed = current_scene_id_ != scene_id;

    if (is_first_frame_ || scene_changed) {
        clear_histories();
        stable_frame_count_ = 0;
        active_mode_ = ViewerQualityMode::preview;
    } else {
        ++stable_frame_count_;
        active_mode_ = stable_frame_count_ >= 1 ? ViewerQualityMode::converge : ViewerQualityMode::preview;
    }

    current_scene_id_ = std::string(scene_id);
    is_first_frame_ = false;
    (void)pose;
}

RadianceFrame ViewerQualityController::materialize_frame(const ResolvedBeautyFrameView& resolved_view,
    const RadianceFrame& raw_frame) {
    RadianceFrame resolved = raw_frame;
    resolved.average_luminance = resolved_view.average_luminance;
    resolved.beauty_rgba.assign(resolved_view.beauty_rgba.begin(), resolved_view.beauty_rgba.end());
    return resolved;
}

ResolvedBeautyFrameView ViewerQualityController::resolve_beauty_view(
    int camera_index, const RadianceFrame& raw_frame) {
    (void)camera_index;
    return ResolvedBeautyFrameView {
        .width = raw_frame.width,
        .height = raw_frame.height,
        .average_luminance = compute_average_luminance(raw_frame.beauty_rgba),
        .beauty_rgba = raw_frame.beauty_rgba,
    };
}

void ViewerQualityController::reset_all() {
    clear_histories();
    active_mode_ = ViewerQualityMode::preview;
    current_scene_id_.clear();
    is_first_frame_ = true;
    stable_frame_count_ = 0;
}

ViewerQualityMode ViewerQualityController::active_mode() const {
    return active_mode_;
}

const RenderProfile& ViewerQualityController::active_profile() const {
    return active_mode_ == ViewerQualityMode::preview ? preview_profile_ : converge_profile_;
}

int ViewerQualityController::history_length(int camera_index) const {
    (void)camera_index;
    return 0;
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
