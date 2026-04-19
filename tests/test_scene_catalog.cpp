#include "realtime/scene_catalog.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 12> expected_ids {
        "bouncing_spheres",
        "checkered_spheres",
        "earth_sphere",
        "perlin_spheres",
        "quads",
        "simple_light",
        "cornell_smoke",
        "cornell_box",
        "cornell_box_and_sphere",
        "rttnw_final_scene",
        "smoke",
        "final_room",
    };

    const auto& scenes = rt::scene_catalog();
    expect_true(scenes.size() == expected_ids.size(), "scene count");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");
        expect_true(entry->id == id, "catalog id matches");
        expect_true(!entry->label.empty(), "catalog label non-empty");
    }

    expect_true(rt::find_scene_catalog_entry("final_room")->supports_realtime, "final_room realtime");
    expect_true(rt::find_scene_catalog_entry("cornell_box")->supports_realtime, "cornell_box realtime");
    expect_true(rt::find_scene_catalog_entry("quads")->supports_realtime, "quads realtime");
    expect_true(rt::find_scene_catalog_entry("quads")->supports_cpu_render, "quads cpu");
    expect_true(!rt::find_scene_catalog_entry("smoke")->supports_cpu_render, "smoke realtime-only");
    expect_true(rt::find_scene_catalog_entry("cornell_box_extreme") == nullptr,
        "extreme scene id removed from public catalog");
    return 0;
}
