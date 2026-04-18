#include "realtime/viewer/scene_switch_controller.h"

#include "realtime/realtime_scene_factory.h"

namespace rt::viewer {

SceneSwitchController::SceneSwitchController(std::string initial_scene_id)
    : current_scene_id_(std::move(initial_scene_id)) {}

void SceneSwitchController::request_scene(std::string scene_id) {
    pending_scene_id_ = std::move(scene_id);
}

SceneSwitchResult SceneSwitchController::resolve_pending() {
    SceneSwitchResult result;
    if (pending_scene_id_.empty() || pending_scene_id_ == current_scene_id_) {
        return result;
    }

    if (!rt::realtime_scene_supported(pending_scene_id_)) {
        last_error_ = "Scene is not available in realtime";
        result.error_message = last_error_;
        pending_scene_id_.clear();
        return result;
    }

    current_scene_id_ = pending_scene_id_;
    pending_scene_id_.clear();
    last_error_.clear();
    result.applied = true;
    result.reset_pose = true;
    return result;
}

const std::string& SceneSwitchController::current_scene_id() const {
    return current_scene_id_;
}

const std::string& SceneSwitchController::last_error() const {
    return last_error_;
}

}  // namespace rt::viewer
