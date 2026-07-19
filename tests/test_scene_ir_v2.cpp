#include "scene/scene_ir_v2.h"
#include "test_support.h"

#include <Eigen/Core>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

Eigen::Matrix4d translated(double x, double y, double z) {
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
    transform.block<3, 1>(0, 3) = Eigen::Vector3d {x, y, z};
    return transform;
}

rt::scene::SceneIRv2 semantic_scene() {
    rt::scene::SceneIRv2 scene;
    scene.stage_metadata().meters_per_unit = 0.01;
    scene.stage_metadata().up_axis = rt::scene::SceneUpAxis::z;
    scene.stage_metadata().time_codes_per_second = 48.0;
    scene.stage_metadata().frames_per_second = 24.0;
    scene.stage_metadata().start_time_code = 0.0;
    scene.stage_metadata().end_time_code = 10.0;

    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World",
        .local_to_parent = translated(1.0, 0.0, 0.0),
    });

    Eigen::Matrix4d affine = translated(0.0, 2.0, 0.0);
    affine(0, 0) = 2.0;
    affine(0, 1) = 0.25;
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Group",
        .local_to_parent = affine,
        .visibility = rt::scene::SceneVisibility::invisible,
        .authored_purpose = rt::scene::ScenePurpose::proxy,
    });
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Group/Child",
        .local_to_parent = translated(0.0, 0.0, 3.0),
    });
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Reset",
        .local_to_parent = translated(0.0, 4.0, 0.0),
        .reset_xform_stack = true,
    });
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Animated",
        .transform_samples =
            {
                {.time_code = 0.0, .local_to_parent = translated(0.0, 0.0, 0.0)},
                {.time_code = 10.0, .local_to_parent = translated(10.0, 0.0, 0.0)},
            },
    });
    return scene;
}

rt::scene::SceneMeshGeometry valid_mesh_geometry(
    rt::scene::SceneSubdivisionScheme subdivision_scheme =
        rt::scene::SceneSubdivisionScheme::none) {
    rt::scene::SceneMeshGeometry mesh;
    mesh.points = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0},
        {3.0, 1.0, 0.0},
        {2.0, 1.0, 0.0},
    };
    mesh.face_vertex_counts = {3, 4};
    mesh.face_vertex_indices = {0, 1, 2, 3, 4, 5, 6};
    mesh.orientation = rt::scene::SceneMeshOrientation::right_handed;
    mesh.subdivision_scheme = subdivision_scheme;
    mesh.primvars = {
        rt::scene::ScenePrimvar {
            .name = "normals",
            .interpolation = rt::scene::ScenePrimvarInterpolation::face_varying,
            .role = rt::scene::ScenePrimvarRole::normal,
            .values =
                std::vector<Eigen::Vector3d> {
                    {0.0, 0.0, 1.0},
                    {0.0, 0.0, 1.0},
                },
            .indices = {0, 0, 0, 1, 1, 1, 1},
        },
        rt::scene::ScenePrimvar {
            .name = "st",
            .interpolation = rt::scene::ScenePrimvarInterpolation::face_varying,
            .role = rt::scene::ScenePrimvarRole::texcoord,
            .values =
                std::vector<Eigen::Vector2d> {
                    {0.0, 0.0},
                    {1.0, 0.0},
                    {0.0, 1.0},
                    {1.0, 1.0},
                },
            .indices = {0, 1, 2, 0, 1, 3, 2},
        },
        rt::scene::ScenePrimvar {
            .name = "tangents",
            .interpolation = rt::scene::ScenePrimvarInterpolation::vertex,
            .role = rt::scene::ScenePrimvarRole::vector,
            .values = std::vector<Eigen::Vector3d>(7, Eigen::Vector3d {1.0, 0.0, 0.0}),
        },
        rt::scene::ScenePrimvar {
            .name = "displayColor",
            .interpolation = rt::scene::ScenePrimvarInterpolation::uniform,
            .role = rt::scene::ScenePrimvarRole::color,
            .values =
                std::vector<Eigen::Vector3f> {
                    {1.0F, 0.0F, 0.0F},
                    {0.0F, 1.0F, 0.0F},
                },
        },
    };
    mesh.material_subset_family_type = rt::scene::SceneMaterialSubsetFamilyType::partition;
    mesh.material_subsets = {
        {.name = "matteFaces", .face_indices = {0}, .material_path = "/World/Materials/Matte"},
        {.name = "metalFaces", .face_indices = {1}, .material_path = "/World/Materials/Metal"},
    };
    return mesh;
}

rt::scene::SceneIRv2 scene_with_mesh(rt::scene::SceneMeshGeometry mesh) {
    rt::scene::SceneIRv2 scene;
    scene.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Matte",
        .kind = rt::scene::ScenePrimKind::material,
        .material = rt::scene::SceneMaterial {rt::scene::SceneOpenPbrSurface {}},
    });
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Metal",
        .kind = rt::scene::ScenePrimKind::material,
        .material = rt::scene::SceneMaterial {rt::scene::SceneOpenPbrSurface {}},
    });
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Prototypes"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Prototypes/Mesh",
        .kind = rt::scene::ScenePrimKind::geometry_prototype,
        .geometry = std::move(mesh),
    });
    return scene;
}

rt::scene::SceneIRv2 scene_with_camera(rt::scene::SceneCamera camera) {
    rt::scene::SceneIRv2 scene;
    scene.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Cameras"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Cameras/Main",
        .kind = rt::scene::ScenePrimKind::camera,
        .camera = std::move(camera),
    });
    return scene;
}

