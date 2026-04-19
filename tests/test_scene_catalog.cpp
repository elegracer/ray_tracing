#include "realtime/scene_catalog.h"
#include "test_support.h"

#include <iterator>
#include <string_view>

int main() {
    constexpr std::string_view builtin_ids[] {
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
    expect_true(scenes.size() >= std::size(builtin_ids) + 1, "scene count includes file-backed scenes");
    const rt::SceneCatalogEntry* stable_storage = scenes.data();
    expect_true(stable_storage != nullptr, "scene catalog storage exists");
    expect_true(rt::scene_catalog().data() == stable_storage, "scene catalog storage stable without catalog change");

    for (std::string_view id : builtin_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");
        expect_true(entry->id == id, "catalog id matches");
        expect_true(!entry->label.empty(), "catalog label non-empty");
    }

    const rt::SceneCatalogEntry* imported = rt::find_scene_catalog_entry("imported_obj_smoke");
    expect_true(imported != nullptr, "file-backed scene visible");
    expect_true(imported->supports_cpu_render, "file-backed scene cpu support");
    expect_true(imported->supports_realtime, "file-backed scene realtime support");

    expect_true(rt::find_scene_catalog_entry("final_room")->supports_realtime, "final_room realtime");
    expect_true(rt::find_scene_catalog_entry("cornell_box")->supports_realtime, "cornell_box realtime");
    expect_true(rt::find_scene_catalog_entry("quads")->supports_realtime, "quads realtime");
    expect_true(rt::find_scene_catalog_entry("quads")->supports_cpu_render, "quads cpu");
    expect_true(!rt::find_scene_catalog_entry("smoke")->supports_cpu_render, "smoke realtime-only");
    expect_true(rt::find_scene_catalog_entry("cornell_box_extreme") == nullptr,
        "extreme scene id removed from public catalog");
    return 0;
}
