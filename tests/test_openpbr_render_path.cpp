#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/interval.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/ray.h"
#include "common/sphere.h"
#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/openpbr_core_adapter.h"
#include "scene/openusd_stage_importer.h"
#include "scene/realtime_scene_adapter.h"
#include "test_support.h"

#include <tbb/global_control.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <unordered_map>

namespace {

constexpr int kImageSize = 64;
constexpr int kCenterPixel = kImageSize / 2;

struct OpenPbrReferenceScene {
    rt::scene::SceneIR compatibility;
    rt::scene::SceneIRv2 scene_v2;
};

OpenPbrReferenceScene make_reference_scene() {
    OpenPbrReferenceScene scene;
    const Eigen::Vector3d base_source {0.5, 0.25, 0.75};
    const Eigen::Vector3d emission_source {0.5, 0.25, 0.125};
    const Eigen::Vector3d metalness_source = Eigen::Vector3d::Zero();
    const Eigen::Vector3d roughness_source = Eigen::Vector3d::Constant(0.6);
    const int base_texture =
        scene.compatibility.add_texture(rt::scene::ConstantColorTextureDesc {.color = base_source});
    const int emission_texture = scene.compatibility.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = emission_source});
    const int metalness_texture = scene.compatibility.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = metalness_source});
    const int roughness_texture = scene.compatibility.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = roughness_source});
    const int legacy_material = scene.compatibility.add_material(
        rt::scene::DiffuseMaterial {.albedo_texture = base_texture});
    const int sphere = scene.compatibility.add_shape(rt::scene::SphereShape {
        .center = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}),
        .radius = 1.0,
    });
    scene.compatibility.add_instance(rt::scene::SurfaceInstance {
        .shape_index = sphere,
        .material_index = legacy_material,
    });

    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World/Textures"});
    scene.scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Base",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .color_space = rt::scene::SceneColorSpace::srgb_texture,
                .value = base_source,
            },
        .compatibility_source_index = static_cast<std::size_t>(base_texture),
    });
    scene.scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Emission",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .color_space = rt::scene::SceneColorSpace::linear_srgb,
                .value = emission_source,
            },
        .compatibility_source_index = static_cast<std::size_t>(emission_texture),
    });
    scene.scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Metalness",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .node_definition = "ND_constant_float",
                .output_type = rt::scene::SceneMaterialValueType::float_,
                .color_space = rt::scene::SceneColorSpace::raw,
                .value = metalness_source,
            },
        .compatibility_source_index = static_cast<std::size_t>(metalness_texture),
    });
    scene.scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Roughness",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture =
            rt::scene::SceneTexture {
                .node_definition = "ND_constant_float",
                .output_type = rt::scene::SceneMaterialValueType::float_,
                .color_space = rt::scene::SceneColorSpace::raw,
                .value = roughness_source,
            },
        .compatibility_source_index = static_cast<std::size_t>(roughness_texture),
    });
    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    rt::scene::SceneOpenPbrSurface surface;
    surface.specular_weight = 0.0;
    surface.base_metalness = 1.0;
    surface.specular_roughness = 0.05;
    surface.emission_luminance = 2.0;
    surface.connections = {
        rt::scene::SceneMaterialConnection {
            .input_name = "base_color",
            .input_type = rt::scene::SceneMaterialValueType::color3,
            .texture_path = "/World/Textures/Base",
        },
        rt::scene::SceneMaterialConnection {
            .input_name = "emission_color",
            .input_type = rt::scene::SceneMaterialValueType::color3,
            .texture_path = "/World/Textures/Emission",
        },
        rt::scene::SceneMaterialConnection {
            .input_name = "base_metalness",
            .input_type = rt::scene::SceneMaterialValueType::float_,
            .texture_path = "/World/Textures/Metalness",
        },
        rt::scene::SceneMaterialConnection {
            .input_name = "specular_roughness",
            .input_type = rt::scene::SceneMaterialValueType::float_,
            .texture_path = "/World/Textures/Roughness",
        },
    };
    scene.scene_v2.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Reference",
        .kind = rt::scene::ScenePrimKind::material,
        .material = surface,
        .compatibility_source_index = 0,
    });
    return scene;
}

rt::CameraRig make_test_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {200.0, 200.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), kImageSize, kImageSize);
    return rig;
}

