#pragma once

#include "scene/scene_definition.h"

#include <filesystem>

namespace rt::scene {

SceneDefinition load_scene_definition(const std::filesystem::path& scene_file);

}  // namespace rt::scene