rt::scene::SceneIRv2 scene_with_light(rt::scene::SceneLight light) {
    rt::scene::SceneIRv2 scene;
    scene.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Lights"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Lights/Key",
        .kind = rt::scene::ScenePrimKind::light,
        .light = std::move(light),
    });
    return scene;
}

rt::scene::SceneIRv2 scene_with_open_pbr_material(rt::scene::SceneOpenPbrSurface material) {
    rt::scene::SceneIRv2 scene;
    scene.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Test",
        .kind = rt::scene::ScenePrimKind::material,
        .material = rt::scene::SceneMaterial {std::move(material)},
    });
    return scene;
}

rt::scene::SceneCamera calibrated_camera() {
    rt::scene::SceneCamera camera;
    camera.horizontal_aperture = 20.955;
    camera.vertical_aperture = 11.7871875;
    camera.focal_length = 16.2;
    camera.clipping_range = Eigen::Vector2d {0.1, 10'000.0};
    camera.f_stop = 2.8;
    camera.focus_distance = 5.0;
    camera.renderer_calibration = rt::scene::SceneCameraCalibration {
        .model = rt::scene::SceneCameraCalibrationModel::equi62_lut1d,
        .image_width = 640,
        .image_height = 360,
        .focal_length_pixels = Eigen::Vector2d {494.5, 494.5},
        .principal_point_pixels = Eigen::Vector2d {320.0, 180.0},
        .radial_distortion = {0.01, -0.001, 0.0, 0.0, 0.0, 0.0},
        .tangential_distortion = Eigen::Vector2d {0.001, -0.002},
    };
    return camera;
}

rt::scene::SceneIR legacy_scene() {
    rt::scene::SceneIR scene;
    const int white =
        scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte = scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int volume =
        scene.add_material(rt::scene::IsotropicVolumeMaterial {.albedo_texture = white});
    const int sphere = scene.add_shape(rt::scene::SphereShape {.radius = 1.0});
    const int mesh = scene.add_shape(rt::scene::TriangleMeshShape {
        .positions =
            {
                {0.0, 0.0, 0.0},
                {1.0, 0.0, 0.0},
                {0.0, 1.0, 0.0},
            },
        .triangles = {{0, 1, 2}},
        .normals = {{0.0, 0.0, 1.0}},
        .normal_indices = {{0, 0, 0}},
        .texcoords =
            {
                {0.0, 0.0},
                {1.0, 0.0},
                {0.0, 1.0},
            },
        .texcoord_indices = {{0, 1, 2}},
    });

    rt::scene::Transform surface_transform = rt::scene::Transform::identity();
    surface_transform.translation = Eigen::Vector3d {1.0, 2.0, 3.0};
    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = mesh,
        .material_index = matte,
        .transform = surface_transform,
    });
    scene.add_medium(rt::scene::MediumInstance {
        .shape_index = sphere,
        .material_index = volume,
        .density = 0.5,
    });
    return scene;
}

rt::scene::SceneIR legacy_emissive_scene() {
    rt::scene::SceneIR scene;
    const int emission = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {4.0, 3.0, 2.0},
    });
    const int light =
        scene.add_material(rt::scene::EmissiveMaterial {.emission_texture = emission});
    const int quad = scene.add_shape(rt::scene::QuadShape {
        .edge_u = Eigen::Vector3d {2.0, 0.0, 0.0},
        .edge_v = Eigen::Vector3d {0.0, 3.0, 0.0},
    });
    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = quad,
        .material_index = light,
    });
    return scene;
}

rt::scene::SceneIR legacy_material_texture_scene() {
    rt::scene::SceneIR scene;
    const int red = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {0.8, 0.1, 0.05},
    });
    const int blue = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {0.05, 0.1, 0.8},
    });
    const int checker = scene.add_texture(rt::scene::CheckerTextureDesc {
        .scale = 2.0,
        .even_texture = red,
        .odd_texture = blue,
    });
    const int image = scene.add_texture(rt::scene::ImageTextureDesc {
        .authored_path = "textures/emission.exr",
        .path = "/resolved/textures/emission.exr",
    });
    const int noise = scene.add_texture(rt::scene::NoiseTextureDesc {.scale = 4.0});

    scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = red});
    scene.add_material(rt::scene::MetalMaterial {
        .albedo_texture = checker,
        .fuzz = 0.25,
    });
    scene.add_material(rt::scene::DielectricMaterial {.ior = 1.52});
    scene.add_material(rt::scene::EmissiveMaterial {.emission_texture = image});
    scene.add_material(rt::scene::IsotropicVolumeMaterial {.albedo_texture = noise});
    return scene;
}

void expect_invalid(const rt::scene::SceneIRv2& scene, std::string_view code) {
    const std::vector<rt::scene::SceneDiagnostic> diagnostics =
        rt::scene::validate_scene_ir_v2(scene);
    expect_true(rt::scene::has_scene_diagnostic(diagnostics, code),
        "missing diagnostic " + std::string {code});
    try {
        rt::scene::require_valid_scene_ir_v2(scene);
    } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid SceneIR v2 was accepted: " + std::string {code});
}

