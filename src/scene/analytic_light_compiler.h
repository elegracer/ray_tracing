#pragma once

#include "common/analytic_light.h"
#include "scene/scene_ir_v2.h"

#include <vector>

namespace rt::scene {

std::vector<AnalyticLightDesc> compile_analytic_lights(const SceneIRv2& scene);

} // namespace rt::scene
