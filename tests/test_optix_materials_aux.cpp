#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct MeanRgb {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

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

MeanRgb window_mean_rgb(const std::vector<float>& rgba, int width, int height,
    int center_x, int center_y, int half_window) {
    MeanRgb mean {};
    int samples = 0;
    for (int dy = -half_window; dy <= half_window; ++dy) {
        const int y = center_y + dy;
        if (y < 0 || y >= height) {
            continue;
        }
        for (int dx = -half_window; dx <= half_window; ++dx) {
            const int x = center_x + dx;
            if (x < 0 || x >= width) {
                continue;
            }
            const std::size_t base = static_cast<std::size_t>((y * width + x) * 4);
            mean.r += rgba[base + 0];
            mean.g += rgba[base + 1];
            mean.b += rgba[base + 2];
            ++samples;
        }
    }
    if (samples > 0) {
        const double inv = 1.0 / static_cast<double>(samples);
        mean.r *= inv;
        mean.g *= inv;
        mean.b *= inv;
    }
    return mean;
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {8.0, 8.0, 8.0}});
    const int diffuse = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.7, 0.2, 0.2}});
    const int metal = scene.add_material(rt::MetalMaterial {Eigen::Vector3d {0.9, 0.9, 0.9}, 0.02});
    const int glass = scene.add_material(rt::DielectricMaterial {1.5});

    scene.add_quad(rt::QuadPrimitive {light,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-1.0, 1.5, -3.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {2.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -2.0}), false});
    scene.add_sphere(rt::SpherePrimitive {diffuse, rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, -0.2, -3.5}), 0.5, false});
    scene.add_sphere(rt::SpherePrimitive {metal, rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, -0.2, -3.5}), 0.5, false});
    scene.add_sphere(rt::SpherePrimitive {glass, rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, -0.2, -3.5}), 0.5, false});

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {80.0, 80.0, 48.0, 48.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 96, 96);
    const rt::PackedCameraRig packed_rig = rig.pack();

    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(
        scene.pack(), packed_rig, rt::RenderProfile::quality(), 0);

    expect_true(frame.average_luminance > 0.02, "beauty is lit");
    expect_true(!frame.normal_rgba.empty(), "normal buffer present");
    expect_true(!frame.albedo_rgba.empty(), "albedo buffer present");
    expect_true(!frame.depth.empty(), "depth buffer present");
    expect_true(frame.normal_rgba != frame.beauty_rgba, "normal differs from beauty");
    const std::size_t expected_albedo_size = static_cast<std::size_t>(frame.width)
        * static_cast<std::size_t>(frame.height) * 4U;
    expect_true(frame.albedo_rgba.size() >= expected_albedo_size, "albedo has expected rgba size");

    const rt::PackedCamera& camera = packed_rig.cameras[0];
    const Eigen::Matrix3d R_rc = camera.T_rc.rotationMatrix();
    const Eigen::Vector3d t_rc = camera.T_rc.translation();
    const auto project_renderer_point = [&](const Eigen::Vector3d& point_renderer) {
        const Eigen::Vector3d point_camera = R_rc.transpose() * (point_renderer - t_rc);
        return rt::project_pinhole32(camera.pinhole, point_camera);
    };

    const int y = static_cast<int>(std::round(project_renderer_point(rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, -0.2, -3.5})).y()));
    const int left_x = static_cast<int>(std::round(project_renderer_point(rt::legacy_renderer_to_world(Eigen::Vector3d {-0.8, -0.2, -3.5})).x()));
    const int center_x = static_cast<int>(std::round(project_renderer_point(rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, -0.2, -3.5})).x()));
    const int right_x = static_cast<int>(std::round(project_renderer_point(rt::legacy_renderer_to_world(Eigen::Vector3d {0.8, -0.2, -3.5})).x()));
    const int half_window = std::max(2, frame.width / 24);

    const MeanRgb left = window_mean_rgb(frame.albedo_rgba, frame.width, frame.height, left_x, y, half_window);
    const MeanRgb center = window_mean_rgb(frame.albedo_rgba, frame.width, frame.height, center_x, y, half_window);
    const MeanRgb right = window_mean_rgb(frame.albedo_rgba, frame.width, frame.height, right_x, y, half_window);

    const double left_red_bias = left.r - std::max(left.g, left.b);
    const double right_red_bias = right.r - std::max(right.g, right.b);
    expect_true(left_red_bias > 0.08, "left sphere albedo is red-dominant");
    expect_true(std::abs(center.r - center.g) < 0.03 && std::abs(center.r - center.b) < 0.03,
        "center sphere albedo is near-neutral");
    expect_true(center.g > left.g + 0.08 && center.b > left.b + 0.08,
        "center sphere has stronger non-red channels than left sphere");
    expect_true(right_red_bias < left_red_bias - 0.03,
        "right sphere is less red-dominant than left sphere");

    expect_true(has_variation(frame.depth, 1e-6f), "depth varies across the frame");
    return 0;
}
