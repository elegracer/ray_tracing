#pragma once

#include "scene/scene_ir_v2.h"

#include <filesystem>

namespace rt::scene {

bool openusd_stage_exporter_available();
void export_openusd_stage(const SceneIRv2& scene, const std::filesystem::path& path);

} // namespace rt::scene
