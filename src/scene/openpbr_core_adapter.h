#pragma once

#include "common/openpbr_core.h"
#include "scene/scene_ir_v2.h"

namespace rt::scene {

OpenPbrCoreMaterial compile_openpbr_core_material(const SceneOpenPbrSurface& material);

} // namespace rt::scene