Eigen::Vector3d center_pixel_rgb(const rt::RadianceFrame& frame) {
    const std::size_t index =
        (static_cast<std::size_t>(kCenterPixel) * static_cast<std::size_t>(frame.width)
            + static_cast<std::size_t>(kCenterPixel))
        * 4U;
    return {frame.beauty_rgba[index], frame.beauty_rgba[index + 1], frame.beauty_rgba[index + 2]};
}

double center_pixel_luminance(const rt::RadianceFrame& frame) {
    return center_pixel_rgb(frame).mean();
}

rt::RadianceFrame render_openpbr_direct_light(bool openpbr_light) {
    rt::SceneDescription scene;
    rt::OpenPbrCoreMaterial receiver_parameters;
    receiver_parameters.base_weight = 0.0f;
    receiver_parameters.base_color = {0.0f, 0.0f, 0.0f};
    receiver_parameters.specular_weight = 1.0f;
    receiver_parameters.specular_roughness = 0.5f;
    const int receiver = scene.add_material(rt::OpenPbrMaterialDesc {
        .compiled = rt::OpenPbrCompiledMaterial {.parameters = receiver_parameters},
    });

    int light = -1;
    if (openpbr_light) {
        rt::OpenPbrCoreMaterial light_parameters;
        light_parameters.base_weight = 0.0f;
        light_parameters.specular_weight = 0.0f;
        light_parameters.emission_luminance = 20.0f;
        light = scene.add_material(rt::OpenPbrMaterialDesc {
            .compiled = rt::OpenPbrCompiledMaterial {.parameters = light_parameters},
        });
    } else {
        light = scene.add_material(
            rt::DiffuseLightMaterial {.emission = Eigen::Vector3d::Constant(20.0)});
    }

    scene.add_triangle(rt::TrianglePrimitive {
        .material_index = light,
        .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, 1.2, 1.0}),
        .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 2.4, 1.0}),
        .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, 1.2, 1.0}),
        .dynamic = false,
    });
    scene.add_quad(rt::QuadPrimitive {
        .material_index = receiver,
        .origin = rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -4.0}),
        .edge_u = rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        .edge_v = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
        .dynamic = false,
    });

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 1;
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), profile, 0);
}

rt::RadianceFrame render_analytic_direct_light(rt::AnalyticLightType type, bool occluded) {
    rt::SceneDescription scene;
    const int receiver =
        scene.add_material(rt::LambertianMaterial {.albedo = Eigen::Vector3d::Constant(0.8)});
    scene.add_quad(rt::QuadPrimitive {
        .material_index = receiver,
        .origin = rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -4.0}),
        .edge_u = rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        .edge_v = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
        .dynamic = false,
    });

    constexpr double inverse_sqrt_two = 0.7071067811865475244;
    const Eigen::Vector3d direction {inverse_sqrt_two, -inverse_sqrt_two, 0.0};
    rt::AnalyticLightDesc light;
    light.type = type;
    light.radiance = Eigen::Vector3d::Constant(12.0);
    light.selection_pdf = 1.0;
    light.cdf = 1.0;
    if (type == rt::AnalyticLightType::distant) {
        light.local_to_world_linear.col(2) = direction;
        light.cos_theta_max = 1.0;
        light.delta = true;
    } else if (type == rt::AnalyticLightType::sphere) {
        light.position = Eigen::Vector3d {0.0, 4.0, 0.0} + direction * 2.0;
        light.radius = 0.5;
        light.world_area = 4.0 * std::numbers::pi * light.radius * light.radius;
    } else if (type == rt::AnalyticLightType::cylinder) {
        light.position = Eigen::Vector3d {0.0, 4.0, 0.0} + direction * 2.0;
        light.local_to_world_linear.col(0) = direction;
        light.local_to_world_linear.col(1) =
            Eigen::Vector3d {inverse_sqrt_two, inverse_sqrt_two, 0.0};
        light.local_to_world_linear.col(2) = Eigen::Vector3d::UnitZ();
        light.radius = 0.5;
        light.length = 1.0;
        light.world_area = 2.0 * std::numbers::pi * light.radius * light.length;
    } else {
        light.position = Eigen::Vector3d {0.0, 4.0, 0.0} + direction * 2.0;
        light.local_to_world_linear.col(0) = Eigen::Vector3d {0.0, 0.0, 1.0};
        light.local_to_world_linear.col(1) =
            Eigen::Vector3d {-inverse_sqrt_two, -inverse_sqrt_two, 0.0};
        light.local_to_world_linear.col(2) = direction;
        if (type == rt::AnalyticLightType::disk) {
            light.radius = 0.5;
            light.world_area = std::numbers::pi * light.radius * light.radius;
        } else {
            light.width = 1.0;
            light.height = 1.0;
            light.world_area = 1.0;
        }
    }
    scene.add_analytic_light(light);

    if (occluded) {
        scene.add_sphere(rt::SpherePrimitive {receiver, Eigen::Vector3d {0.0, 4.0, 0.0} + direction,
            0.45, false});
    }

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = type == rt::AnalyticLightType::distant ? 1 : 64;
    profile.max_bounces = 1;
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), profile, 0);
}

