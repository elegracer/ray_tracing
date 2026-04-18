#include "realtime/camera_models.h"
#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <random>
#include <numbers>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct TemporaryImageFixture {
    std::filesystem::path path;
    std::filesystem::path dir;

    ~TemporaryImageFixture() {
        std::error_code ec;
        if (!path.empty()) {
            std::filesystem::remove(path, ec);
        }
        if (!dir.empty()) {
            std::filesystem::remove(dir, ec);
        }
    }
};

TemporaryImageFixture make_checker_fixture() {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    std::random_device entropy;
    std::mt19937_64 generator(entropy());
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto now_ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path fixture_dir = temp_dir
            / ("rt-realtime-texture-materials-" + std::to_string(now_ticks) + "-"
                + std::to_string(distribution(generator)));
        std::error_code ec;
        if (!std::filesystem::create_directory(fixture_dir, ec)) {
            if (ec && ec != std::make_error_code(std::errc::file_exists)) {
                throw std::runtime_error("unable to allocate checker fixture directory");
            }
            continue;
        }

        const std::filesystem::path fixture_path = fixture_dir / "checker.png";
        cv::Mat image(2, 2, CV_8UC3);
        image.at<cv::Vec3b>(0, 0) = cv::Vec3b {0, 0, 255};
        image.at<cv::Vec3b>(0, 1) = cv::Vec3b {0, 255, 0};
        image.at<cv::Vec3b>(1, 0) = cv::Vec3b {255, 0, 0};
        image.at<cv::Vec3b>(1, 1) = cv::Vec3b {255, 255, 255};
        expect_true(cv::imwrite(fixture_path.string(), image), "checker fixture should be written");
        return TemporaryImageFixture {.path = fixture_path, .dir = fixture_dir};
    }

    throw std::runtime_error("unable to allocate checker fixture path");
}

TemporaryImageFixture make_sphere_uv_fixture() {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    std::random_device entropy;
    std::mt19937_64 generator(entropy());
    std::uniform_int_distribution<std::uint64_t> distribution;

    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto now_ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path fixture_dir = temp_dir
            / ("rt-realtime-sphere-uv-" + std::to_string(now_ticks) + "-" + std::to_string(distribution(generator)));
        std::error_code ec;
        if (!std::filesystem::create_directory(fixture_dir, ec)) {
            if (ec && ec != std::make_error_code(std::errc::file_exists)) {
                throw std::runtime_error("unable to allocate sphere uv fixture directory");
            }
            continue;
        }

        const std::filesystem::path fixture_path = fixture_dir / "sphere_uv.png";
        cv::Mat image(1, 2, CV_8UC3);
        image.at<cv::Vec3b>(0, 0) = cv::Vec3b {0, 0, 255};
        image.at<cv::Vec3b>(0, 1) = cv::Vec3b {0, 255, 0};
        expect_true(cv::imwrite(fixture_path.string(), image), "sphere uv fixture should be written");
        return TemporaryImageFixture {.path = fixture_path, .dir = fixture_dir};
    }

    throw std::runtime_error("unable to allocate sphere uv fixture path");
}

