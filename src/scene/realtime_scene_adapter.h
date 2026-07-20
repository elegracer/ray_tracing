#pragma once

#include "realtime/scene_description.h"
#include "scene/scene_ir_v2.h"
#include "scene/shared_scene_ir.h"

namespace rt::scene {

SceneDescription adapt_to_realtime(const SceneIR& scene);
SceneDescription adapt_to_realtime_openpbr(const SceneIR& compatibility_scene,
    const SceneIRv2& scene_v2);
SceneDescription adapt_scene_ir_v2_to_realtime(const SceneIRv2& scene_v2);

} // namespace rt::scene
