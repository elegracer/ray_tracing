#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
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

void expect_frame_near(const rt::RadianceFrame& actual, const rt::RadianceFrame& expected, const std::string& label) {
    expect_true(actual.width == expected.width, label + " width");
    expect_true(actual.height == expected.height, label + " height");
    expect_vector_near(actual.beauty_rgba, expected.beauty_rgba, 1e-6, label + " beauty");
    expect_vector_near(actual.normal_rgba, expected.normal_rgba, 1e-6, label + " normal");
    expect_vector_near(actual.albedo_rgba, expected.albedo_rgba, 1e-6, label + " albedo");
    expect_vector_near(actual.depth, expected.depth, 1e-6, label + " depth");
    expect_near(actual.average_luminance, expected.average_luminance, 1e-9, label + " luminance");
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

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse, rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5, false});
    scene.add_quad(rt::QuadPrimitive {
        light,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-0.75, 1.25, -1.5}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {1.5, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.5}),
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 32, 32);

    const rt::PackedScene packed_scene = scene.pack();
    const rt::PackedCameraRig packed_rig = rig.pack();
    const rt::RenderProfile profile = rt::RenderProfile::realtime();

    rt::OptixRenderer renderer;
    renderer.prepare_scene(packed_scene);
    const rt::ProfiledRadianceFrame prepared_first = renderer.render_prepared_radiance(packed_rig, profile, 0);
    const rt::ProfiledRadianceFrame prepared_second = renderer.render_prepared_radiance(packed_rig, profile, 0);
    const rt::ProfiledRadianceFrame fallback =
        renderer.render_radiance_profiled(packed_scene, packed_rig, profile, 0);

    expect_frame_near(prepared_second.frame, prepared_first.frame, "prepared repeat parity");
    expect_frame_near(fallback.frame, prepared_first.frame, "fallback parity");
    expect_true(prepared_first.timing.render_ms >= 0.0f, "prepared first render timing non-negative");
    expect_true(prepared_first.timing.download_ms >= 0.0f, "prepared first download timing non-negative");
    expect_true(prepared_second.timing.render_ms >= 0.0f, "prepared repeat render timing non-negative");
    expect_true(prepared_second.timing.download_ms >= 0.0f, "prepared repeat download timing non-negative");
    expect_true(fallback.timing.render_ms >= 0.0f, "fallback render timing non-negative");
    expect_true(fallback.timing.download_ms >= 0.0f, "fallback download timing non-negative");

    rt::OptixRenderer no_prepare_renderer;
    expect_throws([&]() { no_prepare_renderer.render_prepared_radiance(packed_rig, profile, 0); },
        "prepared render requires prepare_scene");

    rt::OptixRenderer invalid_wrapper_renderer;
    expect_throws([&]() { invalid_wrapper_renderer.render_radiance_profiled(packed_scene, packed_rig, profile, 1); },
        "profiled wrapper rejects invalid camera index");
    expect_throws([&]() { invalid_wrapper_renderer.render_prepared_radiance(packed_rig, profile, 0); },
        "failed profiled wrapper must not leave prepared state behind");

    rt::OptixRenderer invalidation_renderer;
    invalidation_renderer.prepare_scene(packed_scene);
    invalidation_renderer.render_radiance(packed_scene, packed_rig, profile, 0);
    expect_throws([&]() { invalidation_renderer.render_prepared_radiance(packed_rig, profile, 0); },
        "legacy render invalidates prepared state");
    return 0;
}
