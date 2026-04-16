#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {0.0, 0.0, -1.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-0.75, 1.25, -1.5},
        Eigen::Vector3d {1.5, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.5},
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 32, 32);

    rt::OptixRenderer renderer;
    const rt::ProfiledRadianceFrame profiled =
        renderer.render_radiance_profiled(scene.pack(), rig.pack(), rt::RenderProfile::realtime(), 0);

    expect_true(profiled.frame.width == 32, "profiled frame width");
    expect_true(profiled.frame.height == 32, "profiled frame height");
    expect_true(!profiled.frame.beauty_rgba.empty(), "profiled beauty present");
    expect_true(profiled.timing.render_ms >= 0.0f, "profiled render timing non-negative");
    expect_true(profiled.timing.download_ms >= 0.0f, "profiled download timing non-negative");
    return 0;
}
