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

    expect_true(rt::scene::find_scene_metadata("unknown") == nullptr, "unknown metadata");

    bool build_unknown_threw = false;
    try {
        (void)rt::scene::build_scene("unknown");
    } catch (...) {
        build_unknown_threw = true;
    }
    expect_true(build_unknown_threw, "unknown build throws");

    bool spp_unknown_threw = false;
    try {
        (void)rt::scene::scene_default_samples_per_pixel("unknown");
    } catch (...) {
        spp_unknown_threw = true;
    }
    expect_true(spp_unknown_threw, "unknown spp throws");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists for capability");

        const bool is_realtime_only = (id == "smoke" || id == "final_room");
        if (is_realtime_only) {
            expect_true(!entry->supports_cpu_render, "realtime-only non-cpu");
            expect_true(entry->supports_realtime, "realtime-only realtime");
        } else {
            expect_true(entry->supports_cpu_render, "offline cpu");
            expect_true(!entry->supports_realtime, "offline non-realtime");
        }
    }
    return 0;
}
