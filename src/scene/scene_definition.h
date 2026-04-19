#pragma once

#include "scene/shared_scene_builders.h"
#include "scene/shared_scene_ir.h"

#include <optional>
#include <string>
#include <vector>

namespace rt::scene {

struct SceneDefinition {
    SceneMetadata metadata {};
    SceneIR scene_ir {};
    std::vector<CpuRenderPreset> cpu_presets;
    std::optional<RealtimeViewPreset> realtime_preset;
    std::vector<std::string> dependencies;
};

}  // namespace rt::scene
