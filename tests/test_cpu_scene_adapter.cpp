#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 3> scene_ids {
        "checkered_spheres",
        "earth_sphere",
        "cornell_smoke",
    };

    for (const std::string_view scene_id : scene_ids) {
        const rt::scene::SceneIR scene = rt::scene::build_scene(scene_id);
        const rt::scene::CpuSceneAdapterResult adapted = rt::scene::adapt_to_cpu(scene);
        expect_true(adapted.world.has_value(), "adapted world should be non-null");
    }

    return 0;
}