rt::RadianceFrame render_analytic_dome_miss() {
    rt::SceneDescription scene;
    const int offscreen =
        scene.add_material(rt::LambertianMaterial {.albedo = Eigen::Vector3d::Constant(0.5)});
    scene.add_sphere(
        rt::SpherePrimitive {offscreen, Eigen::Vector3d {100.0, 100.0, 100.0}, 1.0, false});
    rt::AnalyticLightDesc dome;
    dome.type = rt::AnalyticLightType::dome;
    dome.radiance = Eigen::Vector3d {0.25, 0.5, 1.0};
    dome.selection_pdf = 1.0;
    dome.cdf = 1.0;
    scene.add_analytic_light(dome);

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 1;
    profile.max_bounces = 1;
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), profile, 0);
}

rt::AnalyticLightDesc make_cpu_analytic_light(rt::AnalyticLightType type) {
    const Eigen::Vector3d receiver {0.0, 0.0, -4.0};
    const Eigen::Vector3d center {0.0, 1.5, -2.5};
    const Eigen::Vector3d direction = (center - receiver).normalized();

    rt::AnalyticLightDesc light;
    light.type = type;
    light.position = center;
    light.radiance = Eigen::Vector3d::Constant(1.5);
    light.selection_pdf = 1.0;
    light.cdf = 1.0;
    if (type == rt::AnalyticLightType::distant) {
        light.local_to_world_linear.col(2) = direction;
        light.cos_theta_max = 1.0;
        light.delta = true;
    } else if (type == rt::AnalyticLightType::sphere) {
        light.radius = 0.5;
        light.world_area = 4.0 * std::numbers::pi * light.radius * light.radius;
    } else if (type == rt::AnalyticLightType::cylinder) {
        light.local_to_world_linear.col(0) = -direction;
        light.local_to_world_linear.col(1) = Eigen::Vector3d::UnitX();
        light.local_to_world_linear.col(2) =
            light.local_to_world_linear.col(0).cross(light.local_to_world_linear.col(1));
        light.radius = 0.5;
        light.length = 1.0;
        light.world_area = 2.0 * std::numbers::pi * light.radius * light.length;
    } else if (type == rt::AnalyticLightType::disk || type == rt::AnalyticLightType::rect) {
        light.local_to_world_linear.col(0) = Eigen::Vector3d::UnitX();
        light.local_to_world_linear.col(1) = direction.cross(Eigen::Vector3d::UnitX());
        light.local_to_world_linear.col(2) = direction;
        if (type == rt::AnalyticLightType::disk) {
            light.radius = 0.5;
            light.world_area = std::numbers::pi * light.radius * light.radius;
        } else {
            light.width = 1.0;
            light.height = 1.0;
            light.world_area = 1.0;
        }
    }
    return light;
}

Eigen::Vector3d center_display_rgb(const cv::Mat& image) {
    const cv::Vec3b bgr = image.at<cv::Vec3b>(image.rows / 2, image.cols / 2);
    return {static_cast<double>(bgr[2]) / 255.0, static_cast<double>(bgr[1]) / 255.0,
        static_cast<double>(bgr[0]) / 255.0};
}

