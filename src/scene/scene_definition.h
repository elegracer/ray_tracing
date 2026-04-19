#pragma once

#include "scene/shared_scene_builders.h"
#include "scene/shared_scene_ir.h"

#include <optional>
#include <string>
#include <vector>

namespace rt::scene {

struct SceneDefinitionMetadata {
    std::string id;
    std::string label;
    Eigen::Vector3d background = Eigen::Vector3d::Zero();
    bool supports_cpu_render = false;
    bool supports_realtime = false;
};

struct SceneDefinitionCpuRenderPreset {
    std::string scene_id;
    std::string preset_id;
    int samples_per_pixel = 500;
    CpuCameraPreset camera {};
};

struct SceneDefinition {
    SceneDefinitionMetadata metadata {};
    SceneIR scene_ir {};
    std::vector<SceneDefinitionCpuRenderPreset> cpu_presets;
    std::optional<RealtimeViewPreset> realtime_preset;
    std::vector<std::string> dependencies;
};

}  // namespace rt::scene