void test_stage_hierarchy_and_sampling() {
    const rt::scene::SceneIRv2 scene = semantic_scene();
    rt::scene::require_valid_scene_ir_v2(scene);
    expect_true(scene.stage_metadata().up_axis == rt::scene::SceneUpAxis::z, "stage up axis");
    expect_near(scene.stage_metadata().meters_per_unit, 0.01, 1e-12, "stage meters per unit");
    expect_true(scene.find_prim("/World/Group/Child") != nullptr, "stable prim lookup");
    expect_true(rt::scene::parent_scene_prim_path("/World/Group/Child") == "/World/Group",
        "prim parent");

    const Eigen::Matrix4d child_world =
        rt::scene::compute_scene_world_transform(scene, "/World/Group/Child", 0.0);
    expect_vec3_near(child_world.block<3, 1>(0, 3), Eigen::Vector3d {1.0, 2.0, 3.0}, 1e-12,
        "hierarchical world transform");
    const Eigen::Matrix4d reset_world =
        rt::scene::compute_scene_world_transform(scene, "/World/Reset", 0.0);
    expect_vec3_near(reset_world.block<3, 1>(0, 3), Eigen::Vector3d {0.0, 4.0, 0.0}, 1e-12,
        "reset transform stack");
    expect_true(!rt::scene::compute_scene_visibility(scene, "/World/Group/Child"),
        "inherited invisibility");
    expect_true(rt::scene::compute_scene_purpose(scene, "/World/Group/Child")
                    == rt::scene::ScenePurpose::proxy,
        "inherited purpose");

    const rt::scene::ScenePrim& animated = *scene.find_prim("/World/Animated");
    const Eigen::Matrix4d linear = rt::scene::sample_scene_local_transform(animated, 5.0,
        rt::scene::SceneTimeInterpolation::linear);
    const Eigen::Matrix4d held = rt::scene::sample_scene_local_transform(animated, 5.0,
        rt::scene::SceneTimeInterpolation::held);
    expect_near(linear(0, 3), 5.0, 1e-12, "linear transform sample");
    expect_near(held(0, 3), 0.0, 1e-12, "held transform sample");
}

void test_mesh_geometry_contract() {
    const rt::scene::SceneIRv2 scene = scene_with_mesh(valid_mesh_geometry());
    const std::vector<rt::scene::SceneDiagnostic> diagnostics =
        rt::scene::validate_scene_ir_v2(scene);
    expect_true(diagnostics.empty(), "valid mesh contract diagnostics");
    rt::scene::require_valid_scene_ir_v2(scene);

    const rt::scene::ScenePrim& prim = *scene.find_prim("/World/Prototypes/Mesh");
    const auto& mesh = std::get<rt::scene::SceneMeshGeometry>(*prim.geometry);
    expect_true(mesh.face_vertex_counts == std::vector<std::int32_t>({3, 4}),
        "mesh preserves triangle and ngon topology");
    expect_true(mesh.face_vertex_indices.size() == 7, "mesh face-vertex index count");
    expect_true(mesh.primvars.size() == 4, "mesh primvar count");
    expect_true(mesh.primvars[0].interpolation
                    == rt::scene::ScenePrimvarInterpolation::face_varying,
        "normal interpolation");
    expect_true(mesh.primvars[0].indices.size() == 7, "indexed normal count");
    expect_true(std::holds_alternative<std::vector<Eigen::Vector2d>>(mesh.primvars[1].values),
        "texcoord value type");
    expect_true(mesh.material_subset_family_type
                    == rt::scene::SceneMaterialSubsetFamilyType::partition,
        "material subset family");
    expect_true(mesh.material_subsets.size() == 2, "material subset count");
}

void test_camera_contract() {
    const rt::scene::SceneIRv2 scene = scene_with_camera(calibrated_camera());
    rt::scene::require_valid_scene_ir_v2(scene);
    const rt::scene::ScenePrim& prim = *scene.find_prim("/World/Cameras/Main");
    expect_true(prim.camera.has_value(), "camera payload");
    expect_near(prim.camera->focal_length, 16.2, 1e-12, "camera focal length");
    expect_true(prim.camera->renderer_calibration->radial_distortion.size() == 6,
        "camera extension radial coefficients");

    const std::vector<rt::scene::SceneDiagnostic> unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene,
            rt::scene::SceneBackendCapabilities {.backend_name = "USD core only"});
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.camera_model_extension"),
        "camera model extension capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.camera_distortion"),
        "camera distortion capability diagnostic");

    const std::vector<rt::scene::SceneDiagnostic> supported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene, rt::scene::SceneBackendCapabilities {
                                                                .camera_model_extensions = true,
                                                                .camera_distortion = true,
                                                            });
    expect_true(supported.empty(), "camera-capable backend diagnostics");
}

void test_asset_reference_contract() {
    rt::scene::SceneIR legacy;
    legacy.add_texture(rt::scene::ImageTextureDesc {
        .authored_path = "textures/albedo.exr",
        .path = "/resolved/show/textures/albedo.exr",
    });
    const rt::scene::SceneIRv2 scene = rt::scene::compile_legacy_scene_ir_v2(legacy);
    rt::scene::require_valid_scene_ir_v2(scene);
    const rt::scene::ScenePrim& texture = *scene.find_prim("/World/Textures/Texture_0000");
    expect_true(texture.asset_references.size() == 1, "image texture asset reference count");
    expect_true(texture.asset_references[0].authored_path == "textures/albedo.exr",
        "asset authored path");
    expect_true(texture.asset_references[0].resolved_path == "/resolved/show/textures/albedo.exr",
        "asset resolved path");

    const std::vector<rt::scene::SceneDiagnostic> unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene,
            rt::scene::SceneBackendCapabilities {.backend_name = "path-blind backend"});
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.asset_references"),
        "asset reference capability diagnostic");
    expect_true(rt::scene::diagnose_scene_ir_v2_capabilities(scene,
                    rt::scene::SceneBackendCapabilities {
                        .asset_references = true,
                        .materialx_textures = true,
                    })
                    .empty(),
        "asset-aware backend diagnostics");
}