double render_cpu_analytic_direct_light(rt::AnalyticLightType type, bool occluded) {
    const pro::proxy<Material> receiver_material =
        pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.8, 0.8, 0.8});
    HittableList world;
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {-2.0, -2.0, -4.0},
        Vec3d {4.0, 0.0, 0.0}, Vec3d {0.0, 4.0, 0.0}, receiver_material));
    if (occluded) {
        world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.75, -3.25}, 0.7,
            receiver_material));
    }
    const pro::proxy<Hittable> world_proxy = &world;

    Camera camera;
    camera.aspect_ratio = 1.0;
    camera.image_width = 16;
    camera.samples_per_pixel = type == rt::AnalyticLightType::distant ? 1 : 64;
    camera.max_depth = 2;
    camera.background = Vec3d::Zero();
    camera.vfov = 45.0;
    camera.lookfrom = Vec3d::Zero();
    camera.lookat = {0.0, 0.0, -4.0};
    camera.vup = {0.0, 1.0, 0.0};
    camera.defocus_angle = 0.0;
    camera.focus_dist = 4.0;
    camera.render(world_proxy, {}, {make_cpu_analytic_light(type)});
    return center_display_rgb(camera.img).mean();
}

Eigen::Vector3d render_cpu_analytic_dome_miss() {
    const pro::proxy<Material> offscreen_material =
        pro::make_proxy_shared<Material, Lambertion>(Vec3d {0.5, 0.5, 0.5});
    HittableList world;
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {100.0, 100.0, 100.0}, 1.0,
        offscreen_material));
    const pro::proxy<Hittable> world_proxy = &world;

    rt::AnalyticLightDesc dome;
    dome.type = rt::AnalyticLightType::dome;
    dome.radiance = Eigen::Vector3d {0.25, 0.5, 1.0};
    dome.selection_pdf = 1.0;
    dome.cdf = 1.0;

    Camera camera;
    camera.aspect_ratio = 1.0;
    camera.image_width = 8;
    camera.samples_per_pixel = 1;
    camera.max_depth = 1;
    camera.background = Vec3d::Zero();
    camera.vfov = 45.0;
    camera.lookfrom = Vec3d::Zero();
    camera.lookat = {0.0, 0.0, -1.0};
    camera.vup = {0.0, 1.0, 0.0};
    camera.defocus_angle = 0.0;
    camera.focus_dist = 1.0;
    camera.render(world_proxy, {}, {dome});
    return center_display_rgb(camera.img);
}

rt::RadianceFrame render_subsurface_sphere() {
    rt::SceneDescription scene;
    scene.background = Eigen::Vector3d::Ones();
    rt::OpenPbrCoreMaterial parameters;
    parameters.base_weight = 0.0f;
    parameters.specular_roughness = 0.0f;
    parameters.subsurface_weight = 1.0f;
    parameters.subsurface_color = {0.95f, 0.7f, 0.45f};
    parameters.subsurface_radius = 2.0f;
    parameters.subsurface_radius_scale = {1.0f, 0.5f, 0.25f};
    parameters.subsurface_scatter_anisotropy = 0.35f;
    const int material = scene.add_material(rt::OpenPbrMaterialDesc {
        .compiled = rt::OpenPbrCompiledMaterial {.parameters = parameters},
    });
    scene.add_sphere(rt::SpherePrimitive {material,
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}), 1.0, false});

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 32;
    profile.max_bounces = 12;
    profile.rr_start_bounce = 13;
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), profile, 0);
}

cv::Mat render_cpu_subsurface_sphere() {
    rt::OpenPbrCoreMaterial parameters;
    parameters.base_weight = 0.0f;
    parameters.specular_ior = 1.0f;
    parameters.specular_roughness = 0.0f;
    parameters.subsurface_weight = 1.0f;
    parameters.subsurface_color = {0.95f, 0.7f, 0.45f};
    parameters.subsurface_radius = 2.0f;
    parameters.subsurface_radius_scale = {1.0f, 0.5f, 0.25f};
    parameters.subsurface_scatter_anisotropy = 0.35f;

    HittableList world;
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d {0.0, 0.0, -3.0}, 1.0,
        pro::make_proxy_shared<Material, OpenPbrSurfaceMaterial>(
            rt::OpenPbrCompiledMaterial {.parameters = parameters},
            std::vector<pro::proxy<Texture>> {})));
    pro::proxy<Hittable> world_as_hittable = &world;

    Camera camera;
    camera.aspect_ratio = 1.0;
    camera.image_width = 16;
    camera.samples_per_pixel = 64;
    camera.max_depth = 16;
    camera.background = Vec3d::Ones();
    camera.vfov = 45.0;
    camera.lookfrom = Vec3d::Zero();
    camera.lookat = {0.0, 0.0, -3.0};
    camera.vup = {0.0, 1.0, 0.0};
    camera.defocus_angle = 0.0;
    camera.render(world_as_hittable);
    return camera.img;
}

