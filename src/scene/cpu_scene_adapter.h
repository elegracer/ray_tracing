#pragma once

#include "common/traits.h"
#include "scene/scene_ir_v2.h"
#include "scene/shared_scene_ir.h"

namespace rt::scene {

struct CpuSceneAdapterResult {
    pro::proxy<Hittable> world;
    pro::proxy<Hittable> lights;
};

CpuSceneAdapterResult adapt_to_cpu(const SceneIR& scene);
CpuSceneAdapterResult adapt_to_cpu_openpbr(const SceneIR& compatibility_scene,
    const SceneIRv2& scene_v2);

} // namespace rt::scene