void test_open_pbr_defaults_and_legacy_projection() {
    const rt::scene::SceneOpenPbrSurface defaults;
    expect_true(defaults.version == rt::scene::kOpenPbrVersion, "OpenPBR version");
    expect_true(rt::scene::kOpenPbrMaterialXNodeDef
                    == std::string_view {"ND_open_pbr_surface_surfaceshader"},
        "OpenPBR MaterialX nodedef");
    expect_near(defaults.base_weight, 1.0, 1e-12, "OpenPBR base weight default");
    expect_vec3_near(defaults.base_color, Eigen::Vector3d::Constant(0.8), 1e-12,
        "OpenPBR base color default");
    expect_near(defaults.specular_roughness, 0.3, 1e-12, "OpenPBR specular roughness default");
    expect_near(defaults.specular_ior, 1.5, 1e-12, "OpenPBR IOR default");
    expect_near(defaults.coat_ior, 1.6, 1e-12, "OpenPBR coat IOR default");
    expect_near(defaults.thin_film_thickness, 0.5, 1e-12, "OpenPBR thin-film thickness default");
    expect_true(defaults.geometry_normal_default_geomprop == "Nworld",
        "OpenPBR normal default geomprop");
    expect_true(defaults.geometry_tangent_default_geomprop == "Tworld",
        "OpenPBR tangent default geomprop");
    expect_true(defaults.displacement.type == rt::scene::SceneMaterialXDisplacementType::none,
        "MaterialX displacement is explicitly disabled by default");
    expect_true(defaults.energy_policy
                    == rt::scene::SceneMaterialEnergyPolicy::open_pbr_layered_energy_conserving,
        "OpenPBR layered energy policy");

    const rt::scene::SceneIRv2 compiled =
        rt::scene::compile_legacy_scene_ir_v2(legacy_material_texture_scene());
    rt::scene::require_valid_scene_ir_v2(compiled);

    const rt::scene::ScenePrim& constant = *compiled.find_prim("/World/Textures/Texture_0000");
    expect_true(constant.texture->node == rt::scene::SceneTextureNode::constant_color,
        "constant texture MaterialX node");
    expect_true(constant.texture->node_definition == "ND_constant_color3",
        "constant texture MaterialX nodedef");
    expect_true(constant.texture->color_space == rt::scene::SceneColorSpace::linear_srgb,
        "numeric legacy color space");
    expect_true(constant.compatibility_source_name == "constant_color",
        "constant texture compatibility source");

    const rt::scene::ScenePrim& checker = *compiled.find_prim("/World/Textures/Texture_0002");
    expect_true(checker.texture->node == rt::scene::SceneTextureNode::checkerboard,
        "checkerboard MaterialX node");
    expect_true(checker.texture->node_definition == "ND_checkerboard_color3",
        "checkerboard MaterialX nodedef");
    expect_true(checker.texture->even_texture_path == "/World/Textures/Texture_0000"
                    && checker.texture->odd_texture_path == "/World/Textures/Texture_0001",
        "checkerboard connected texture inputs");
    expect_near(checker.texture->scale, 2.0, 1e-12, "checkerboard scale");

    const rt::scene::ScenePrim& image = *compiled.find_prim("/World/Textures/Texture_0003");
    expect_true(image.texture->node == rt::scene::SceneTextureNode::image, "image MaterialX node");
    expect_true(image.texture->node_definition == "ND_image_color3", "image MaterialX nodedef");
    expect_true(image.texture->color_space == rt::scene::SceneColorSpace::raw,
        "legacy image no-conversion color space");
    expect_true(image.texture->u_address_mode == rt::scene::SceneTextureAddressMode::periodic
                    && image.texture->filter_type == rt::scene::SceneTextureFilterType::linear,
        "MaterialX image sampling defaults");
    expect_true(image.asset_references.size() == 1
                    && image.asset_references[0].authored_path == "textures/emission.exr",
        "image texture asset semantics");

    const rt::scene::ScenePrim& noise = *compiled.find_prim("/World/Textures/Texture_0004");
    expect_true(noise.texture->node == rt::scene::SceneTextureNode::noise3d,
        "noise MaterialX node");
    expect_true(noise.texture->node_definition == "ND_noise3d_color3", "noise MaterialX nodedef");
    expect_near(noise.texture->scale, 4.0, 1e-12, "noise scale");

    const auto& diffuse = std::get<rt::scene::SceneOpenPbrSurface>(
        *compiled.find_prim("/World/Materials/Material_0000")->material);
    expect_near(diffuse.specular_weight, 0.0, 1e-12, "legacy diffuse lobe mapping");
    expect_true(diffuse.connections.size() == 1 && diffuse.connections[0].input_name == "base_color"
                    && diffuse.connections[0].input_type
                           == rt::scene::SceneMaterialValueType::color3
                    && diffuse.connections[0].texture_path == "/World/Textures/Texture_0000",
        "legacy diffuse OpenPBR connection");

    const auto& metal = std::get<rt::scene::SceneOpenPbrSurface>(
        *compiled.find_prim("/World/Materials/Material_0001")->material);
    expect_near(metal.base_metalness, 1.0, 1e-12, "legacy metal metalness");
    expect_near(metal.specular_roughness, 0.25, 1e-12, "legacy metal fuzz mapping");
    expect_true(metal.connections[0].texture_path == "/World/Textures/Texture_0002",
        "legacy metal base color connection");

    const auto& dielectric = std::get<rt::scene::SceneOpenPbrSurface>(
        *compiled.find_prim("/World/Materials/Material_0002")->material);
    expect_near(dielectric.specular_ior, 1.52, 1e-12, "legacy dielectric IOR");
    expect_near(dielectric.transmission_weight, 1.0, 1e-12, "legacy dielectric transmission");
    expect_near(dielectric.specular_roughness, 0.0, 1e-12,
        "legacy dielectric perfect specular mapping");

    const auto& emissive = std::get<rt::scene::SceneOpenPbrSurface>(
        *compiled.find_prim("/World/Materials/Material_0003")->material);
    expect_near(emissive.base_weight, 0.0, 1e-12, "legacy emissive base disabled");
    expect_near(emissive.emission_luminance, 1.0, 1e-12, "legacy emissive unit luminance mapping");
    expect_true(emissive.connections[0].input_name == "emission_color"
                    && emissive.connections[0].texture_path == "/World/Textures/Texture_0003",
        "legacy emissive OpenPBR connection");

    const auto& volume = std::get<rt::scene::SceneIsotropicVolumeMaterial>(
        *compiled.find_prim("/World/Materials/Material_0004")->material);
    expect_true(volume.scattering_color_texture_path == "/World/Textures/Texture_0004",
        "legacy volume scattering connection");
    expect_near(volume.scattering_anisotropy, 0.0, 1e-12, "legacy isotropic volume anisotropy");
    expect_true(compiled.find_prim("/World/Materials/Material_0004")->compatibility_source_name
                    == "isotropic_volume",
        "legacy volume compatibility source");

    const std::vector<rt::scene::SceneDiagnostic> unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(compiled,
            rt::scene::SceneBackendCapabilities {.backend_name = "legacy backend"});
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.materialx_textures"),
        "MaterialX texture capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.texture_color_spaces"),
        "texture color-space capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.open_pbr_surface"),
        "OpenPBR capability diagnostic");
    expect_true(
        rt::scene::has_scene_diagnostic(unsupported, "capability.isotropic_volume_material"),
        "volume capability diagnostic");

    const std::vector<rt::scene::SceneDiagnostic> supported =
        rt::scene::diagnose_scene_ir_v2_capabilities(compiled,
            rt::scene::SceneBackendCapabilities {
                .asset_references = true,
                .materialx_textures = true,
                .texture_color_spaces = true,
                .open_pbr_surface = true,
                .isotropic_volume_materials = true,
            });
    expect_true(supported.empty(), "fully capable material backend diagnostics");
}

