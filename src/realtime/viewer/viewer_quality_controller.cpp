#include "realtime/viewer/viewer_quality_controller.h"

#include <algorithm>
#include <cmath>

namespace rt::viewer {

namespace {

constexpr float kMaxSanitizedBeautyValue = 64.0f;

double pose_translation_delta(const BodyPose& a, const BodyPose& b) {
    return (a.position - b.position).norm();
}

double pose_rotation_delta_deg(const BodyPose& a, const BodyPose& b) {
    return std::max(std::abs(a.yaw_deg - b.yaw_deg), std::abs(a.pitch_deg - b.pitch_deg));
}

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

bool is_valid_beauty_value(float value) {
    // Task 3's approved sanitization rule treats unusually large finite beauty values as invalid samples.
    return std::isfinite(value) && value >= 0.0f && value <= kMaxSanitizedBeautyValue;
}

float sanitized_value(float candidate, float fallback) {
    return is_valid_beauty_value(candidate) ? candidate : fallback;
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

RadianceFrame ViewerQualityController::materialize_frame(const ResolvedBeautyFrameView& resolved_view,
    const RadianceFrame& raw_frame) {
    RadianceFrame resolved = raw_frame;
    resolved.average_luminance = resolved_view.average_luminance;
    resolved.beauty_rgba.assign(resolved_view.beauty_rgba.begin(), resolved_view.beauty_rgba.end());
    return resolved;
}

ResolvedBeautyFrameView ViewerQualityController::resolve_beauty_view(int camera_index, const RadianceFrame& raw_frame) {
    CameraHistory& history = history_for(camera_index);
    const std::size_t expected_beauty_size =
        static_cast<std::size_t>(raw_frame.width) * static_cast<std::size_t>(raw_frame.height) * 4;
    if (raw_frame.width <= 0 || raw_frame.height <= 0 || raw_frame.beauty_rgba.size() < expected_beauty_size) {
        history = {};
        return ResolvedBeautyFrameView {
            .width = raw_frame.width,
            .height = raw_frame.height,
            .average_luminance = raw_frame.average_luminance,
            .beauty_rgba = raw_frame.beauty_rgba,
        };
    }

    if (history.width != raw_frame.width || history.height != raw_frame.height) {
        history = {};
    }

    if (active_mode_ == ViewerQualityMode::preview) {
        history = {};
        return ResolvedBeautyFrameView {
            .width = raw_frame.width,
            .height = raw_frame.height,
            .average_luminance = raw_frame.average_luminance,
            .beauty_rgba = raw_frame.beauty_rgba,
        };
    }

    history.width = raw_frame.width;
    history.height = raw_frame.height;
    if (history.history_length == 0) {
        history.beauty_rgba.resize(expected_beauty_size);
        for (std::size_t i = 0; i < expected_beauty_size; ++i) {
            const float fallback = (i % 4 == 3) ? 1.0f : 0.0f;
            history.beauty_rgba[i] = sanitized_value(raw_frame.beauty_rgba[i], fallback);
        }
        history.history_length = 1;
    } else {
        const int next_history_length = history.history_length + 1;
        const float blend = 1.0f / static_cast<float>(next_history_length);
        for (std::size_t i = 0; i < expected_beauty_size; ++i) {
            const float previous = history.beauty_rgba[i];
            const float current = sanitized_value(raw_frame.beauty_rgba[i], previous);
            history.beauty_rgba[i] = previous + (current - previous) * blend;
        }
        history.history_length = next_history_length;
    }

    return ResolvedBeautyFrameView {
        .width = raw_frame.width,
        .height = raw_frame.height,
        .average_luminance = compute_average_luminance(history.beauty_rgba),
        .beauty_rgba = history.beauty_rgba,
    };
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
