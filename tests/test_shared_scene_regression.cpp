#include "core/offline_shared_scene_renderer.h"
#include "scene/scene_file_catalog.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "scene/yaml_scene_loader.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string_view>

namespace {

std::filesystem::path source_tree_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().lexically_normal();
}

std::filesystem::path scene_file_path(std::string_view scene_id) {
    return source_tree_root() / "assets" / "scenes" / scene_id / "scene.yaml";
}

const rt::scene::SceneDefinitionCpuRenderPreset* find_definition_cpu_preset(
    const rt::scene::SceneDefinition& definition, std::string_view preset_id) {
    const auto it = std::find_if(definition.cpu_presets.begin(), definition.cpu_presets.end(),
        [preset_id](const rt::scene::SceneDefinitionCpuRenderPreset& preset) { return preset.preset_id == preset_id; });
    return it == definition.cpu_presets.end() ? nullptr : &*it;
}

}  // namespace

int main() {
    rt::scene::SceneFileCatalog catalog;
    catalog.scan_directory(source_tree_root() / "assets" / "scenes");

    const rt::scene::SceneDefinition final_room_definition =
        rt::scene::load_scene_definition(scene_file_path("final_room"));
    expect_true(final_room_definition.metadata.id == "final_room", "final_room yaml id");
    expect_true(final_room_definition.scene_ir.shapes().size() == 17, "final_room yaml shape count");
    expect_true(final_room_definition.scene_ir.materials().size() == 5, "final_room yaml material count");
    expect_true(final_room_definition.cpu_presets.size() == 1, "final_room yaml cpu preset count");
    expect_true(final_room_definition.realtime_preset.has_value(), "final_room yaml realtime preset");
    const rt::scene::SceneDefinitionCpuRenderPreset* final_room_yaml_default =
        find_definition_cpu_preset(final_room_definition, "default");
    expect_true(final_room_yaml_default != nullptr, "final_room yaml default preset");
    expect_true(final_room_yaml_default->samples_per_pixel == 500, "final_room yaml default spp");
    expect_true(final_room_yaml_default->camera.camera.model == rt::CameraModelType::pinhole32,
        "final_room yaml cpu camera model");
    expect_true(final_room_yaml_default->camera.camera.fx > 0.0, "final_room yaml cpu camera fx");
    expect_vec3_near(final_room_yaml_default->camera.lookfrom, Eigen::Vector3d(13.0, 2.0, 3.0), 1e-12,
        "final_room yaml cpu camera lookfrom");
    expect_vec3_near(final_room_definition.realtime_preset->initial_body_pose.position, Eigen::Vector3d(0.0, -0.8, 0.35),
        1e-12, "final_room yaml realtime spawn");
    expect_true(final_room_definition.realtime_preset->camera.model == rt::CameraModelType::pinhole32,
        "final_room yaml realtime camera model");
    const rt::scene::SceneDefinition* final_room_catalog_definition = catalog.find_scene("final_room");
    expect_true(final_room_catalog_definition != nullptr, "final_room catalog definition");
    expect_true(std::find(final_room_catalog_definition->dependencies.begin(),
                    final_room_catalog_definition->dependencies.end(),
                    scene_file_path("final_room").lexically_normal().string()) !=
            final_room_catalog_definition->dependencies.end(),
        "final_room catalog overlays yaml scene file");
    const rt::scene::CpuRenderPreset* final_room_catalog_default = catalog.default_cpu_render_preset("final_room");
    expect_true(final_room_catalog_default != nullptr, "final_room catalog default preset");
    expect_true(final_room_catalog_default->samples_per_pixel == 500, "final_room catalog default spp");
    expect_true(final_room_catalog_default->camera.camera.model == rt::CameraModelType::pinhole32,
        "final_room catalog cpu camera model");
    expect_vec3_near(final_room_catalog_default->camera.lookfrom, Eigen::Vector3d(13.0, 2.0, 3.0), 1e-12,
        "final_room catalog cpu camera lookfrom");
    const rt::scene::RealtimeViewPreset* final_room_catalog_view = catalog.find_realtime_view_preset("final_room");
    expect_true(final_room_catalog_view != nullptr, "final_room catalog realtime preset");
    expect_vec3_near(final_room_catalog_view->initial_body_pose.position, Eigen::Vector3d(0.0, -0.8, 0.35), 1e-12,
        "final_room catalog realtime spawn");
    expect_vec3_near(rt::scene::scene_background("final_room"), Eigen::Vector3d::Zero(), 1e-12,
        "final_room production background");

    const rt::scene::SceneDefinition cornell_box_definition =
        rt::scene::load_scene_definition(scene_file_path("cornell_box"));
    expect_true(cornell_box_definition.metadata.id == "cornell_box", "cornell_box yaml id");
    expect_true(cornell_box_definition.scene_ir.shapes().size() == 8, "cornell_box yaml shape count");
    expect_true(cornell_box_definition.scene_ir.materials().size() == 4, "cornell_box yaml material count");
    expect_true(cornell_box_definition.cpu_presets.size() == 2, "cornell_box yaml cpu preset count");
    const rt::scene::SceneDefinitionCpuRenderPreset* cornell_box_yaml_default =
        find_definition_cpu_preset(cornell_box_definition, "default");
    const rt::scene::SceneDefinitionCpuRenderPreset* cornell_box_yaml_extreme =
        find_definition_cpu_preset(cornell_box_definition, "extreme");
    expect_true(cornell_box_yaml_default != nullptr, "cornell_box yaml default preset");
    expect_true(cornell_box_yaml_extreme != nullptr, "cornell_box yaml extreme preset");
    expect_true(cornell_box_yaml_default->samples_per_pixel == 1000, "cornell_box yaml default spp");
    expect_true(cornell_box_yaml_extreme->samples_per_pixel == 10000, "cornell_box yaml extreme spp");
    expect_true(cornell_box_yaml_default->camera.camera.model == rt::CameraModelType::pinhole32,
        "cornell_box yaml cpu camera model");
    expect_vec3_near(cornell_box_yaml_default->camera.lookfrom, Eigen::Vector3d(278.0, 278.0, -800.0), 1e-12,
        "cornell_box yaml cpu camera lookfrom");
    expect_true(cornell_box_definition.realtime_preset.has_value(), "cornell_box yaml realtime preset");
    expect_vec3_near(cornell_box_definition.realtime_preset->initial_body_pose.position,
        Eigen::Vector3d(278.0, 278.0, -120.0), 1e-12, "cornell_box yaml realtime spawn");
    const rt::scene::SceneDefinition* cornell_box_catalog_definition = catalog.find_scene("cornell_box");
    expect_true(cornell_box_catalog_definition != nullptr, "cornell_box catalog definition");
    expect_true(std::find(cornell_box_catalog_definition->dependencies.begin(),
                    cornell_box_catalog_definition->dependencies.end(),
                    scene_file_path("cornell_box").lexically_normal().string()) !=
            cornell_box_catalog_definition->dependencies.end(),
        "cornell_box catalog overlays yaml scene file");
    const rt::scene::CpuRenderPreset* cornell_box_catalog_default = catalog.find_cpu_render_preset("cornell_box", "default");
    const rt::scene::CpuRenderPreset* cornell_box_catalog_extreme = catalog.find_cpu_render_preset("cornell_box", "extreme");
    expect_true(cornell_box_catalog_default != nullptr, "cornell_box catalog default preset");
    expect_true(cornell_box_catalog_extreme != nullptr, "cornell_box catalog extreme preset");
    expect_true(cornell_box_catalog_default->samples_per_pixel == 1000, "cornell_box catalog default spp");
    expect_true(cornell_box_catalog_extreme->samples_per_pixel == 10000, "cornell_box catalog extreme spp");
    expect_true(cornell_box_catalog_default->camera.camera.fx == cornell_box_yaml_default->camera.camera.fx,
        "cornell_box catalog cpu camera fx preserved");
    expect_vec3_near(cornell_box_catalog_default->camera.lookfrom, Eigen::Vector3d(278.0, 278.0, -800.0), 1e-12,
        "cornell_box catalog cpu camera lookfrom");
    const rt::scene::RealtimeViewPreset* cornell_box_catalog_view = catalog.find_realtime_view_preset("cornell_box");
    expect_true(cornell_box_catalog_view != nullptr, "cornell_box catalog realtime preset");
    expect_vec3_near(cornell_box_catalog_view->initial_body_pose.position, Eigen::Vector3d(278.0, 278.0, -120.0), 1e-12,
        "cornell_box catalog realtime spawn");
    expect_vec3_near(rt::scene::scene_background("cornell_box"), Eigen::Vector3d::Zero(), 1e-12,
        "cornell_box production background");

    const rt::scene::SceneDefinition simple_light_definition =
        rt::scene::load_scene_definition(scene_file_path("simple_light"));
    expect_true(simple_light_definition.metadata.id == "simple_light", "simple_light yaml id");
    expect_true(simple_light_definition.scene_ir.shapes().size() == 4, "simple_light yaml shape count");
    expect_true(simple_light_definition.scene_ir.materials().size() == 2, "simple_light yaml material count");
    expect_true(simple_light_definition.cpu_presets.size() == 1, "simple_light yaml cpu preset count");
    expect_true(simple_light_definition.realtime_preset.has_value(), "simple_light yaml realtime preset");
    const rt::scene::SceneDefinitionCpuRenderPreset* simple_light_yaml_default =
        find_definition_cpu_preset(simple_light_definition, "default");
    expect_true(simple_light_yaml_default != nullptr, "simple_light yaml default preset");
    expect_true(simple_light_yaml_default->samples_per_pixel == 500, "simple_light yaml default spp");
    expect_true(simple_light_yaml_default->camera.camera.model == rt::CameraModelType::pinhole32,
        "simple_light yaml cpu camera model");
    expect_vec3_near(simple_light_yaml_default->camera.lookfrom, Eigen::Vector3d(26.0, 3.0, 6.0), 1e-12,
        "simple_light yaml cpu camera lookfrom");
    expect_vec3_near(simple_light_definition.realtime_preset->initial_body_pose.position, Eigen::Vector3d(10.0, 3.0, 6.0),
        1e-12, "simple_light yaml realtime spawn");
    const rt::scene::SceneDefinition* simple_light_catalog_definition = catalog.find_scene("simple_light");
    expect_true(simple_light_catalog_definition != nullptr, "simple_light catalog definition");
    expect_true(std::find(simple_light_catalog_definition->dependencies.begin(),
                    simple_light_catalog_definition->dependencies.end(),
                    scene_file_path("simple_light").lexically_normal().string()) !=
            simple_light_catalog_definition->dependencies.end(),
        "simple_light catalog overlays yaml scene file");
    const rt::scene::CpuRenderPreset* simple_light_catalog_default = catalog.default_cpu_render_preset("simple_light");
    expect_true(simple_light_catalog_default != nullptr, "simple_light catalog default preset");
    expect_true(simple_light_catalog_default->samples_per_pixel == 500, "simple_light catalog default spp");
    expect_true(simple_light_catalog_default->camera.camera.fx == simple_light_yaml_default->camera.camera.fx,
        "simple_light catalog cpu camera fx preserved");
    expect_vec3_near(simple_light_catalog_default->camera.lookfrom, Eigen::Vector3d(26.0, 3.0, 6.0), 1e-12,
        "simple_light catalog cpu camera lookfrom");
    const rt::scene::RealtimeViewPreset* simple_light_catalog_view = catalog.find_realtime_view_preset("simple_light");
    expect_true(simple_light_catalog_view != nullptr, "simple_light catalog realtime preset");
    expect_vec3_near(simple_light_catalog_view->initial_body_pose.position, Eigen::Vector3d(10.0, 3.0, 6.0), 1e-12,
        "simple_light catalog realtime spawn");
    expect_vec3_near(rt::scene::scene_background("simple_light"), Eigen::Vector3d::Zero(), 1e-12,
        "simple_light production background");

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