void expect_vector_near(
    const std::vector<float>& actual, const std::vector<float>& expected, double tol, const std::string& label) {
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

Eigen::Vector3d pixel_rgb(const std::vector<float>& rgba, int width, int x, int y) {
    const std::size_t base = static_cast<std::size_t>((y * width + x) * 4);
    return Eigen::Vector3d {rgba[base + 0], rgba[base + 1], rgba[base + 2]};
}

bool quad_uv_for_pixel(
    const rt::PackedCamera& camera, const rt::QuadPrimitive& quad, int x, int y, double& u, double& v) {
    const Eigen::Matrix3d R_rc = camera.T_rc.block<3, 3>(0, 0);
    const Eigen::Vector3d origin = camera.T_rc.block<3, 1>(0, 3);
    const Eigen::Vector3d direction = (R_rc
        * rt::unproject_pinhole32(camera.pinhole, Eigen::Vector2d {static_cast<double>(x) + 0.5,
              static_cast<double>(y) + 0.5}))
                                           .normalized();

    const Eigen::Vector3d edge_u = quad.edge_u;
    const Eigen::Vector3d edge_v = quad.edge_v;
    const Eigen::Vector3d n = edge_u.cross(edge_v);
    const double denom = n.normalized().dot(direction);
    if (std::abs(denom) < 1e-8) {
        return false;
    }

    const double t = n.normalized().dot(quad.origin - origin) / denom;
    if (t <= 1e-6) {
        return false;
    }

    const Eigen::Vector3d p = origin + t * direction;
    const Eigen::Vector3d planar = p - quad.origin;
    const Eigen::Vector3d w = n / n.squaredNorm();
    u = w.dot(planar.cross(edge_v));
    v = w.dot(edge_u.cross(planar));
    return u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0;
}

Eigen::Vector3d checker_color(double u, double v) {
    const int ix = std::min(1, static_cast<int>(std::floor(std::clamp(u, 0.0, 1.0) * 2.0)));
    const int iy = std::min(1, static_cast<int>(std::floor(std::clamp(1.0 - v, 0.0, 1.0) * 2.0)));
    if (iy == 0 && ix == 0) {
        return Eigen::Vector3d {1.0, 0.0, 0.0};
    }
    if (iy == 0 && ix == 1) {
        return Eigen::Vector3d {0.0, 1.0, 0.0};
    }
    if (iy == 1 && ix == 0) {
        return Eigen::Vector3d {0.0, 0.0, 1.0};
    }
    return Eigen::Vector3d {1.0, 1.0, 1.0};
}

bool sphere_uv_for_pixel(
    const rt::PackedCamera& camera, const rt::SpherePrimitive& sphere, int x, int y, double& u, double& v) {
    const Eigen::Matrix3d R_rc = camera.T_rc.block<3, 3>(0, 0);
    const Eigen::Vector3d origin = camera.T_rc.block<3, 1>(0, 3);
    const Eigen::Vector3d direction = (R_rc
        * rt::unproject_pinhole32(camera.pinhole, Eigen::Vector2d {static_cast<double>(x) + 0.5,
              static_cast<double>(y) + 0.5}))
                                           .normalized();

    const Eigen::Vector3d oc = origin - sphere.center;
    const double half_b = oc.dot(direction);
    const double c = oc.squaredNorm() - sphere.radius * sphere.radius;
    const double discriminant = half_b * half_b - c;
    if (discriminant < 0.0) {
        return false;
    }

    const double sqrt_discriminant = std::sqrt(discriminant);
    double t = -half_b - sqrt_discriminant;
    if (t <= 1e-6) {
        t = -half_b + sqrt_discriminant;
    }
    if (t <= 1e-6) {
        return false;
    }

    const Eigen::Vector3d hit = origin + t * direction;
    const Eigen::Vector3d outward = (hit - sphere.center) / sphere.radius;
    const double theta = std::acos(std::clamp(-outward.y(), -1.0, 1.0));
    const double phi = std::atan2(-outward.z(), outward.x()) + std::numbers::pi;
    u = phi / (2.0 * std::numbers::pi);
    v = theta / std::numbers::pi;
    return true;
}

}  // namespace

