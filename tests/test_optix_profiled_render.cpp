#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

namespace {

void expect_vector_near(const std::vector<float>& actual, const std::vector<float>& expected, double tol,
    const std::string& label) {
    expect_true(actual.size() == expected.size(), label + " size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        expect_near(actual[i], expected[i], tol, label + " value[" + std::to_string(i) + "]");
    }
}

}  // namespace

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
    const rt::RenderProfile profile = rt::RenderProfile::realtime();
    const rt::RadianceFrame baseline = renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);
    const rt::ProfiledRadianceFrame profiled =
        renderer.render_radiance_profiled(scene.pack(), rig.pack(), profile, 0);

    expect_true(profiled.frame.width == 32, "profiled frame width");
    expect_true(profiled.frame.height == 32, "profiled frame height");
    expect_true(!profiled.frame.beauty_rgba.empty(), "profiled beauty present");
    expect_true(!profiled.frame.normal_rgba.empty(), "profiled normal present");
    expect_true(!profiled.frame.albedo_rgba.empty(), "profiled albedo present");
    expect_true(!profiled.frame.depth.empty(), "profiled depth present");
    expect_vector_near(profiled.frame.beauty_rgba, baseline.beauty_rgba, 1e-6, "beauty parity");
    expect_vector_near(profiled.frame.normal_rgba, baseline.normal_rgba, 1e-6, "normal parity");
    expect_vector_near(profiled.frame.albedo_rgba, baseline.albedo_rgba, 1e-6, "albedo parity");
    expect_vector_near(profiled.frame.depth, baseline.depth, 1e-6, "depth parity");
    expect_near(profiled.frame.average_luminance, baseline.average_luminance, 1e-9, "luminance parity");
    expect_true(profiled.timing.render_ms >= 0.0f, "profiled render timing non-negative");
    expect_true(profiled.timing.download_ms >= 0.0f, "profiled download timing non-negative");
    return 0;
}
