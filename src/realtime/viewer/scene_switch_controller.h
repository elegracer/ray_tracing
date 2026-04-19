#pragma once

#include <string_view>
#include <string>

namespace rt::viewer {

struct SceneSwitchResult {
    bool applied = false;
    bool reset_pose = false;
    std::string error_message;
};

struct SceneCatalogUpdateResult {
    bool ok = false;
    bool reload_active_scene = false;
    std::string error_message;
};

class SceneSwitchController {
public:
    explicit SceneSwitchController(std::string initial_scene_id);

    void request_scene(std::string scene_id);
    SceneSwitchResult resolve_pending();
    SceneCatalogUpdateResult reload_current_scene();
    SceneCatalogUpdateResult rescan_scene_directory(std::string_view root);

    const std::string& current_scene_id() const;
    const std::string& last_error() const;

private:
    std::string current_scene_id_;
    std::string pending_scene_id_;
    std::string last_error_;
};

}  // namespace rt::viewer
