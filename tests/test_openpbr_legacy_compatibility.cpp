#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/scene_ir_v2.h"
#include "scene/shared_scene_ir.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kImageSize = 64;
constexpr float kSubjectDepthLimit = 4.5f;

enum class LegacyMaterialKind {
    diffuse,
    metal,
    dielectric,
    emissive,
    volume,
};

struct CompatibilityCase {
    LegacyMaterialKind kind;
    const char* name;
    double max_mae;
    double max_p99_error;
};

struct ImageError {
    double mae = 0.0;
    double p99 = 0.0;
    double peak = 0.0;
    std::size_t subject_pixels = 0;
};

rt::scene::SceneIR make_legacy_material_scene(LegacyMaterialKind kind) {
    rt::scene::SceneIR scene;
    const int warm = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {4.0, 0.8, 0.2},
    });
    const int cool = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {0.2, 0.8, 4.0},
    });
    const int backdrop_texture = scene.add_texture(rt::scene::CheckerTextureDesc {
        .scale = 0.75,
        .even_texture = warm,
        .odd_texture = cool,
    });
    const int subject_texture = scene.add_texture(rt::scene::ConstantColorTextureDesc {
        .color = Eigen::Vector3d {0.65, 0.3, 0.12},
    });

    int subject_material = -1;
    switch (kind) {
        case LegacyMaterialKind::diffuse:
            subject_material = scene.add_material(
                rt::scene::DiffuseMaterial {.albedo_texture = subject_texture});
            break;
        case LegacyMaterialKind::metal:
            subject_material = scene.add_material(rt::scene::MetalMaterial {
                .albedo_texture = subject_texture,
                .fuzz = 0.35,
            });
            break;
        case LegacyMaterialKind::dielectric:
            subject_material =
                scene.add_material(rt::scene::DielectricMaterial {.ior = 1.5});
            break;
        case LegacyMaterialKind::emissive:
            subject_material = scene.add_material(
                rt::scene::EmissiveMaterial {.emission_texture = subject_texture});
            break;
        case LegacyMaterialKind::volume:
            subject_material = scene.add_material(
                rt::scene::IsotropicVolumeMaterial {.albedo_texture = subject_texture});
            break;
    }
    const int backdrop_material = scene.add_material(
        rt::scene::EmissiveMaterial {.emission_texture = backdrop_texture});

    const int sphere = scene.add_shape(rt::scene::SphereShape {
        .center = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}),
        .radius = 1.0,
    });
    const int backdrop = scene.add_shape(rt::scene::QuadShape {
        .origin = rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -6.0}),
        .edge_u = rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        .edge_v = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
    });
    if (kind == LegacyMaterialKind::volume) {
        scene.add_medium(rt::scene::MediumInstance {
            .shape_index = sphere,
            .material_index = subject_material,
            .density = 0.8,
        });
    } else {
        scene.add_instance(rt::scene::SurfaceInstance {
            .shape_index = sphere,
            .material_index = subject_material,
        });
    }
    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = backdrop,
        .material_index = backdrop_material,
    });
    return scene;
}

rt::CameraRig make_test_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {80.0, 80.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), kImageSize, kImageSize);
    return rig;
}

rt::RenderProfile make_test_profile() {
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 16;
    profile.max_bounces = 4;
    profile.rr_start_bounce = 5;
    profile.enable_denoise = false;
    return profile;
}

rt::RadianceFrame render_legacy(const rt::scene::SceneIR& scene) {
    rt::SceneDescription realtime = rt::scene::adapt_to_realtime(scene);
    realtime.background = Eigen::Vector3d::Zero();
    rt::OptixRenderer renderer;
    return renderer.render_radiance(
        realtime.pack(), make_test_rig().pack(), make_test_profile(), 0);
}

