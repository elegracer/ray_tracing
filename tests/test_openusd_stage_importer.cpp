#include "scene/openusd_stage_importer.h"
#include "test_support.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

void expect_translation(const Eigen::Matrix4d& transform, const Eigen::Vector3d& expected,
    std::string_view message) {
    expect_vec3_near(transform.block<3, 1>(0, 3), expected, 1e-12, std::string {message});
}

const rt::scene::ScenePrim& require_prim(const rt::scene::SceneIRv2& scene, std::string_view path) {
    const rt::scene::ScenePrim* prim = scene.find_prim(path);
    expect_true(prim != nullptr, "missing expected SceneIR v2 prim: " + std::string {path});
    return *prim;
}

} // namespace

int main(int argc, char** argv) {
    expect_true(argc == 2, "OpenUSD importer test requires one fixture path");
    const std::filesystem::path fixture = argv[1];

#if RT_HAS_OPENUSD
    expect_true(rt::scene::openusd_stage_importer_available(), "OpenUSD importer capability");
    const rt::scene::SceneIRv2 scene = rt::scene::import_openusd_stage(fixture);
    rt::scene::require_valid_scene_ir_v2(scene);

    const rt::scene::SceneStageMetadata& stage = scene.stage_metadata();
    expect_near(stage.meters_per_unit, 1.0, 1e-12, "composed stage meters per unit");
    expect_true(stage.up_axis == rt::scene::SceneUpAxis::z, "composed stage up axis");
    expect_near(stage.time_codes_per_second, 48.0, 1e-12, "stage time rate");
    expect_near(stage.frames_per_second, 24.0, 1e-12, "stage frame rate");
    expect_true(stage.start_time_code == 1.0 && stage.end_time_code == 24.0,
        "authored stage time range");
    expect_true(stage.interpolation == rt::scene::SceneTimeInterpolation::linear,
        "stage interpolation policy");
    expect_true(stage.default_prim_path == "/World", "composed stage default prim");

    const rt::scene::ScenePrim& world = require_prim(scene, "/World");
    expect_true(world.reset_xform_stack, "root reset xform stack");
    expect_true(world.transform_samples.size() == 2, "root transform samples");
    expect_near(world.transform_samples[0].time_code, 1.0, 1e-12, "first sample time");
    expect_near(world.transform_samples[1].time_code, 24.0, 1e-12, "last sample time");
    expect_translation(world.transform_samples[0].local_to_parent, {1.0, 2.0, 3.0},
        "first sampled translation");
    expect_translation(world.transform_samples[1].local_to_parent, {4.0, 5.0, 6.0},
        "last sampled translation");

    const rt::scene::ScenePrim& referenced = require_prim(scene, "/World/Referenced");
    expect_translation(referenced.local_to_parent, {7.0, 8.0, 9.0},
        "USD row-vector transform converts to Eigen column-vector convention");
    expect_true(referenced.visibility == rt::scene::SceneVisibility::invisible,
        "authored visibility is preserved");
    expect_true(referenced.authored_purpose == rt::scene::ScenePurpose::proxy,
        "authored purpose is preserved");
    static_cast<void>(require_prim(scene, "/World/Local"));

    const rt::scene::ScenePrim& local_ball = require_prim(scene, "/World/Ball");
    const rt::scene::ScenePrim& instance_a_ball = require_prim(scene, "/World/InstanceA/Ball");
    const rt::scene::ScenePrim& instance_b_ball = require_prim(scene, "/World/InstanceB/Ball");
    expect_true(local_ball.kind == rt::scene::ScenePrimKind::surface,
        "composed sphere compiles as a surface instance");
    expect_true(instance_a_ball.prototype_path == instance_b_ball.prototype_path,
        "shared OpenUSD prototype compiles to one renderer prototype");
    expect_true(instance_a_ball.prototype_path.find("__Prototype") == std::string::npos,
        "unstable OpenUSD prototype paths never enter SceneIR v2");
    expect_true(instance_a_ball.material_path == "/World/Looks/Red"
                    && instance_b_ball.material_path == "/World/Looks/Red",
        "inherited material binding resolves through instance proxies");

    const rt::scene::ScenePrim& instance_prototype =
        require_prim(scene, instance_a_ball.prototype_path);
    expect_true(instance_prototype.kind == rt::scene::ScenePrimKind::geometry_prototype,
        "instance prototype kind");
    const auto& sphere = std::get<rt::scene::SceneSphereGeometry>(*instance_prototype.geometry);
    expect_near(sphere.radius, 2.0, 1e-12, "referenced sphere radius");
    expect_translation(require_prim(scene, "/World/InstanceA").local_to_parent, {10.0, 0.0, 0.0},
        "first instance transform");
    expect_translation(require_prim(scene, "/World/InstanceB").local_to_parent, {20.0, 0.0, 0.0},
        "second instance transform");

    const rt::scene::ScenePrim& material = require_prim(scene, "/World/Looks/Red");
    expect_true(material.kind == rt::scene::ScenePrimKind::material,
        "UsdShade material compiles as a SceneIR material");
    const auto& openpbr = std::get<rt::scene::SceneOpenPbrSurface>(*material.material);
    expect_vec3_near(openpbr.base_color, {0.8, 0.2, 0.1}, 1e-6, "OpenPBR constant base color");
    expect_near(openpbr.base_metalness, 0.25, 1e-6, "OpenPBR constant metalness");
    expect_near(openpbr.specular_roughness, 0.4, 1e-6, "OpenPBR constant roughness");

    const rt::scene::ScenePrim& camera = require_prim(scene, "/World/RenderCamera");
    expect_true(camera.kind == rt::scene::ScenePrimKind::camera && camera.camera.has_value(),
        "UsdGeomCamera payload");
    expect_near(camera.camera->focal_length, 35.0, 1e-6, "camera focal length");
    expect_near(camera.camera->horizontal_aperture, 24.0, 1e-6, "camera horizontal aperture");
    expect_near(camera.camera->clipping_range.x(), 0.1, 1e-6, "camera near clip");
    expect_near(camera.camera->clipping_range.y(), 500.0, 1e-6, "camera far clip");
    expect_near(camera.camera->f_stop, 2.8, 1e-6, "camera f-stop");
    expect_near(camera.camera->focus_distance, 12.0, 1e-6, "camera focus distance");
    expect_translation(camera.local_to_parent, {3.0, 4.0, 5.0}, "camera local transform");

    const rt::scene::ScenePrim& sphere_key = require_prim(scene, "/World/SphereKey");
    expect_true(sphere_key.light->type == rt::scene::SceneLightType::sphere,
        "UsdLux sphere light type");
    expect_vec3_near(sphere_key.light->color, {1.0, 0.5, 0.25}, 1e-6, "UsdLux light color");
    expect_near(sphere_key.light->intensity, 8.0, 1e-6, "UsdLux light intensity");
    expect_near(sphere_key.light->exposure, 1.0, 1e-6, "UsdLux light exposure");
    expect_true(sphere_key.light->normalize && sphere_key.light->treat_as_point,
        "UsdLux sphere normalization and point semantics");
    expect_true(sphere_key.light->material_sync_mode
                    == rt::scene::SceneLightMaterialSyncMode::independent,
        "UsdLux material-sync mode");
    expect_true(require_prim(scene, "/World/DiskFill").light->type
                    == rt::scene::SceneLightType::disk,
        "UsdLux disk light type");
    expect_true(require_prim(scene, "/World/RectPanel").light->type
                    == rt::scene::SceneLightType::rect,
        "UsdLux rect light type");
    expect_true(require_prim(scene, "/World/Tube").light->type
                    == rt::scene::SceneLightType::cylinder,
        "UsdLux cylinder light type");
    expect_true(require_prim(scene, "/World/Sun").light->type == rt::scene::SceneLightType::distant,
        "UsdLux distant light type");
    const rt::scene::ScenePrim& environment = require_prim(scene, "/World/Environment");
    expect_true(environment.light->type == rt::scene::SceneLightType::dome,
        "UsdLux dome light type");
    expect_true(environment.asset_references.size() == 1
                    && environment.asset_references[0].authored_path == "environment.exr"
                    && environment.asset_references[0].evaluated_path == "environment.exr",
        "UsdLux texture asset preserves authored and evaluated identity");

    const std::filesystem::path connected_fixture =
        fixture.parent_path() / "unsupported_connected_material.usda";
    try {
        static_cast<void>(rt::scene::import_openusd_stage(connected_fixture));
    } catch (const std::invalid_argument& error) {
        expect_true(std::string {error.what()}.find("connected OpenPBR inputs")
                        != std::string::npos,
            "unsupported connected shader input fails explicitly");
        return 0;
    }
    throw std::runtime_error("connected OpenPBR input was silently accepted");
#else
    expect_true(!rt::scene::openusd_stage_importer_available(), "disabled OpenUSD capability");
    try {
        static_cast<void>(rt::scene::import_openusd_stage(fixture));
    } catch (const std::runtime_error& error) {
        expect_true(std::string {error.what()}.find("RT_ENABLE_OPENUSD=ON") != std::string::npos,
            "disabled importer reports the exact build capability");
        return 0;
    }
    throw std::runtime_error("disabled OpenUSD importer unexpectedly accepted a stage");
#endif
    return 0;
}