void test_light_contract() {
    const rt::scene::SceneLight rect {
        .type = rt::scene::SceneLightType::rect,
        .color = Eigen::Vector3d {1.0, 0.8, 0.6},
        .intensity = 2.0,
        .exposure = 2.0,
        .normalize = true,
        .enable_color_temperature = true,
        .color_temperature_kelvin = 3200.0,
        .width = 2.0,
        .height = 3.0,
    };
    const rt::scene::SceneIRv2 scene = scene_with_light(rect);
    rt::scene::require_valid_scene_ir_v2(scene);
    expect_near(rt::scene::scene_light_exposed_intensity(rect), 8.0, 1e-12, "light exposure scale");
    expect_true(rt::scene::scene_light_intensity_unit(rect)
                    == rt::scene::SceneLightIntensityUnit::nit,
        "area light intensity unit");

    rt::scene::SceneLight distant;
    distant.type = rt::scene::SceneLightType::distant;
    distant.normalize = true;
    expect_true(rt::scene::scene_light_intensity_unit(distant)
                    == rt::scene::SceneLightIntensityUnit::lux,
        "normalized distant light intensity unit");

    const std::vector<rt::scene::SceneDiagnostic> unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene,
            rt::scene::SceneBackendCapabilities {.backend_name = "legacy backend"});
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.usd_lux_lights"),
        "UsdLux capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.analytic_lights"),
        "analytic light capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.light_normalization"),
        "light normalization capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.light_color_temperature"),
        "light color temperature capability diagnostic");

    rt::scene::SceneLight dome;
    dome.type = rt::scene::SceneLightType::dome;
    const std::vector<rt::scene::SceneDiagnostic> dome_unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene_with_light(std::move(dome)),
            rt::scene::SceneBackendCapabilities {
                .usd_lux_lights = true,
                .analytic_lights = true,
            });
    expect_true(rt::scene::has_scene_diagnostic(dome_unsupported, "capability.dome_lights"),
        "dome light capability diagnostic");

    const std::vector<rt::scene::SceneDiagnostic> supported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene, rt::scene::SceneBackendCapabilities {
                                                                .usd_lux_lights = true,
                                                                .analytic_lights = true,
                                                                .light_normalization = true,
                                                                .light_color_temperature = true,
                                                            });
    expect_true(supported.empty(), "UsdLux-capable backend diagnostics");
}