std::string resolved_asset_path(const rt::scene::ScenePrim& prim) {
    for (const rt::scene::SceneAssetReference& asset : prim.asset_references) {
        if (!asset.resolved_path.empty()) {
            return asset.resolved_path;
        }
        if (!asset.evaluated_path.empty()) {
            return asset.evaluated_path;
        }
        if (!asset.authored_path.empty()) {
            return asset.authored_path;
        }
    }
    throw std::runtime_error("imported image texture has no asset path: " + prim.path);
}

std::vector<pro::proxy<Texture>> make_imported_cpu_textures(
    const rt::scene::SceneIRv2& scene, std::unordered_map<std::string, int>& texture_indices) {
    for (const rt::scene::ScenePrim& prim : scene.prims()) {
        if (prim.kind == rt::scene::ScenePrimKind::texture && prim.texture) {
            texture_indices.emplace(prim.path, static_cast<int>(texture_indices.size()));
        }
    }

    std::vector<pro::proxy<Texture>> textures(texture_indices.size());
    for (const rt::scene::ScenePrim& prim : scene.prims()) {
        const auto index = texture_indices.find(prim.path);
        if (index == texture_indices.end()) {
            continue;
        }
        if (prim.texture->node == rt::scene::SceneTextureNode::image) {
            textures[static_cast<std::size_t>(index->second)] =
                pro::make_proxy_shared<Texture, ImageTexture>(resolved_asset_path(prim));
        } else {
            textures[static_cast<std::size_t>(index->second)] =
                pro::make_proxy_shared<Texture, SolidColor>(prim.texture->value);
        }
    }
    return textures;
}

Eigen::Vector3d imported_cpu_direct_response(const rt::OpenPbrCompiledMaterial& compiled,
    const std::vector<pro::proxy<Texture>>& textures) {
    const pro::proxy<Material> material =
        pro::make_proxy_shared<Material, OpenPbrSurfaceMaterial>(compiled, textures);
    const Sphere sphere {Vec3d {0.0, 3.0, 0.0}, 1.0, material};
    const Ray ray {Vec3d::Zero(), Vec3d {0.0, 1.0, 0.0}};
    HitRecord hit;
    expect_true(sphere.hit(ray, Interval {0.001, infinity}, hit),
        "imported CPU material receiver is visible");
    constexpr double inverse_sqrt_two = 0.7071067811865475244;
    double pdf = 0.0;
    const Vec3d response =
        hit.mat->evaluate_direct(ray, hit, Vec3d {inverse_sqrt_two, -inverse_sqrt_two, 0.0}, pdf);
    expect_true(pdf > 0.0 && response.allFinite(), "imported CPU material response is finite");
    return response;
}

rt::RadianceFrame render_imported_scalar_image(rt::PackedScene scene) {
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 1;
    profile.max_bounces = 1;
    profile.enable_denoise = false;
    profile.enable_restir_di = false;
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene, make_test_rig().pack(), profile, 0);
}

