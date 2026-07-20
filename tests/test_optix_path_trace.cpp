#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

namespace {

constexpr int kImageSize = 64;
constexpr int kCenterPixel = kImageSize / 2;

rt::CameraRig make_test_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {200.0, 200.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), kImageSize, kImageSize);
    return rig;
}

rt::RenderProfile make_test_profile(int samples_per_pixel = 4, int max_bounces = 1) {
    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = samples_per_pixel;
    profile.max_bounces = max_bounces;
    profile.rr_start_bounce = max_bounces + 1;
    return profile;
}

double pixel_luminance(const rt::RadianceFrame& frame, int x, int y) {
    const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width)
                                  + static_cast<std::size_t>(x))
                              * 4U;
    return (static_cast<double>(frame.beauty_rgba[index + 0])
               + static_cast<double>(frame.beauty_rgba[index + 1])
               + static_cast<double>(frame.beauty_rgba[index + 2]))
           / 3.0;
}

rt::RadianceFrame render_triangle_direct_light(bool front_facing_light, int samples_per_pixel = 4,
    int max_bounces = 1, bool add_occluder = false) {
    rt::SceneDescription scene;
    const int light =
        scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {20.0, 20.0, 20.0}});
    const int diffuse =
        scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.8, 0.8}});
    const int dark = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d::Zero()});
    if (front_facing_light) {
        scene.add_triangle(rt::TrianglePrimitive {
            .material_index = light,
            .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, 1.2, 1.0}),
            .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 2.4, 1.0}),
            .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, 1.2, 1.0}),
            .dynamic = false,
        });
    } else {
        scene.add_triangle(rt::TrianglePrimitive {
            .material_index = light,
            .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, 1.2, 1.0}),
            .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, 1.2, 1.0}),
            .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 2.4, 1.0}),
            .dynamic = false,
        });
    }
    scene.add_quad(rt::QuadPrimitive {
        diffuse,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -4.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
        false,
    });
    if (add_occluder) {
        scene.add_sphere(rt::SpherePrimitive {
            dark,
            Eigen::Vector3d {0.0, 1.5, 0.9},
            0.65,
            false,
        });
    }

    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(),
        make_test_profile(samples_per_pixel, max_bounces), 0);
}

rt::RadianceFrame render_sphere_direct_light() {
    rt::SceneDescription scene;
    const int light =
        scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {20.0, 20.0, 20.0}});
    const int diffuse =
        scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.8, 0.8, 0.8}});
    scene.add_sphere(rt::SpherePrimitive {
        light,
        Eigen::Vector3d {0.0, -1.0, 1.8},
        0.6,
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        diffuse,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-3.0, -3.0, -4.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {6.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 6.0, 0.0}),
        false,
    });
    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), make_test_profile(64, 1),
        0);
}

rt::RadianceFrame render_triangle_emissive_hit(bool front_facing_light) {
    rt::SceneDescription scene;
    const int light =
        scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {20.0, 20.0, 20.0}});
    if (front_facing_light) {
        scene.add_triangle(rt::TrianglePrimitive {
            .material_index = light,
            .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-1.2, -1.0, -3.0}),
            .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {1.2, -1.0, -3.0}),
            .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 1.2, -3.0}),
            .dynamic = false,
        });
    } else {
        scene.add_triangle(rt::TrianglePrimitive {
            .material_index = light,
            .p0 = rt::legacy_renderer_to_world(Eigen::Vector3d {-1.2, -1.0, -3.0}),
            .p1 = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 1.2, -3.0}),
            .p2 = rt::legacy_renderer_to_world(Eigen::Vector3d {1.2, -1.0, -3.0}),
            .dynamic = false,
        });
    }

    rt::OptixRenderer renderer;
    return renderer.render_radiance(scene.pack(), make_test_rig().pack(), make_test_profile(), 0);
}

} // namespace

int main() {
    const rt::RadianceFrame front_direct = render_triangle_direct_light(true);
    const rt::RadianceFrame back_direct = render_triangle_direct_light(false);
    const rt::RadianceFrame front_emissive = render_triangle_emissive_hit(true);
    const rt::RadianceFrame back_emissive = render_triangle_emissive_hit(false);
    const rt::RadianceFrame sphere_direct = render_sphere_direct_light();
    const rt::RadianceFrame unoccluded = render_triangle_direct_light(true, 256, 1, false);
    const rt::RadianceFrame occluded = render_triangle_direct_light(true, 256, 1, true);
    const rt::RadianceFrame light_only = render_triangle_direct_light(true, 512, 1, false);
    const rt::RadianceFrame light_and_bsdf = render_triangle_direct_light(true, 512, 2, false);

    expect_near(static_cast<double>(front_direct.width), 64.0, 1e-12, "radiance width");
    expect_near(static_cast<double>(front_direct.height), 64.0, 1e-12, "radiance height");
    expect_true(front_direct.average_luminance > 0.05,
        "front-facing triangle direct lighting should be visible");
    expect_true(back_direct.average_luminance < front_direct.average_luminance * 0.25,
        "back-facing triangle direct lighting should be strongly suppressed");
    const double sphere_center = pixel_luminance(sphere_direct, kCenterPixel, kCenterPixel);
    const double unoccluded_center = pixel_luminance(unoccluded, kCenterPixel, kCenterPixel);
    const double occluded_center = pixel_luminance(occluded, kCenterPixel, kCenterPixel);
    const double light_only_center = pixel_luminance(light_only, kCenterPixel, kCenterPixel);
    const double light_and_bsdf_center =
        pixel_luminance(light_and_bsdf, kCenterPixel, kCenterPixel);
    expect_true(sphere_center > 0.01, "sphere solid-angle sampling contributes direct light");
    expect_true(occluded_center < unoccluded_center * 0.5,
        "visibility ray suppresses occluded direct light");
    expect_true(std::abs(light_and_bsdf_center - light_only_center) < light_only_center * 0.05,
        "matched light and BSDF MIS preserves direct-light energy");

    const double front_center = pixel_luminance(front_emissive, kCenterPixel, kCenterPixel);
    const double back_center = pixel_luminance(back_emissive, kCenterPixel, kCenterPixel);
    expect_true(front_center > 1.0, "front-facing emissive triangle hit should be visible");
    expect_true(back_center < front_center * 0.1,
        "back-facing emissive triangle hit should be suppressed");
    return 0;
}
