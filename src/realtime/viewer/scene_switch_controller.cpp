#include "realtime/viewer/scene_switch_controller.h"

#include "realtime/realtime_scene_factory.h"
#include "scene/scene_file_catalog.h"

#include <exception>
#include <utility>

namespace rt::viewer {
namespace {

constexpr std::string_view kRealtimeUnavailableMessage = "Scene is not available in realtime";

SceneCatalogUpdateResult make_error_result(std::string message) {
    return SceneCatalogUpdateResult {
        .ok = false,
        .reload_active_scene = false,
        .error_message = std::move(message),
    };
}

}  // namespace

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

SceneCatalogUpdateResult SceneSwitchController::reload_current_scene() {
    scene::SceneFileCatalog snapshot = scene::global_scene_file_catalog();
    const scene::ReloadStatus status = scene::global_scene_file_catalog().reload_scene(current_scene_id_);
    if (!status.ok) {
        last_error_ = status.error_message;
        return make_error_result(last_error_);
    }
    if (!rt::realtime_scene_supported(current_scene_id_)) {
        scene::global_scene_file_catalog() = std::move(snapshot);
        last_error_ = std::string(kRealtimeUnavailableMessage);
        return make_error_result(last_error_);
    }

    last_error_.clear();
    return SceneCatalogUpdateResult {
        .ok = true,
        .reload_active_scene = true,
    };
}

SceneCatalogUpdateResult SceneSwitchController::rescan_scene_directory(std::string_view root) {
    scene::SceneFileCatalog snapshot = scene::global_scene_file_catalog();
    try {
        scene::global_scene_file_catalog().scan_directory(std::string(root));
    } catch (const std::exception& err) {
        last_error_ = err.what();
        return make_error_result(last_error_);
    }
    if (!rt::realtime_scene_supported(current_scene_id_)) {
        scene::global_scene_file_catalog() = std::move(snapshot);
        last_error_ = std::string(kRealtimeUnavailableMessage);
        return make_error_result(last_error_);
    }

    last_error_.clear();
    return SceneCatalogUpdateResult {
        .ok = true,
        .reload_active_scene = true,
    };
}

const std::string& SceneSwitchController::current_scene_id() const {
    return current_scene_id_;
}

const std::string& SceneSwitchController::last_error() const {
    return last_error_;
}

}  // namespace rt::viewer