void test_legacy_emissive_light_projection() {
    const rt::scene::SceneIRv2 compiled =
        rt::scene::compile_legacy_scene_ir_v2(legacy_emissive_scene());
    rt::scene::require_valid_scene_ir_v2(compiled);
    const rt::scene::ScenePrim& surface = *compiled.find_prim("/World/Geometry/Surface_0000");
    expect_true(surface.light.has_value(), "legacy emissive surface light payload");
    expect_true(surface.light->type == rt::scene::SceneLightType::geometry,
        "legacy emissive geometry light type");
    expect_true(surface.light->material_sync_mode
                    == rt::scene::SceneLightMaterialSyncMode::material_glow_tints_light,
        "legacy emissive material sync mode");
    expect_true(!surface.light->normalize, "legacy emissive radiance is not area-normalized");
    expect_near(rt::scene::scene_light_exposed_intensity(*surface.light), 1.0, 1e-12,
        "legacy emissive exposed intensity");
    expect_true(rt::scene::scene_light_intensity_unit(*surface.light)
                    == rt::scene::SceneLightIntensityUnit::nit,
        "legacy emissive intensity unit");
    expect_true(surface.material_path == "/World/Materials/Material_0000",
        "legacy emissive material binding preserved");

    const std::vector<rt::scene::SceneDiagnostic> unsupported =
        rt::scene::diagnose_scene_ir_v2_capabilities(compiled,
            rt::scene::SceneBackendCapabilities {.mesh_ngons = true});
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.usd_lux_lights"),
        "legacy geometry light UsdLux capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(unsupported, "capability.geometry_lights"),
        "legacy geometry light capability diagnostic");
    expect_true(rt::scene::diagnose_scene_ir_v2_capabilities(compiled,
                    rt::scene::SceneBackendCapabilities {
                        .mesh_ngons = true,
                        .materialx_textures = true,
                        .texture_color_spaces = true,
                        .open_pbr_surface = true,
                        .usd_lux_lights = true,
                        .geometry_lights = true,
                    })
                    .empty(),
        "geometry-light-capable backend diagnostics");
}

