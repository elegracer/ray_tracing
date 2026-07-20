#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/interval.h"
#include "common/material.h"
#include "common/ray.h"
#include "common/sphere.h"
#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/realtime_scene_adapter.h"
#include "test_support.h"

#include <tbb/global_control.h>

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
    const int base_texture =
        scene.compatibility.add_texture(rt::scene::ConstantColorTextureDesc {.color = base_source});
    const int emission_texture = scene.compatibility.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = emission_source});
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
    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    rt::scene::SceneOpenPbrSurface surface;
    surface.specular_weight = 0.0;
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

} // namespace

int main() {
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
    const Vec3d cpu_reference = emitted + scatter.attenuation;

    rt::SceneDescription gpu_scene =
        rt::scene::adapt_to_realtime_openpbr(reference.compatibility, reference.scene_v2);
    gpu_scene.background = Eigen::Vector3d::Ones();
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 2;
    profile.rr_start_bounce = 3;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame gpu =
        renderer.render_radiance(gpu_scene.pack(), make_test_rig().pack(), profile, 0);
    const Eigen::Vector3d gpu_reference = center_pixel_rgb(gpu);
    expect_vec3_near(gpu_reference, cpu_reference, 5e-4,
        "SceneIR v2 connected OpenPBR CPU and GPU linear reference agreement");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(false)) > 0.01,
        "OpenPBR direct response uses the shared BSDF instead of base-color fallback");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(true)) > 0.01,
        "OpenPBR emissive geometry participates in direct lighting");

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
    expect_true(!other_subsurface_material.scatter(
                    entry_scatter.skip_pdf_ray, entry_hit, wrong_owner_scatter),
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
    expect_true(std::max({subsurface_cpu_center[0], subsurface_cpu_center[1],
                    subsurface_cpu_center[2]})
                    > 0,
        "CPU random-walk subsurface transports white-environment energy through a closed sphere");
    return 0;
}
