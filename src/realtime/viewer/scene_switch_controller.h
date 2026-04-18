#pragma once

#include <string>

namespace rt::viewer {

struct SceneSwitchResult {
    bool applied = false;
    bool reset_pose = false;
    std::string error_message;
};

class SceneSwitchController {
public:
    explicit SceneSwitchController(std::string initial_scene_id);

    void request_scene(std::string scene_id);
    SceneSwitchResult resolve_pending();

    const std::string& current_scene_id() const;
    const std::string& last_error() const;

private:
    std::string current_scene_id_;
    std::string pending_scene_id_;
    std::string last_error_;
};

}  // namespace rt::viewer
