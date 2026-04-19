#include "realtime/frame_convention.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    const rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    expect_near(static_cast<double>(profile.samples_per_pixel), 2.0, 1e-12, "default spp");
    expect_near(static_cast<double>(profile.max_bounces), 4.0, 1e-12, "default bounces");

    rt::SceneDescription scene;
    scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    scene.add_sphere(rt::SpherePrimitive {0, rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}), 1.0, false});
    scene.add_quad(rt::QuadPrimitive {
        0,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-1.0, -1.0, -5.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {2.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 2.0, 0.0}),
        true,
    });
    scene.add_triangle(rt::TrianglePrimitive {
        .material_index = 0,
        .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -2.0}),
        .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {1.0, 0.0, -2.0}),
        .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 1.0, -2.0}),
        .dynamic = false,
    });

    const rt::PackedScene packed = scene.pack();
    expect_near(static_cast<double>(packed.material_count), 1.0, 1e-12, "material count");
    expect_near(static_cast<double>(packed.sphere_count), 1.0, 1e-12, "sphere count");
    expect_near(static_cast<double>(packed.quad_count), 1.0, 1e-12, "quad count");
    expect_near(static_cast<double>(packed.triangle_count), 1.0, 1e-12, "triangle count");
    return 0;
}
