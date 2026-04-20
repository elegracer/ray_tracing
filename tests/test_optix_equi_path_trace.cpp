#include "realtime/camera_models.h"
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <array>
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

rt::CameraRig make_mixed_model_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {90.0, 92.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 64, 64);
    rig.add_equi62(
        rt::make_equi62_lut1d_params(64, 64, 55.0, 55.0, 32.0, 32.0, std::array<double, 6> {}, Eigen::Vector2d::Zero()),
        Sophus::SE3d(), 64, 64);
    return rig;
}

rt::SceneDescription make_lit_scene() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.7, 0.3, 0.2}});
    scene.add_quad(rt::QuadPrimitive {
        light,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-0.75, 1.25, -1.5}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {1.5, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.5}),
        false,
    });
    scene.add_sphere(
        rt::SpherePrimitive {diffuse, rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5, false});
    return scene;
}

rt::SceneDescription make_background_only_scene() {
    rt::SceneDescription scene;
    scene.background = Eigen::Vector3d {0.40, 0.55, 0.70};
    const int hidden = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.2, 0.2, 0.2}});
    scene.add_sphere(rt::SpherePrimitive {hidden, Eigen::Vector3d {1000.0, 1000.0, 1000.0}, 1.0, false});
    return scene;
}

rt::RenderProfile make_profile() {
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 4;
    return profile;
}

}  // namespace

int main() {
    const rt::CameraRig rig = make_mixed_model_rig();
    const rt::PackedCameraRig packed = rig.pack();
    rt::OptixRenderer renderer;

    const rt::RadianceFrame background_frame =
        renderer.render_radiance(make_background_only_scene().pack(), packed, make_profile(), 1);
    expect_near(static_cast<double>(background_frame.width), 64.0, 1e-12, "equi background width");
    expect_near(static_cast<double>(background_frame.height), 64.0, 1e-12, "equi background height");
    expect_true(background_frame.average_luminance > 0.5, "equi background rays should preserve non-black background");

    const rt::RadianceFrame frame = renderer.render_radiance(make_lit_scene().pack(), packed, make_profile(), 1);
    expect_near(static_cast<double>(frame.width), 64.0, 1e-12, "equi radiance width");
    expect_near(static_cast<double>(frame.height), 64.0, 1e-12, "equi radiance height");
    expect_true(frame.average_luminance > 0.01, "equi radiance should be non-black");
    expect_true(!frame.beauty_rgba.empty(), "equi beauty buffer present");
    expect_true(!frame.normal_rgba.empty(), "equi normal buffer present");
    expect_true(!frame.albedo_rgba.empty(), "equi albedo buffer present");
    expect_true(!frame.depth.empty(), "equi depth buffer present");
    expect_true(has_variation(frame.depth, 1e-6f), "equi depth varies across frame");
    return 0;
}
