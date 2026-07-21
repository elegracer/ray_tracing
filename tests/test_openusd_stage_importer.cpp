#include "scene/openusd_stage_importer.h"
#include "test_support.h"

#include <algorithm>
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

const rt::scene::SceneMaterialConnection& require_connection(
    const rt::scene::SceneOpenPbrSurface& surface, std::string_view input_name) {
    for (const rt::scene::SceneMaterialConnection& connection : surface.connections) {
        if (connection.input_name == input_name) {
            return connection;
        }
    }
    throw std::runtime_error("missing expected OpenPBR connection: " + std::string {input_name});
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

    const rt::scene::ScenePrim& triangle = require_prim(scene, "/World/Triangle");
    const rt::scene::ScenePrim& triangle_prototype = require_prim(scene, triangle.prototype_path);
    const auto& mesh = std::get<rt::scene::SceneMeshGeometry>(*triangle_prototype.geometry);
    expect_true(mesh.points.size() == 3 && mesh.face_vertex_counts == std::vector<std::int32_t> {3}
                    && mesh.face_vertex_indices == std::vector<std::int32_t> {0, 1, 2},
        "UsdGeom mesh topology");
    expect_vec3_near(mesh.points[1], {2.0, 0.0, 0.0}, 1e-12, "UsdGeom mesh points");
    expect_true(mesh.orientation == rt::scene::SceneMeshOrientation::left_handed
                    && mesh.subdivision_scheme == rt::scene::SceneSubdivisionScheme::none,
        "UsdGeom orientation and subdivision metadata");
    expect_true(triangle.material_path == "/World/Looks/Red",
        "mesh inherits the resolved material binding");

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
        fixture.parent_path() / "connected_material.usda";
    const rt::scene::SceneIRv2 connected_scene = rt::scene::import_openusd_stage(connected_fixture);
    const auto& connected_surface = std::get<rt::scene::SceneOpenPbrSurface>(
        *require_prim(connected_scene, "/World/Material").material);
    expect_true(connected_surface.connections.size() == 6,
        "supported OpenPBR color and scalar connections compile");

    const rt::scene::ScenePrim& metalness = require_prim(connected_scene,
        require_connection(connected_surface, "base_metalness").texture_path);
    expect_true(metalness.texture->node == rt::scene::SceneTextureNode::constant_color
                    && metalness.texture->node_definition == "ND_constant_float"
                    && metalness.texture->output_type == rt::scene::SceneMaterialValueType::float_
                    && metalness.texture->color_space == rt::scene::SceneColorSpace::raw,
        "MaterialX constant float node contract");
    expect_vec3_near(metalness.texture->value, Eigen::Vector3d::Constant(0.7), 1e-6,
        "MaterialX constant float value is replicated for runtime projection");

    const rt::scene::ScenePrim& roughness = require_prim(connected_scene,
        require_connection(connected_surface, "specular_roughness").texture_path);
    expect_true(roughness.texture->node == rt::scene::SceneTextureNode::image
                    && roughness.texture->node_definition == "ND_image_float"
                    && roughness.texture->output_type == rt::scene::SceneMaterialValueType::float_
                    && roughness.texture->color_space == rt::scene::SceneColorSpace::raw,
        "MaterialX float image node contract");
    expect_vec3_near(roughness.texture->value, Eigen::Vector3d::Constant(0.35), 1e-6,
        "MaterialX float image fallback is replicated for runtime projection");

    const rt::scene::ScenePrim& constant = require_prim(connected_scene,
        require_connection(connected_surface, "specular_color").texture_path);
    expect_true(constant.texture->node == rt::scene::SceneTextureNode::constant_color,
        "MaterialX constant node type");
    expect_vec3_near(constant.texture->value, {0.2, 0.4, 0.8}, 1e-6, "MaterialX constant value");

    const rt::scene::ScenePrim& checker = require_prim(connected_scene,
        require_connection(connected_surface, "base_color").texture_path);
    expect_true(checker.texture->node == rt::scene::SceneTextureNode::checkerboard,
        "MaterialX checkerboard node type");
    expect_near(checker.texture->scale, 4.0, 1e-12, "checkerboard uniform UV tiling");
    expect_true(checker.texture->even_texture_path == constant.path,
        "checkerboard reuses the connected constant node");
    const rt::scene::ScenePrim& checker_literal =
        require_prim(connected_scene, checker.texture->odd_texture_path);
    expect_vec3_near(checker_literal.texture->value, {0.05, 0.1, 0.15}, 1e-6,
        "checkerboard literal compiles to a stable constant texture");

    const rt::scene::ScenePrim& noise = require_prim(connected_scene,
        require_connection(connected_surface, "transmission_color").texture_path);
    expect_true(noise.texture->node == rt::scene::SceneTextureNode::noise3d,
        "MaterialX noise3d node type");

    const rt::scene::ScenePrim& image = require_prim(connected_scene,
        require_connection(connected_surface, "emission_color").texture_path);
    expect_true(image.texture->node == rt::scene::SceneTextureNode::image,
        "MaterialX image node type");
    expect_true(image.texture->color_space == rt::scene::SceneColorSpace::srgb_texture,
        "image colorSpace metadata");
    expect_true(image.texture->u_address_mode == rt::scene::SceneTextureAddressMode::clamp
                    && image.texture->v_address_mode == rt::scene::SceneTextureAddressMode::mirror
                    && image.texture->filter_type == rt::scene::SceneTextureFilterType::cubic,
        "image address and filter semantics");
    expect_true(image.asset_references.size() == 1
                    && image.asset_references[0].authored_path == "textures/emission.png",
        "image authored asset identity");

    const std::filesystem::path preview_fixture =
        fixture.parent_path() / "preview_surface_subset.usda";
    const rt::scene::SceneIRv2 preview_scene = rt::scene::import_openusd_stage(preview_fixture);
    rt::scene::require_valid_scene_ir_v2(preview_scene);
    const rt::scene::ScenePrim& panel = require_prim(preview_scene, "/World/Panel");
    const auto& panel_mesh = std::get<rt::scene::SceneMeshGeometry>(
        *require_prim(preview_scene, panel.prototype_path).geometry);
    expect_true(panel_mesh.material_subset_family_type
                        == rt::scene::SceneMaterialSubsetFamilyType::partition
                    && panel_mesh.material_subsets.size() == 1
                    && panel_mesh.material_subsets[0].face_indices
                           == std::vector<std::int32_t> {0, 1}
                    && panel_mesh.material_subsets[0].material_path == "/World/Looks/Paint",
        "materialBind GeomSubset compiles into the parent mesh prototype");
    expect_true(panel_mesh.primvars.size() == 3,
        "authored display color, UV, and normal primvars compile without fallback");
    const auto normals = std::find_if(panel_mesh.primvars.begin(), panel_mesh.primvars.end(),
        [](const rt::scene::ScenePrimvar& primvar) { return primvar.name == "normals"; });
    expect_true(normals != panel_mesh.primvars.end()
                    && normals->role == rt::scene::ScenePrimvarRole::normal,
        "float3 normals from DCC-authored USD retain normal semantics");

    const auto& preview_surface = std::get<rt::scene::SceneOpenPbrSurface>(
        *require_prim(preview_scene, "/World/Looks/Paint").material);
    expect_near(preview_surface.base_metalness, 0.2, 1e-6,
        "UsdPreviewSurface metallic maps to OpenPBR base metalness");
    expect_near(preview_surface.specular_roughness, 0.35, 1e-6,
        "UsdPreviewSurface roughness maps to OpenPBR specular roughness");
    const rt::scene::ScenePrim& preview_texture =
        require_prim(preview_scene, require_connection(preview_surface, "base_color").texture_path);
    expect_true(preview_texture.texture->node == rt::scene::SceneTextureNode::image
                    && preview_texture.texture->node_definition == "ND_image_color3"
                    && preview_texture.texture->color_space
                           == rt::scene::SceneColorSpace::srgb_texture
                    && preview_texture.asset_references.size() == 1
                    && preview_texture.asset_references[0].authored_path == "paint.png",
        "UsdPreviewSurface diffuseColor maps its UsdUVTexture without silent fallback");

    const std::filesystem::path unsupported_fixture =
        fixture.parent_path() / "unsupported_connected_material.usda";
    try {
        static_cast<void>(rt::scene::import_openusd_stage(unsupported_fixture));
    } catch (const std::invalid_argument& error) {
        expect_true(std::string {error.what()}.find("unsupported connected MaterialX shader")
                        != std::string::npos,
            "unknown connected shader node fails explicitly");
        return 0;
    }
    throw std::runtime_error("unsupported connected MaterialX node was silently accepted");
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
