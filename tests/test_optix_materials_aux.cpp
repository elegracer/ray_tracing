#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <cmath>
#include <vector>

namespace {

bool has_variation(const std::vector<float>& values, float epsilon) {
    if (values.empty()) {
        return false;
    }
    const float first = values.front();
    for (float value : values) {
        if (std::abs(value - first) > epsilon) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.7, 0.2, 0.2}});
    const int metal = scene.add_material(rt::MetalMaterial {Eigen::Vector3d {0.9, 0.9, 0.9}, 0.02});
    const int glass = scene.add_material(rt::DielectricMaterial {1.5});

    scene.add_quad(rt::QuadPrimitive {light,
        Eigen::Vector3d {-1.0, 1.5, -3.0},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, -2.0}, false});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {-0.8, -0.2, -3.5}, 0.5, false});
    scene.add_sphere(rt::SpherePrimitive {metal, Eigen::Vector3d {0.0, -0.2, -3.5}, 0.5, false});
    scene.add_sphere(rt::SpherePrimitive {glass, Eigen::Vector3d {0.8, -0.2, -3.5}, 0.5, false});

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {220.0, 220.0, 48.0, 48.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 96, 96);

    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(
        scene.pack(), rig.pack(), rt::RenderProfile::quality(), 0);

    expect_true(frame.average_luminance > 0.02, "beauty is lit");
    expect_true(!frame.normal_rgba.empty(), "normal buffer present");
    expect_true(!frame.albedo_rgba.empty(), "albedo buffer present");
    expect_true(!frame.depth.empty(), "depth buffer present");
    expect_true(frame.normal_rgba != frame.beauty_rgba, "normal differs from beauty");
    expect_true(has_variation(frame.depth, 1e-6f), "depth varies across the frame");
    return 0;
}