void test_contract_rejections() {
    rt::scene::SceneIRv2 invalid_path = semantic_scene();
    invalid_path.add_prim(rt::scene::ScenePrim {.path = "World/Relative"});
    expect_invalid(invalid_path, "prim.path.invalid");

    rt::scene::SceneIRv2 duplicate = semantic_scene();
    duplicate.add_prim(rt::scene::ScenePrim {.path = "/World/Group"});
    expect_invalid(duplicate, "prim.path.duplicate");

    rt::scene::SceneIRv2 missing_parent = semantic_scene();
    missing_parent.add_prim(rt::scene::ScenePrim {.path = "/World/Missing/Child"});
    expect_invalid(missing_parent, "prim.parent.missing");

    rt::scene::SceneIRv2 non_affine = semantic_scene();
    Eigen::Matrix4d bad_transform = Eigen::Matrix4d::Identity();
    bad_transform(3, 0) = 1.0;
    non_affine.add_prim(
        rt::scene::ScenePrim {.path = "/World/NonAffine", .local_to_parent = bad_transform});
    expect_invalid(non_affine, "xform.non_affine");

    rt::scene::SceneIRv2 sample_order = semantic_scene();
    sample_order.add_prim(rt::scene::ScenePrim {
        .path = "/World/BadSamples",
        .transform_samples =
            {
                {.time_code = 1.0},
                {.time_code = 1.0},
            },
    });
    expect_invalid(sample_order, "xform.sample_time.order");

    rt::scene::SceneIRv2 bad_stage = semantic_scene();
    bad_stage.stage_metadata().meters_per_unit = 0.0;
    expect_invalid(bad_stage, "stage.meters_per_unit");

    rt::scene::SceneMeshGeometry bad_topology = valid_mesh_geometry();
    bad_topology.face_vertex_counts = {3, 3};
    expect_invalid(scene_with_mesh(std::move(bad_topology)), "geometry.mesh.topology_size");

    rt::scene::SceneMeshGeometry bad_vertex_index = valid_mesh_geometry();
    bad_vertex_index.face_vertex_indices.back() = 99;
    expect_invalid(scene_with_mesh(std::move(bad_vertex_index)),
        "geometry.mesh.vertex_index_range");

    rt::scene::SceneMeshGeometry bad_primvar = valid_mesh_geometry();
    bad_primvar.primvars[0].indices.pop_back();
    expect_invalid(scene_with_mesh(std::move(bad_primvar)), "geometry.primvar.index_count");

    rt::scene::SceneMeshGeometry overlapping_subsets = valid_mesh_geometry();
    overlapping_subsets.material_subsets[1].face_indices = {0, 1};
    expect_invalid(scene_with_mesh(std::move(overlapping_subsets)), "geometry.subset.overlap");

    rt::scene::SceneMeshGeometry incomplete_partition = valid_mesh_geometry();
    incomplete_partition.material_subsets.pop_back();
    expect_invalid(scene_with_mesh(std::move(incomplete_partition)), "geometry.subset.partition");

    rt::scene::SceneMeshGeometry bad_subset_material = valid_mesh_geometry();
    bad_subset_material.material_subsets[0].material_path = "/World/Materials/Missing";
    expect_invalid(scene_with_mesh(std::move(bad_subset_material)), "geometry.subset.material");

    expect_invalid(scene_with_mesh(valid_mesh_geometry(rt::scene::SceneSubdivisionScheme::loop)),
        "geometry.mesh.loop_non_triangle");

    rt::scene::SceneIRv2 missing_geometry = semantic_scene();
    missing_geometry.add_prim(rt::scene::ScenePrim {
        .path = "/World/EmptyPrototype",
        .kind = rt::scene::ScenePrimKind::geometry_prototype,
    });
    expect_invalid(missing_geometry, "geometry.payload_missing");

    rt::scene::SceneIRv2 missing_camera = semantic_scene();
    missing_camera.add_prim(rt::scene::ScenePrim {
        .path = "/World/MissingCamera",
        .kind = rt::scene::ScenePrimKind::camera,
    });
    expect_invalid(missing_camera, "camera.payload_missing");

    rt::scene::SceneCamera bad_clipping = calibrated_camera();
    bad_clipping.clipping_range = Eigen::Vector2d {10.0, 1.0};
    expect_invalid(scene_with_camera(std::move(bad_clipping)), "camera.clipping_range");

    rt::scene::SceneCamera bad_calibration = calibrated_camera();
    bad_calibration.renderer_calibration->image_width = 0;
    expect_invalid(scene_with_camera(std::move(bad_calibration)), "camera.calibration.resolution");

    rt::scene::SceneIRv2 unexpected_camera = semantic_scene();
    unexpected_camera.add_prim(rt::scene::ScenePrim {
        .path = "/World/UnexpectedCameraPayload",
        .camera = calibrated_camera(),
    });
    expect_invalid(unexpected_camera, "camera.payload_unexpected");

    rt::scene::SceneIRv2 empty_asset = semantic_scene();
    empty_asset.add_prim(rt::scene::ScenePrim {
        .path = "/World/EmptyAsset",
        .kind = rt::scene::ScenePrimKind::texture,
        .asset_references = {rt::scene::SceneAssetReference {}},
    });
    expect_invalid(empty_asset, "asset.authored_path_empty");

    rt::scene::SceneIRv2 control_asset = semantic_scene();
    control_asset.add_prim(rt::scene::ScenePrim {
        .path = "/World/ControlAsset",
        .kind = rt::scene::ScenePrimKind::texture,
        .asset_references = {rt::scene::SceneAssetReference {.authored_path = "bad\npath"}},
    });
    expect_invalid(control_asset, "asset.path_control_character");

    rt::scene::SceneIRv2 missing_texture;
    missing_texture.add_prim(rt::scene::ScenePrim {.path = "/World"});
    missing_texture.add_prim(rt::scene::ScenePrim {.path = "/World/Textures"});
    missing_texture.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Missing",
        .kind = rt::scene::ScenePrimKind::texture,
    });
    expect_invalid(missing_texture, "texture.payload_missing");

    rt::scene::SceneIRv2 negative_texture;
    negative_texture.add_prim(rt::scene::ScenePrim {.path = "/World"});
    negative_texture.add_prim(rt::scene::ScenePrim {.path = "/World/Textures"});
    negative_texture.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Negative",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture = rt::scene::SceneTexture {.value = Eigen::Vector3d {-0.1, 0.0, 0.0}},
    });
    expect_invalid(negative_texture, "texture.value");

    rt::scene::SceneIRv2 bad_checker;
    bad_checker.add_prim(rt::scene::ScenePrim {.path = "/World"});
    bad_checker.add_prim(rt::scene::ScenePrim {.path = "/World/Textures"});
    bad_checker.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Checker",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .node = rt::scene::SceneTextureNode::checkerboard,
                .even_texture_path = "/World/Textures/MissingEven",
                .odd_texture_path = "/World/Textures/MissingOdd",
            },
    });
    expect_invalid(bad_checker, "texture.checker.even_reference");

    rt::scene::SceneIRv2 missing_material;
    missing_material.add_prim(rt::scene::ScenePrim {.path = "/World"});
    missing_material.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    missing_material.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Missing",
        .kind = rt::scene::ScenePrimKind::material,
    });
    expect_invalid(missing_material, "material.payload_missing");

    rt::scene::SceneOpenPbrSurface invalid_weight;
    invalid_weight.geometry_opacity = 1.5;
    expect_invalid(scene_with_open_pbr_material(std::move(invalid_weight)),
        "material.open_pbr.weight");

    rt::scene::SceneOpenPbrSurface invalid_connection;
    invalid_connection.connections.push_back(rt::scene::SceneMaterialConnection {
        .input_name = "project_specific_base_color",
        .texture_path = "/World/Textures/Missing",
    });
    expect_invalid(scene_with_open_pbr_material(std::move(invalid_connection)),
        "material.connection.input_name");

    rt::scene::SceneIRv2 unexpected_material = semantic_scene();
    unexpected_material.add_prim(rt::scene::ScenePrim {
        .path = "/World/UnexpectedMaterialPayload",
        .material = rt::scene::SceneMaterial {rt::scene::SceneOpenPbrSurface {}},
    });
    expect_invalid(unexpected_material, "material.payload_unexpected");

    rt::scene::SceneIRv2 missing_light = semantic_scene();
    missing_light.add_prim(rt::scene::ScenePrim {
        .path = "/World/MissingLight",
        .kind = rt::scene::ScenePrimKind::light,
    });
    expect_invalid(missing_light, "light.payload_missing");

    rt::scene::SceneLight geometry_light;
    geometry_light.type = rt::scene::SceneLightType::geometry;
    expect_invalid(scene_with_light(std::move(geometry_light)), "light.payload_kind");

    rt::scene::SceneLight negative_intensity;
    negative_intensity.intensity = -1.0;
    expect_invalid(scene_with_light(std::move(negative_intensity)), "light.intensity");

    rt::scene::SceneLight invalid_temperature;
    invalid_temperature.color_temperature_kelvin = 999.0;
    expect_invalid(scene_with_light(std::move(invalid_temperature)), "light.color_temperature");

    rt::scene::SceneLight overflowing_exposure;
    overflowing_exposure.exposure = 1024.0;
    expect_invalid(scene_with_light(std::move(overflowing_exposure)), "light.exposed_intensity");

    rt::scene::SceneLight invalid_rect;
    invalid_rect.type = rt::scene::SceneLightType::rect;
    invalid_rect.width = 0.0;
    expect_invalid(scene_with_light(std::move(invalid_rect)), "light.rect_size");

    rt::scene::SceneIRv2 unexpected_light = semantic_scene();
    unexpected_light.add_prim(rt::scene::ScenePrim {
        .path = "/World/UnexpectedLightPayload",
        .light = rt::scene::SceneLight {},
    });
    expect_invalid(unexpected_light, "light.payload_unexpected");
}