void test_imported_scalar_image(const std::filesystem::path& fixture) {
    if (!rt::scene::openusd_stage_importer_available()) {
        return;
    }

    const rt::scene::SceneIRv2 scene = rt::scene::import_openusd_stage(fixture);
    const rt::scene::ScenePrim* material_prim = scene.find_prim("/World/Material");
    expect_true(material_prim != nullptr && material_prim->material.has_value(),
        "scalar image fixture imports its material");
    const auto& surface = std::get<rt::scene::SceneOpenPbrSurface>(*material_prim->material);

    std::unordered_map<std::string, int> texture_indices;
    const std::vector<pro::proxy<Texture>> textures =
        make_imported_cpu_textures(scene, texture_indices);
    rt::OpenPbrCompiledMaterial with_image =
        rt::scene::compile_openpbr_core_material(surface, scene, texture_indices);
    expect_true(with_image.scalar_textures.specular_roughness.texture_index >= 0,
        "ND_image_float compiles into the scalar binding table");
    const int roughness_index = with_image.scalar_textures.specular_roughness.texture_index;
    const double sampled_roughness = textures[static_cast<std::size_t>(roughness_index)]
                                         ->value(0.5, 0.5, Vec3d::Zero())
                                         .x();
    expect_near(sampled_roughness, 0.6, 1.0 / 255.0,
        "CPU proxy reads the raw scalar image channel");

    rt::OpenPbrCompiledMaterial without_image = with_image;
    without_image.scalar_textures.specular_roughness.texture_index = -1;
    const Eigen::Vector3d cpu_with = imported_cpu_direct_response(with_image, textures);
    const Eigen::Vector3d cpu_without = imported_cpu_direct_response(without_image, textures);
    expect_true((cpu_with - cpu_without).norm() > 1e-4,
        "ND_image_float changes the CPU OpenPBR response");

    rt::SceneDescription realtime = rt::scene::adapt_scene_ir_v2_to_realtime(scene);
    realtime.background = Eigen::Vector3d::Zero();
    constexpr double inverse_sqrt_two = 0.7071067811865475244;
    rt::AnalyticLightDesc light;
    light.type = rt::AnalyticLightType::distant;
    light.radiance = Eigen::Vector3d::Constant(12.0);
    light.local_to_world_linear.col(2) =
        Eigen::Vector3d {inverse_sqrt_two, -inverse_sqrt_two, 0.0};
    light.cos_theta_max = 1.0;
    light.selection_pdf = 1.0;
    light.cdf = 1.0;
    light.delta = true;
    realtime.add_analytic_light(light);

    rt::PackedScene gpu_with_scene = realtime.pack();
    rt::PackedScene gpu_without_scene = gpu_with_scene;
    for (rt::MaterialDesc& material : gpu_without_scene.materials) {
        if (auto* openpbr = std::get_if<rt::OpenPbrMaterialDesc>(&material)) {
            openpbr->compiled.scalar_textures.specular_roughness.texture_index = -1;
        }
    }
    const Eigen::Vector3d gpu_with = center_pixel_rgb(render_imported_scalar_image(gpu_with_scene));
    const Eigen::Vector3d gpu_without =
        center_pixel_rgb(render_imported_scalar_image(gpu_without_scene));
    expect_true((gpu_with - gpu_without).norm() > 1e-4,
        "ND_image_float changes the OptiX OpenPBR response");
}

} // namespace

