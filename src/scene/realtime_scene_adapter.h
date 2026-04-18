#pragma once

#include "realtime/scene_description.h"
#include "scene/shared_scene_ir.h"

namespace rt::scene {

SceneDescription adapt_to_realtime(const SceneIR& scene);

}  // namespace rt::scene
