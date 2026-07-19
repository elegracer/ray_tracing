#pragma once

#include "scene/scene_ir_v2.h"

#include <filesystem>

namespace rt::scene {

bool openusd_stage_importer_available();
SceneIRv2 import_openusd_stage(const std::filesystem::path& path);

} // namespace rt::scene