int main(int argc, char** argv) {
    expect_true(argc == 2, "scalar image fixture argument");
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);
    const OpenPbrReferenceScene reference = make_reference_scene();

    const rt::scene::CpuSceneAdapterResult cpu =
        rt::scene::adapt_to_cpu_openpbr(reference.compatibility, reference.scene_v2);
    const Ray primary {Vec3d::Zero(), Vec3d {0.0, 1.0, 0.0}};
    HitRecord hit;
    expect_true(cpu.world->hit(primary, Interval {0.001, infinity}, hit),
        "CPU OpenPBR reference ray hits the sphere");
    const Vec3d emitted = hit.mat->emitted(primary, hit, hit.u, hit.v, hit.p);
    ScatterRecord scatter;
    expect_true(hit.mat->scatter(primary, hit, scatter), "CPU OpenPBR reference material scatters");
    expect_true(scatter.skip_pdf, "CPU OpenPBR core owns the sampled direction and weight");
    const rt::OpenPbrVec3 decoded_base = rt::openpbr_source_to_linear({0.5f, 0.25f, 0.75f},
        rt::OpenPbrSourceColorSpace::srgb_texture);
    expect_vec3_near(scatter.attenuation, Vec3d {decoded_base.x, decoded_base.y, decoded_base.z},
        1e-5, "CPU OpenPBR connected diffuse throughput");
    expect_vec3_near(emitted, Vec3d {1.0, 0.5, 0.25}, 1e-6, "CPU OpenPBR emission");
    double direct_pdf = 0.0;
    const Vec3d direct_response = hit.mat->evaluate_direct(primary, hit, hit.normal, direct_pdf);
    expect_true(direct_pdf > 0.0 && direct_response.maxCoeff() > 0.0,
        "CPU OpenPBR evaluates a finite direct-light response for analytic NEE");
    const Vec3d cpu_reference = emitted + scatter.attenuation;

    rt::SceneDescription gpu_scene =
        rt::scene::adapt_to_realtime_openpbr(reference.compatibility, reference.scene_v2);
    gpu_scene.background = Eigen::Vector3d::Ones();
    rt::PackedScene packed_gpu_scene = gpu_scene.pack();
    const auto& gpu_material =
        std::get<rt::OpenPbrMaterialDesc>(packed_gpu_scene.materials.front());
    expect_true(gpu_material.compiled.scalar_textures.base_metalness.texture_index == 2
                    && gpu_material.compiled.scalar_textures.specular_roughness.texture_index == 3,
        "SceneIR v2 scalar bindings reach the realtime material table");
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 2;
    profile.rr_start_bounce = 3;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame gpu =
        renderer.render_radiance(packed_gpu_scene, make_test_rig().pack(), profile, 0);
    const Eigen::Vector3d gpu_reference = center_pixel_rgb(gpu);
    expect_vec3_near(gpu_reference, cpu_reference, 5e-4,
        "SceneIR v2 connected OpenPBR CPU and GPU linear reference agreement");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(false)) > 0.01,
        "OpenPBR direct response uses the shared BSDF instead of base-color fallback");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(true)) > 0.01,
        "OpenPBR emissive geometry participates in direct lighting");
    expect_true(
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::sphere, false))
            > 0.01,
        "analytic sphere light reaches the production GPU integrator");
    expect_true(
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::disk, false))
            > 0.01,
        "analytic disk light reaches the production GPU integrator");
    const double rect_unoccluded =
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::rect, false));
    const double rect_occluded =
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::rect, true));
    expect_true(rect_unoccluded > 0.01,
        "analytic rect light reaches the production GPU integrator");
    expect_true(rect_occluded < rect_unoccluded * 0.2,
        "analytic rect light respects finite shadow visibility");
    expect_true(
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::cylinder, false))
            > 0.01,
        "analytic cylinder light reaches the production GPU integrator");
    expect_true(
        center_pixel_luminance(render_analytic_direct_light(rt::AnalyticLightType::distant, false))
            > 0.01,
        "analytic distant light reaches the production GPU integrator");
    const Eigen::Vector3d dome_rgb = center_pixel_rgb(render_analytic_dome_miss());
    expect_vec3_near(dome_rgb, Eigen::Vector3d {0.25, 0.5, 1.0}, 5e-4,
        "analytic dome contributes miss radiance");
    expect_true(render_cpu_analytic_direct_light(rt::AnalyticLightType::sphere, false) > 0.01,
        "analytic sphere light reaches the production CPU integrator");
    expect_true(render_cpu_analytic_direct_light(rt::AnalyticLightType::disk, false) > 0.01,
        "analytic disk light reaches the production CPU integrator");
    const double cpu_rect_unoccluded =
        render_cpu_analytic_direct_light(rt::AnalyticLightType::rect, false);
    const double cpu_rect_occluded =
        render_cpu_analytic_direct_light(rt::AnalyticLightType::rect, true);
    expect_true(cpu_rect_unoccluded > 0.01,
        "analytic rect light reaches the production CPU integrator");
    expect_true(cpu_rect_occluded < cpu_rect_unoccluded * 0.25,
        "analytic rect light respects CPU finite shadow visibility");
    expect_true(render_cpu_analytic_direct_light(rt::AnalyticLightType::cylinder, false) > 0.01,
        "analytic cylinder light reaches the production CPU integrator");
    expect_true(render_cpu_analytic_direct_light(rt::AnalyticLightType::distant, false) > 0.01,
        "analytic distant light reaches the production CPU integrator");
    expect_true(render_cpu_analytic_direct_light(rt::AnalyticLightType::dome, false) > 0.01,
        "analytic dome light reaches the production CPU direct-light integrator");
    expect_vec3_near(render_cpu_analytic_dome_miss(),
        Eigen::Vector3d {std::sqrt(0.25), std::sqrt(0.5), 1.0}, 1.0 / 128.0,
        "analytic dome contributes CPU display-space miss radiance");

    rt::OpenPbrCoreMaterial subsurface_parameters;
    subsurface_parameters.base_weight = 0.0f;
    subsurface_parameters.specular_roughness = 0.0f;
    subsurface_parameters.subsurface_weight = 1.0f;
    subsurface_parameters.subsurface_color = {0.9f, 0.6f, 0.3f};
    subsurface_parameters.subsurface_radius = 1.0f;
    subsurface_parameters.subsurface_scatter_anisotropy = 0.35f;
    const OpenPbrSurfaceMaterial subsurface_material {
        rt::OpenPbrCompiledMaterial {.parameters = subsurface_parameters}, {}};
    const Ray entering_ray {Vec3d {0.0, 0.0, 1.0}, Vec3d {0.0, 0.0, -1.0}};
    HitRecord entry_hit;
    entry_hit.p = Vec3d::Zero();
    entry_hit.u = 0.0;
    entry_hit.v = 0.0;
    entry_hit.set_face_normal(entering_ray, Vec3d {0.0, 0.0, 1.0});
    ScatterRecord entry_scatter;
    bool entered = false;
    for (int attempt = 0; attempt < 128 && !entered; ++attempt) {
        entered = subsurface_material.scatter(entering_ray, entry_hit, entry_scatter)
                  && entry_scatter.skip_pdf_ray.subsurface_medium().active != 0;
    }
    expect_true(entered, "CPU OpenPBR production material enters random-walk medium state");
    expect_true(entry_scatter.skip_pdf_ray.subsurface_owner() == &subsurface_material,
        "CPU random-walk ray retains its material owner");

    const OpenPbrSurfaceMaterial other_subsurface_material {
        rt::OpenPbrCompiledMaterial {.parameters = subsurface_parameters}, {}};
    ScatterRecord wrong_owner_scatter;
    expect_true(!other_subsurface_material.scatter(entry_scatter.skip_pdf_ray, entry_hit,
                    wrong_owner_scatter),
        "CPU random walk cannot cross a different OpenPBR material boundary");

    const Ray exiting_ray {Vec3d::Zero(), Vec3d {0.0, 0.0, 1.0}, 0.0,
        entry_scatter.skip_pdf_ray.subsurface_medium(),
        entry_scatter.skip_pdf_ray.subsurface_owner()};
    HitRecord exit_hit;
    exit_hit.p = Vec3d {0.0, 0.0, 1.0};
    exit_hit.u = 0.0;
    exit_hit.v = 0.0;
    exit_hit.set_face_normal(exiting_ray, Vec3d {0.0, 0.0, 1.0});
    ScatterRecord exit_scatter;
    bool exited = false;
    for (int attempt = 0; attempt < 128 && !exited; ++attempt) {
        exited = subsurface_material.scatter(exiting_ray, exit_hit, exit_scatter)
                 && exit_scatter.skip_pdf_ray.subsurface_medium().active == 0;
    }
    expect_true(exited, "CPU OpenPBR production material exits random-walk medium state");
    expect_true(exit_scatter.skip_pdf_ray.subsurface_owner() == nullptr,
        "CPU random-walk material owner clears on exit");

    const Eigen::Vector3d subsurface_gpu = center_pixel_rgb(render_subsurface_sphere());
    expect_true(subsurface_gpu.allFinite() && (subsurface_gpu.array() >= 0.0).all(),
        "GPU random-walk subsurface render remains finite and non-negative");
    expect_true(subsurface_gpu.maxCoeff() > 0.01,
        "GPU random-walk subsurface transports environment energy through a closed sphere");
    expect_true(subsurface_gpu.maxCoeff() <= 1.25,
        "GPU random-walk subsurface stays within the finite white-furnace bound");
    const cv::Mat subsurface_cpu = render_cpu_subsurface_sphere();
    expect_true(!subsurface_cpu.empty(), "CPU random-walk subsurface render produces an image");
    const cv::Vec3b subsurface_cpu_center =
        subsurface_cpu.at<cv::Vec3b>(subsurface_cpu.rows / 2, subsurface_cpu.cols / 2);
    expect_true(
        std::max({subsurface_cpu_center[0], subsurface_cpu_center[1], subsurface_cpu_center[2]})
            > 0,
        "CPU random-walk subsurface transports white-environment energy through a closed sphere");
    test_imported_scalar_image(argv[1]);
    return 0;
}
