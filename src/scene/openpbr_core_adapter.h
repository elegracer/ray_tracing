#pragma once

#include "common/openpbr_core.h"
#include "scene/scene_ir_v2.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace rt::scene {

OpenPbrCoreMaterial compile_openpbr_core_material(const SceneOpenPbrSurface& material);
std::vector<std::optional<OpenPbrCoreMaterial>> compile_openpbr_core_material_table(
    const SceneIRv2& scene, std::size_t compatibility_material_count);

} // namespace rt::scene
