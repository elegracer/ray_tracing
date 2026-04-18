#pragma once

#include "scene/shared_scene_ir.h"

#include <string_view>
#include <vector>

namespace rt::scene {

struct SceneMetadata {
    std::string_view id;
    std::string_view label;
    int default_samples_per_pixel = 500;
    bool supports_cpu_render = false;
    bool supports_realtime = false;
};

const std::vector<SceneMetadata>& scene_metadata();
const SceneMetadata* find_scene_metadata(std::string_view scene_id);

SceneIR build_scene(std::string_view scene_id);
int scene_default_samples_per_pixel(std::string_view scene_id);

}  // namespace rt::scene
