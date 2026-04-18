#include "realtime/scene_catalog.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 16> expected_ids {
        "bouncing_spheres",
        "checkered_spheres",
        "earth_sphere",
        "perlin_spheres",
        "quads",
        "simple_light",
        "cornell_smoke",
        "cornell_smoke_extreme",
        "cornell_box",
        "cornell_box_extreme",
        "cornell_box_and_sphere",
        "cornell_box_and_sphere_extreme",
        "rttnw_final_scene",
        "rttnw_final_scene_extreme",
        "smoke",
        "final_room",
    };

    const auto& catalog = rt::scene_catalog();
    expect_true(catalog.size() == expected_ids.size(), "catalog scene count");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");

        const rt::scene::SceneIR scene = rt::scene::build_scene(id);
        expect_true(!scene.materials().empty(), "scene has materials");
        expect_true(!scene.shapes().empty(), "scene has shapes");
    }

    expect_true(rt::scene::scene_default_samples_per_pixel("cornell_box_extreme") == 10000, "extreme spp");
    expect_true(rt::scene::scene_default_samples_per_pixel("cornell_box") == 1000, "cornell spp");

    expect_true(rt::find_scene_catalog_entry("cornell_box")->supports_cpu_render, "cornell_box cpu");
    expect_true(!rt::find_scene_catalog_entry("cornell_box")->supports_realtime, "cornell_box non-realtime");
    expect_true(!rt::find_scene_catalog_entry("smoke")->supports_cpu_render, "smoke non-cpu");
    expect_true(rt::find_scene_catalog_entry("smoke")->supports_realtime, "smoke realtime");
    expect_true(!rt::find_scene_catalog_entry("final_room")->supports_cpu_render, "final_room non-cpu");
    expect_true(rt::find_scene_catalog_entry("final_room")->supports_realtime, "final_room realtime");
    return 0;
}
