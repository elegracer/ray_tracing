#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    const rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    expect_near(static_cast<double>(profile.samples_per_pixel), 1.0, 1e-12, "default spp");
    expect_near(static_cast<double>(profile.max_bounces), 4.0, 1e-12, "default bounces");

    rt::SceneDescription scene;
    scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    scene.add_sphere(rt::SpherePrimitive {0, Eigen::Vector3d {0.0, 0.0, -3.0}, 1.0, false});
    scene.add_quad(rt::QuadPrimitive {
        0,
        Eigen::Vector3d {-1.0, -1.0, -5.0},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 2.0, 0.0},
        true,
    });

    const rt::PackedScene packed = scene.pack();
    expect_near(static_cast<double>(packed.material_count), 1.0, 1e-12, "material count");
    expect_near(static_cast<double>(packed.sphere_count), 1.0, 1e-12, "sphere count");
    expect_near(static_cast<double>(packed.quad_count), 1.0, 1e-12, "quad count");
    return 0;
}
