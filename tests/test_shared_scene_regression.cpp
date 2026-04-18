#include "core/offline_shared_scene_renderer.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 5> representative_scenes {
        "quads",
        "earth_sphere",
        "cornell_smoke",
        "bouncing_spheres",
        "rttnw_final_scene",
    };

    for (std::string_view id : representative_scenes) {
        const rt::scene::SceneIR scene = rt::scene::build_scene(id);
        expect_true(!scene.materials().empty(), "shared builder materials");
        expect_true(!scene.shapes().empty(), "shared builder shapes");

        const rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
        const rt::PackedScene packed = realtime.pack();
        expect_true(packed.material_count > 0, "packed materials");
        expect_true(packed.sphere_count + packed.quad_count > 0, "packed geometry");
    }

    const rt::PackedScene earth = rt::scene::adapt_to_realtime(rt::scene::build_scene("earth_sphere")).pack();
    expect_true(earth.texture_count >= 1, "earth texture survives shared path");

    const rt::PackedScene smoke = rt::scene::adapt_to_realtime(rt::scene::build_scene("cornell_smoke")).pack();
    expect_true(smoke.medium_count >= 1, "smoke medium survives shared path");

    const cv::Mat earth_image = rt::render_shared_scene("earth_sphere", 1);
    expect_true(!earth_image.empty(), "earth sphere renders through shared offline path");

    const cv::Mat smoke_image = rt::render_shared_scene("cornell_smoke", 1);
    expect_true(!smoke_image.empty(), "cornell smoke renders through shared offline path");
    return 0;
}