rt::RadianceFrame render_openpbr(const rt::scene::SceneIR& scene) {
    const rt::scene::SceneIRv2 scene_v2 = rt::scene::compile_legacy_scene_ir_v2(scene);
    rt::SceneDescription realtime = rt::scene::adapt_to_realtime_openpbr(scene, scene_v2);
    realtime.background = Eigen::Vector3d::Zero();
    rt::OptixRenderer renderer;
    return renderer.render_radiance(
        realtime.pack(), make_test_rig().pack(), make_test_profile(), 0);
}

ImageError compare_subject_images(
    const rt::RadianceFrame& legacy, const rt::RadianceFrame& openpbr) {
    expect_true(legacy.width == openpbr.width && legacy.height == openpbr.height,
        "compatibility image dimensions match");
    expect_true(legacy.depth.size() == openpbr.depth.size(),
        "compatibility depth buffers match");
    expect_true(legacy.beauty_rgba.size() == openpbr.beauty_rgba.size(),
        "compatibility beauty buffers match");

    ImageError error;
    double absolute_error = 0.0;
    std::vector<double> channel_errors;
    for (std::size_t pixel = 0; pixel < legacy.depth.size(); ++pixel) {
        const float legacy_depth = legacy.depth[pixel];
        const float openpbr_depth = openpbr.depth[pixel];
        const bool subject = (std::isfinite(legacy_depth) && legacy_depth > 0.0f
                                 && legacy_depth < kSubjectDepthLimit)
                             || (std::isfinite(openpbr_depth) && openpbr_depth > 0.0f
                                 && openpbr_depth < kSubjectDepthLimit);
        if (!subject) {
            continue;
        }
        ++error.subject_pixels;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::size_t index = pixel * 4U + channel;
            const double difference =
                std::abs(static_cast<double>(legacy.beauty_rgba[index])
                         - static_cast<double>(openpbr.beauty_rgba[index]));
            absolute_error += difference;
            channel_errors.push_back(difference);
            error.peak = std::max(error.peak, difference);
        }
    }
    expect_true(error.subject_pixels > 300, "compatibility image contains the subject");
    error.mae = absolute_error / static_cast<double>(error.subject_pixels * 3U);
    std::sort(channel_errors.begin(), channel_errors.end());
    const std::size_t p99_index = static_cast<std::size_t>(
        0.99 * static_cast<double>(channel_errors.size() - 1U));
    error.p99 = channel_errors[p99_index];
    return error;
}

void run_case(const CompatibilityCase& test_case) {
    const rt::scene::SceneIR scene = make_legacy_material_scene(test_case.kind);
    const rt::RadianceFrame legacy = render_legacy(scene);
    const rt::RadianceFrame openpbr = render_openpbr(scene);
    const ImageError error = compare_subject_images(legacy, openpbr);
    std::cout << test_case.name << " subject_pixels=" << error.subject_pixels
              << " mae=" << error.mae << " p99=" << error.p99 << " peak=" << error.peak << '\n';
    expect_true(legacy.average_luminance > 0.001 && openpbr.average_luminance > 0.001,
        std::string {test_case.name} + " compatibility images are non-black");
    expect_true(error.mae <= test_case.max_mae,
        std::string {test_case.name} + " compatibility image MAE");
    expect_true(error.p99 <= test_case.max_p99_error,
        std::string {test_case.name} + " compatibility image P99 error");
}

} // namespace

int main() {
    constexpr std::array<CompatibilityCase, 5> cases {{
        {LegacyMaterialKind::diffuse, "diffuse", 2e-4, 1e-3},
        {LegacyMaterialKind::metal, "metal", 0.012, 0.32},
        {LegacyMaterialKind::dielectric, "dielectric", 0.20, 1.20},
        {LegacyMaterialKind::emissive, "emissive", 1e-6, 1e-6},
        {LegacyMaterialKind::volume, "volume", 1e-6, 1e-6},
    }};
    for (const CompatibilityCase& test_case : cases) {
        run_case(test_case);
    }
    return 0;
}
