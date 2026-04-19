#include "realtime/scene_catalog.h"
#include "scene/shared_scene_builders.h"
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

    const auto& catalog = rt::scene_catalog();
    expect_true(catalog.size() == expected_ids.size(), "catalog scene count");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");

        const rt::scene::SceneIR scene = rt::scene::build_scene(id);
        expect_true(!scene.materials().empty(), "scene has materials");
        expect_true(!scene.shapes().empty(), "scene has shapes");
    }

    expect_true(rt::scene::find_scene_metadata("cornell_box_extreme") == nullptr,
        "duplicate cornell_box_extreme removed from public metadata");
    expect_true(rt::scene::find_scene_metadata("unknown") == nullptr, "unknown metadata");

    const rt::scene::CpuRenderPreset* cornell_default =
        rt::scene::find_cpu_render_preset("cornell_box", "default");
    const rt::scene::CpuRenderPreset* cornell_extreme =
        rt::scene::find_cpu_render_preset("cornell_box", "extreme");
    expect_true(cornell_default != nullptr, "cornell_box default CPU preset exists");
    expect_true(cornell_extreme != nullptr, "cornell_box extreme CPU preset exists");
    expect_true(cornell_default->samples_per_pixel == 1000, "cornell_box default spp preserved");
    expect_true(cornell_extreme->samples_per_pixel == 10000, "cornell_box extreme spp preserved");

    const rt::scene::RealtimeViewPreset* final_room_view =
        rt::scene::find_realtime_view_preset("final_room");
    expect_true(final_room_view != nullptr, "final_room realtime preset exists");
    expect_true(final_room_view->base_move_speed > 0.0, "final_room move speed is positive");

    bool build_unknown_threw = false;
    try {
        (void)rt::scene::build_scene("unknown");
    } catch (...) {
        build_unknown_threw = true;
    }
    expect_true(build_unknown_threw, "unknown build throws");

    bool cpu_preset_unknown_threw = false;
    try {
        (void)rt::scene::default_cpu_render_preset("unknown");
    } catch (...) {
        cpu_preset_unknown_threw = true;
    }
    expect_true(cpu_preset_unknown_threw, "unknown cpu preset throws");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists for capability");
        expect_true(entry->supports_realtime, "catalog scene supports realtime");
    }

    expect_true(!rt::find_scene_catalog_entry("smoke")->supports_cpu_render, "smoke remains realtime-only");
    expect_true(rt::find_scene_catalog_entry("final_room")->supports_cpu_render, "final_room supports cpu reference");
    return 0;
}