int main() {
    const TemporaryImageFixture fixture = make_checker_fixture();
    const TemporaryImageFixture sphere_fixture = make_sphere_uv_fixture();

    rt::SceneDescription scene;
    const int checker = scene.add_texture(rt::ImageTextureDesc {.path = fixture.path.string()});
    const int textured = scene.add_material(rt::LambertianMaterial {.albedo_texture = checker});
    scene.add_quad(rt::QuadPrimitive {
        textured,
        rt::legacy_renderer_to_world(Eigen::Vector3d {-1.1, -0.9, -3.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {2.0, 0.0, 0.0}),
        rt::legacy_renderer_to_world(Eigen::Vector3d {1.0, 2.0, 0.0}),
        false,
    });

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {80.0, 80.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 64, 64);
    const rt::PackedCameraRig packed_rig = rig.pack();

    rt::RenderProfile profile = rt::RenderProfile::realtime();
    profile.samples_per_pixel = 1;
    profile.max_bounces = 1;
    profile.enable_denoise = false;
    profile.rr_start_bounce = 1;

    const rt::PackedScene packed = scene.pack();
    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(packed, packed_rig, profile, 0);

    expect_true(!frame.albedo_rgba.empty(), "albedo buffer present");
    expect_true(!frame.normal_rgba.empty(), "normal buffer present");
    expect_true(!frame.depth.empty(), "depth buffer present");

    const rt::PackedCamera& camera = packed_rig.cameras[0];
    int stable_pixels = 0;
    int mismatched_pixels = 0;
    int red_pixels = 0;
    int blue_pixels = 0;
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            double u = 0.0;
            double v = 0.0;
            if (!quad_uv_for_pixel(camera, packed.quads[0], x, y, u, v)) {
                continue;
            }
            if (u <= 0.08 || u >= 0.92 || v <= 0.08 || v >= 0.92) {
                continue;
            }
            if (std::abs(u - 0.5) <= 0.08 || std::abs(v - 0.5) <= 0.08) {
                continue;
            }

            ++stable_pixels;
            const Eigen::Vector3d expected = checker_color(u, v);
            const Eigen::Vector3d actual = pixel_rgb(frame.albedo_rgba, frame.width, x, y);
            if ((actual - expected).cwiseAbs().maxCoeff() > 1e-6) {
                ++mismatched_pixels;
            }
            if (expected.x() > 0.5 && expected.y() < 0.5 && expected.z() < 0.5) {
                ++red_pixels;
            }
            if (expected.z() > 0.5 && expected.x() < 0.5 && expected.y() < 0.5) {
                ++blue_pixels;
            }
        }
    }

    expect_true(stable_pixels > 150, "skew quad should cover many stable interior pixels");
    expect_true(red_pixels > 20, "analytic skew quad coverage should include red texel region");
    expect_true(blue_pixels > 20, "analytic skew quad coverage should include blue texel region");
    expect_true(mismatched_pixels == 0, "gpu albedo should match analytic skew-quad UV sampling");

    rt::SceneDescription sphere_scene;
    const int sphere_texture = sphere_scene.add_texture(rt::ImageTextureDesc {.path = sphere_fixture.path.string()});
    const int sphere_material = sphere_scene.add_material(rt::LambertianMaterial {.albedo_texture = sphere_texture});
    sphere_scene.add_sphere(rt::SpherePrimitive {
        sphere_material,
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}),
        0.9,
        false,
    });

    const rt::PackedScene sphere_packed = sphere_scene.pack();
    const rt::RadianceFrame sphere_frame = renderer.render_radiance(sphere_packed, packed_rig, profile, 0);
    expect_true(!sphere_frame.albedo_rgba.empty(), "sphere albedo buffer present");

    const Eigen::Matrix3d R_rc = camera.T_rc.block<3, 3>(0, 0);
    const Eigen::Vector3d t_rc = camera.T_rc.block<3, 1>(0, 3);
    const rt::SpherePrimitive& packed_sphere = sphere_packed.spheres[0];
    const Eigen::Vector3d sphere_center_camera = R_rc.transpose() * (packed_sphere.center - t_rc);
    const Eigen::Vector2d sphere_center_pixel = rt::project_pinhole32(camera.pinhole, sphere_center_camera);
    const int sphere_center_x = static_cast<int>(std::round(sphere_center_pixel.x()));
    const int sphere_center_y = static_cast<int>(std::round(sphere_center_pixel.y()));
    const int sphere_radius_pixels =
        std::max(4, static_cast<int>(std::round(camera.pinhole.fx * packed_sphere.radius / sphere_center_camera.z())));

    bool found_upper_sample = false;
    int upper_x = sphere_center_x;
    int upper_y = sphere_center_y;
    double upper_u = 0.0;
    double upper_v = 0.0;
    for (int dy = 1; dy <= (3 * sphere_radius_pixels) / 4; ++dy) {
        const int sample_y = sphere_center_y - dy;
        if (sample_y < 0 || sample_y >= sphere_frame.height) {
            continue;
        }
        if (!sphere_uv_for_pixel(camera, packed_sphere, sphere_center_x, sample_y, upper_u, upper_v)) {
            continue;
        }
        if (upper_u < 0.5 && upper_v > 0.1 && upper_v < 0.9) {
            upper_y = sample_y;
            found_upper_sample = true;
            break;
        }
    }

    expect_true(found_upper_sample, "sphere regression should find an upper interior sample with left-half UV");
    expect_true(upper_u < 0.5, "upper sphere sample should land on left half of 2x1 texture");
    expect_true(upper_v > 0.1 && upper_v < 0.9, "upper sphere sample should avoid pole clamping");
    const Eigen::Vector3d upper_rgb = pixel_rgb(sphere_frame.albedo_rgba, sphere_frame.width, upper_x, upper_y);
    expect_true(upper_rgb.x() > 0.95, "upper sphere sample should stay red-dominant");
    expect_true(upper_rgb.y() < 0.05, "upper sphere sample should suppress green on left texel");
    expect_true(upper_rgb.z() < 0.05, "upper sphere sample should suppress blue on left texel");

    rt::PackedScene tampered = packed;
    tampered.texture_count = 123;
    tampered.material_count = 99;
    tampered.sphere_count = 77;
    tampered.quad_count = 55;

    rt::OptixRenderer tampered_renderer;
    const rt::RadianceFrame tampered_frame = tampered_renderer.render_radiance(tampered, packed_rig, profile, 0);
    expect_frame_near(tampered_frame, frame, "tampered explicit counts should not affect render_radiance");

    rt::OptixRenderer prepared_renderer;
    prepared_renderer.prepare_scene(packed);
    const rt::ProfiledRadianceFrame prepared_baseline =
        prepared_renderer.render_prepared_radiance(packed_rig, profile, 0);

    rt::PackedScene prepared_tampered = packed;
    prepared_tampered.sphere_count = 0;
    prepared_tampered.quad_count = 0;

    rt::OptixRenderer prepared_tampered_renderer;
    prepared_tampered_renderer.prepare_scene(prepared_tampered);
    const rt::ProfiledRadianceFrame prepared_tampered_frame =
        prepared_tampered_renderer.render_prepared_radiance(packed_rig, profile, 0);
    expect_frame_near(prepared_tampered_frame.frame, prepared_baseline.frame,
        "prepared rendering should ignore stale zero primitive counts");

    rt::SceneDescription medium_scene;
    const int medium_albedo =
        medium_scene.add_texture(rt::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.9, 0.9, 0.9}});
    const int medium_material = medium_scene.add_material(rt::IsotropicVolumeMaterial {.albedo_texture = medium_albedo});
    const int medium_light = medium_scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {12.0, 12.0, 12.0}});
    const Eigen::Vector3d medium_center = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0});
    medium_scene.add_sphere(rt::SpherePrimitive {
        medium_light,
        medium_center,
        0.2,
        false,
    });
    medium_scene.add_medium(rt::HomogeneousMediumPrimitive {
        .material_index = medium_material,
        .density = 2.0,
        .boundary_type = 0,
        .local_center_or_min = medium_center,
        .radius = 0.9,
    });

    rt::RenderProfile medium_profile = profile;
    medium_profile.samples_per_pixel = 64;
    medium_profile.max_bounces = 2;
    medium_profile.rr_start_bounce = 2;

    rt::OptixRenderer medium_renderer;
    const rt::RadianceFrame medium_frame =
        medium_renderer.render_radiance(medium_scene.pack(), packed_rig, medium_profile, 0);
    expect_true(medium_frame.average_luminance > 0.001, "homogeneous medium should scatter visible light");
    expect_true(!medium_frame.albedo_rgba.empty(), "medium render should keep albedo buffer");
    return 0;
}
