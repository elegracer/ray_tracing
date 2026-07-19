#include "common/interval.h"
#include "common/material.h"
#include "common/ray.h"
#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/realtime_scene_adapter.h"
#include "test_support.h"

namespace {

constexpr int kImageSize = 64;
constexpr int kCenterPixel = kImageSize / 2;

struct OpenPbrReferenceScene {
    rt::scene::SceneIR compatibility;
    rt::scene::SceneIRv2 scene_v2;
};

OpenPbrReferenceScene make_reference_scene() {
    OpenPbrReferenceScene scene;
    const int legacy_texture = scene.compatibility.add_texture(
        rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Zero()});
    const int legacy_material = scene.compatibility.add_material(
        rt::scene::DiffuseMaterial {.albedo_texture = legacy_texture});
    const int sphere = scene.compatibility.add_shape(rt::scene::SphereShape {
        .center = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}),
        .radius = 1.0,
    });
    scene.compatibility.add_instance(rt::scene::SurfaceInstance {
        .shape_index = sphere,
        .material_index = legacy_material,
    });

    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.scene_v2.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    rt::scene::SceneOpenPbrSurface surface;
    surface.base_color = Eigen::Vector3d {0.2, 0.4, 0.6};
    surface.specular_weight = 0.0;
    surface.emission_luminance = 2.0;
    surface.emission_color = Eigen::Vector3d {0.5, 0.25, 0.125};
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
    const int receiver =
        scene.add_material(rt::OpenPbrMaterialDesc {.parameters = receiver_parameters});

    int light = -1;
    if (openpbr_light) {
        rt::OpenPbrCoreMaterial light_parameters;
        light_parameters.base_weight = 0.0f;
        light_parameters.specular_weight = 0.0f;
        light_parameters.emission_luminance = 20.0f;
        light = scene.add_material(rt::OpenPbrMaterialDesc {.parameters = light_parameters});
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

} // namespace

int main() {
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
    expect_vec3_near(scatter.attenuation, Vec3d {0.2, 0.4, 0.6}, 1e-5,
        "CPU OpenPBR diffuse throughput");
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
        "SceneIR v2 OpenPBR CPU and GPU linear reference agreement");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(false)) > 0.01,
        "OpenPBR direct response uses the shared BSDF instead of base-color fallback");
    expect_true(center_pixel_luminance(render_openpbr_direct_light(true)) > 0.01,
        "OpenPBR emissive geometry participates in direct lighting");
    return 0;
}
