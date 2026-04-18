#pragma once

#include "common/traits.h"
#include "scene/shared_scene_ir.h"

namespace rt::scene {

struct CpuSceneAdapterResult {
    pro::proxy<Hittable> world;
    pro::proxy<Hittable> lights;
};

CpuSceneAdapterResult adapt_to_cpu(const SceneIR& scene);

}  // namespace rt::scene