void test_capability_diagnostics() {
    const rt::scene::SceneIRv2 scene = semantic_scene();
    const std::vector<rt::scene::SceneDiagnostic> diagnostics =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene,
            rt::scene::SceneBackendCapabilities {.backend_name = "legacy realtime"});
    expect_true(rt::scene::has_scene_diagnostic(diagnostics, "capability.full_affine_transform"),
        "full affine capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(diagnostics, "capability.transform_time_samples"),
        "time sample capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(diagnostics, "capability.reset_xform_stack"),
        "reset stack capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(diagnostics, "capability.non_render_purpose"),
        "purpose capability diagnostic");

    const std::vector<rt::scene::SceneDiagnostic> supported =
        rt::scene::diagnose_scene_ir_v2_capabilities(scene,
            rt::scene::SceneBackendCapabilities {
                .backend_name = "semantic test backend",
                .full_affine_transforms = true,
                .transform_time_samples = true,
                .reset_xform_stack = true,
                .non_render_purposes = true,
            });
    expect_true(supported.empty(), "fully capable backend diagnostics");

    const rt::scene::SceneIRv2 mesh_scene =
        scene_with_mesh(valid_mesh_geometry(rt::scene::SceneSubdivisionScheme::catmull_clark));
    const std::vector<rt::scene::SceneDiagnostic> mesh_diagnostics =
        rt::scene::diagnose_scene_ir_v2_capabilities(mesh_scene,
            rt::scene::SceneBackendCapabilities {.backend_name = "legacy realtime"});
    expect_true(rt::scene::has_scene_diagnostic(mesh_diagnostics, "capability.mesh_ngons"),
        "ngon capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(mesh_diagnostics, "capability.mesh_primvars"),
        "primvar capability diagnostic");
    expect_true(
        rt::scene::has_scene_diagnostic(mesh_diagnostics, "capability.subdivision_surfaces"),
        "subdivision capability diagnostic");
    expect_true(rt::scene::has_scene_diagnostic(mesh_diagnostics, "capability.material_subsets"),
        "material subset capability diagnostic");

    const std::vector<rt::scene::SceneDiagnostic> mesh_supported =
        rt::scene::diagnose_scene_ir_v2_capabilities(mesh_scene,
            rt::scene::SceneBackendCapabilities {
                .backend_name = "semantic mesh backend",
                .mesh_ngons = true,
                .mesh_primvars = true,
                .subdivision_surfaces = true,
                .material_subsets = true,
                .open_pbr_surface = true,
            });
    expect_true(mesh_supported.empty(), "fully capable mesh backend diagnostics");
}

void test_legacy_compiler() {
    const rt::scene::SceneIRv2 compiled = rt::scene::compile_legacy_scene_ir_v2(legacy_scene());
    rt::scene::require_valid_scene_ir_v2(compiled);
    expect_near(compiled.stage_metadata().meters_per_unit, 1.0, 1e-12,
        "legacy units are explicit meters");
    expect_true(compiled.prims().size() == 13, "deterministic compatibility prim count");

    const rt::scene::ScenePrim* surface = compiled.find_prim("/World/Geometry/Surface_0000");
    expect_true(surface != nullptr, "compiled surface path");
    expect_true(surface->prototype_path == "/World/Prototypes/Shape_0001",
        "compiled prototype reference");
    expect_true(surface->material_path == "/World/Materials/Material_0000",
        "compiled material reference");
    expect_true(!surface->light.has_value(), "non-emissive surface is not a geometry light");
    expect_vec3_near(surface->local_to_parent.block<3, 1>(0, 3), Eigen::Vector3d {1.0, 2.0, 3.0},
        1e-12, "compiled affine transform");

    const rt::scene::ScenePrim* geometry = compiled.find_prim("/World/Prototypes/Shape_0001");
    expect_true(geometry != nullptr && geometry->geometry.has_value(), "compiled geometry payload");
    const auto& mesh = std::get<rt::scene::SceneMeshGeometry>(*geometry->geometry);
    expect_true(mesh.face_vertex_counts == std::vector<std::int32_t>({3}),
        "compiled triangle topology");
    expect_true(mesh.primvars.size() == 2, "compiled normal and texcoord primvars");
    expect_true(mesh.primvars[0].name == "normals" && mesh.primvars[0].indices.size() == 3,
        "compiled indexed normals");
    expect_true(mesh.primvars[1].name == "st" && mesh.primvars[1].indices.size() == 3,
        "compiled indexed texcoords");

    const rt::scene::ScenePrim* sphere = compiled.find_prim("/World/Prototypes/Shape_0000");
    expect_true(sphere != nullptr
                    && std::holds_alternative<rt::scene::SceneSphereGeometry>(*sphere->geometry),
        "compiled sphere geometry");

    const rt::scene::ScenePrim* medium = compiled.find_prim("/World/Volumes/Medium_0000");
    expect_true(medium != nullptr && medium->volume_density == 0.5, "compiled volume");
    expect_true(compiled.find_prim("/World/Textures/Texture_0000") != nullptr,
        "compiled texture identity");
    expect_true(compiled.find_prim("/World/Materials/Material_0001") != nullptr,
        "compiled material identity");
}

} // namespace

int main() {
    test_stage_hierarchy_and_sampling();
    test_mesh_geometry_contract();
    test_camera_contract();
    test_asset_reference_contract();
    test_open_pbr_defaults_and_legacy_projection();
    test_light_contract();
    test_legacy_emissive_light_projection();
    test_contract_rejections();
    test_capability_diagnostics();
    test_legacy_compiler();
    return 0;
}
