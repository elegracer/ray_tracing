#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <cmath>

namespace {

void expect_vector_near(const std::vector<float>& actual, const std::vector<float>& expected,
    double tol, const std::string& label) {
    expect_true(actual.size() == expected.size(), label + " size");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        expect_near(actual[i], expected[i], tol, label + " value[" + std::to_string(i) + "]");
    }
}

} // namespace

int main() {
    rt::SceneDescription scene;
    const int diffuse =
        scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.2, 0.2}});
    const int light =
        scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    scene.add_sphere(rt::SpherePrimitive {diffuse,
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5, false});
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

    rt::OptixRenderer renderer;
    const rt::RenderProfile profile = rt::RenderProfile::quality();
    const rt::RadianceFrame baseline =
        renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);
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
    expect_near(profiled.frame.average_luminance, baseline.average_luminance, 1e-9,
        "luminance parity");
    expect_true(profiled.timing.render_ms >= 0.0f, "profiled render timing non-negative");
    expect_near(profiled.timing.denoise_ms, 0.0, 1e-12, "disabled denoise timing is zero");
    expect_true(profiled.timing.download_ms >= 0.0f, "profiled download timing non-negative");

    rt::OptixRenderer denoised_renderer;
    const rt::RenderProfile denoised_profile = rt::RenderProfile::realtime();
    const rt::ProfiledRadianceFrame denoised_first =
        denoised_renderer.render_radiance_profiled(scene.pack(), rig.pack(), denoised_profile, 0);
    const rt::ProfiledRadianceFrame denoised_second =
        denoised_renderer.render_radiance_profiled(scene.pack(), rig.pack(), denoised_profile, 0);
    for (const rt::ProfiledRadianceFrame* temporal_frame : {&denoised_first, &denoised_second}) {
        expect_true(temporal_frame->timing.denoise_ms > 0.0f, "enabled denoise timing is positive");
        expect_true(!temporal_frame->frame.beauty_rgba.empty(), "denoised beauty present");
        expect_true(std::isfinite(temporal_frame->frame.average_luminance),
            "denoised luminance finite");
        for (float value : temporal_frame->frame.beauty_rgba) {
            expect_true(std::isfinite(value), "denoised beauty finite");
        }
    }

    denoised_renderer.reset_accumulation();
    const rt::ProfiledRadianceFrame reset_sequence =
        denoised_renderer.render_radiance_profiled(scene.pack(), rig.pack(), denoised_profile, 0);
    expect_true(reset_sequence.timing.denoise_ms > 0.0f,
        "reset sequence denoise timing is positive");
    expect_true(std::isfinite(reset_sequence.frame.average_luminance),
        "reset sequence luminance finite");

    rt::OptixRenderer seeded_renderer;
    seeded_renderer.prepare_scene(scene.pack());
    seeded_renderer.reset_sequence(1234U);
    const rt::RadianceFrame seeded_first =
        seeded_renderer.render_prepared_radiance(rig.pack(), profile, 0).frame;
    seeded_renderer.reset_sequence(1234U);
    const rt::RadianceFrame seeded_repeat =
        seeded_renderer.render_prepared_radiance(rig.pack(), profile, 0).frame;
    expect_vector_near(seeded_repeat.beauty_rgba, seeded_first.beauty_rgba, 0.0,
        "reset_sequence reproduces the same seeded beauty frame");
    return 0;
}
