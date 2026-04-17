#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/gpu/renderer_pool.h"
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

template <typename Fn>
void expect_throws(Fn&& fn, const std::string& label) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    expect_true(threw, label);
}

rt::SceneDescription make_scene() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, Eigen::Vector3d {0.0, 0.0, -1.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-0.75, 1.25, -1.5},
        Eigen::Vector3d {1.5, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.5},
        false,
    });
    return scene;
}

rt::CameraRig make_rig(int camera_count) {
    rt::CameraRig rig;
    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = Eigen::Vector3d {0.03 * static_cast<double>(i), 0.0, 0.0};
        rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
            T_bc, 32, 32);
    }
    return rig;
}

}  // namespace

int main() {
    const rt::PackedScene packed_scene = make_scene().pack();
    const rt::PackedCameraRig packed_rig = make_rig(4).pack();
    const rt::RenderProfile profile = rt::RenderProfile::realtime();

    expect_throws([]() { rt::RendererPool invalid_zero(0); }, "reject renderer_count below range");
    expect_throws([]() { rt::RendererPool invalid_over(5); }, "reject renderer_count above range");

    rt::RendererPool pool(4);
    pool.prepare_scene(packed_scene);
    const std::vector<rt::CameraRenderResult> pooled = pool.render_frame(packed_rig, profile, 4);

    expect_true(pooled.size() == 4, "pooled camera count");
    for (int i = 0; i < 4; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        expect_true(pooled[idx].camera_index == i, "pooled camera ordering");

        rt::OptixRenderer baseline;
        baseline.prepare_scene(packed_scene);
        const rt::ProfiledRadianceFrame expected = baseline.render_prepared_radiance(packed_rig, profile, i);

        expect_vector_near(
            pooled[idx].profiled.frame.beauty_rgba, expected.frame.beauty_rgba, 1e-6, "pool beauty parity " + std::to_string(i));
        expect_vector_near(
            pooled[idx].profiled.frame.normal_rgba, expected.frame.normal_rgba, 1e-6, "pool normal parity " + std::to_string(i));
        expect_vector_near(
            pooled[idx].profiled.frame.albedo_rgba, expected.frame.albedo_rgba, 1e-6, "pool albedo parity " + std::to_string(i));
        expect_vector_near(
            pooled[idx].profiled.frame.depth, expected.frame.depth, 1e-6, "pool depth parity " + std::to_string(i));
        expect_near(pooled[idx].profiled.frame.average_luminance, expected.frame.average_luminance, 1e-9,
            "pool luminance parity " + std::to_string(i));
    }

    expect_throws([&]() { pool.render_frame(packed_rig, profile, 0); }, "reject active_cameras below range");
    expect_throws([&]() { pool.render_frame(packed_rig, profile, 5); }, "reject active_cameras above range");

    return 0;
}
