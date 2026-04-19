#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {20.0, 20.0, 20.0}});
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.8, 0.8}});
    scene.add_triangle(rt::TrianglePrimitive {
        .material_index = light,
        .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, 1.2, 1.0}),
        .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, 1.2, 1.0}),
        .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 2.4, 1.0}),
        .dynamic = false,
    });
    scene.add_quad(rt::QuadPrimitive {
        diffuse,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -4.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {200.0, 200.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 64, 64);

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 1;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);

    expect_near(static_cast<double>(frame.width), 64.0, 1e-12, "radiance width");
    expect_near(static_cast<double>(frame.height), 64.0, 1e-12, "radiance height");
    expect_true(frame.average_luminance > 0.05, "triangle-lit direct lighting should be visible");
    return 0;
}
