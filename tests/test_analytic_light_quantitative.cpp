#include "common/camera.h"
#include "common/hittable_list.h"
#include "common/interval.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/ray.h"
#include "common/cpu_analytic_light.h"
#include "realtime/camera_rig.h"
#include "realtime/frame_convention.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "test_support.h"

#include <tbb/global_control.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>

namespace {

constexpr int kEstimatorSamples = 16384;
constexpr int kPdfIntegrationSamples = 262144;
constexpr int kShapeSamples = 65536;
constexpr double kReceiverAlbedo = 0.5;

std::string light_name(rt::AnalyticLightType type) {
    switch (type) {
        case rt::AnalyticLightType::sphere: return "sphere";
        case rt::AnalyticLightType::disk: return "disk";
        case rt::AnalyticLightType::rect: return "rect";
        case rt::AnalyticLightType::cylinder: return "cylinder";
        case rt::AnalyticLightType::distant: return "distant";
        case rt::AnalyticLightType::dome: return "dome";
    }
    return "unknown";
}

Eigen::Matrix3d legacy_to_world_matrix() {
    Eigen::Matrix3d transform;
    transform.col(0) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitX());
    transform.col(1) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitY());
    transform.col(2) = rt::legacy_renderer_to_world(Eigen::Vector3d::UnitZ());
    return transform;
}

rt::AnalyticLightDesc map_light_to_world(const rt::AnalyticLightDesc& legacy) {
    rt::AnalyticLightDesc world = legacy;
    const Eigen::Matrix3d transform = legacy_to_world_matrix();
    world.position = transform * legacy.position;
    world.local_to_world_linear = transform * legacy.local_to_world_linear;
    return world;
}

rt::AnalyticLightDesc make_estimator_light(rt::AnalyticLightType type) {
    const Eigen::Vector3d receiver {0.0, 0.0, -4.0};
    const Eigen::Vector3d center {0.0, 1.5, -2.0};
    const Eigen::Vector3d to_light = (center - receiver).normalized();

    rt::AnalyticLightDesc light;
    light.type = type;
    light.position = center;
    light.radiance = Eigen::Vector3d::Ones();
    light.selection_pdf = 1.0;
    light.cdf = 1.0;
    if (type == rt::AnalyticLightType::dome) {
        return light;
    }
    if (type == rt::AnalyticLightType::distant) {
        light.local_to_world_linear.col(2) = to_light;
        light.cos_theta_max = std::cos(15.0 * std::numbers::pi / 180.0);
        return light;
    }
    if (type == rt::AnalyticLightType::sphere) {
        light.radius = 0.75;
        light.world_area = 4.0 * std::numbers::pi * light.radius * light.radius;
        return light;
    }
    if (type == rt::AnalyticLightType::cylinder) {
        light.local_to_world_linear.col(0) = Eigen::Vector3d::UnitY();
        light.local_to_world_linear.col(1) = Eigen::Vector3d::UnitZ();
        light.local_to_world_linear.col(2) = Eigen::Vector3d::UnitX();
        light.radius = 0.75;
        light.length = 1.5;
        light.world_area = 2.0 * std::numbers::pi * light.radius * light.length;
        return light;
    }

    const Eigen::Vector3d normal = (receiver - center).normalized();
    const Eigen::Vector3d basis_x = Eigen::Vector3d::UnitX();
    light.local_to_world_linear.col(0) = basis_x;
    light.local_to_world_linear.col(1) = basis_x.cross(normal);
    light.local_to_world_linear.col(2) = -normal;
    if (type == rt::AnalyticLightType::disk) {
        light.radius = 1.0;
        light.world_area = std::numbers::pi * light.radius * light.radius;
    } else {
        light.width = 2.0;
        light.height = 2.0;
        light.world_area = light.width * light.height;
    }
    return light;
}

rt::AnalyticLightDesc make_normalization_light(rt::AnalyticLightType type) {
    rt::AnalyticLightDesc light;
    light.type = type;
    light.position = {0.0, 0.0, -2.0};
    light.radiance = Eigen::Vector3d::Ones();
    light.selection_pdf = 1.0;
    light.cdf = 1.0;
    if (type == rt::AnalyticLightType::sphere) {
        light.radius = 0.75;
        light.world_area = 4.0 * std::numbers::pi * light.radius * light.radius;
    } else if (type == rt::AnalyticLightType::disk) {
        light.local_to_world_linear.col(1) = -Eigen::Vector3d::UnitY();
        light.radius = 1.0;
        light.world_area = std::numbers::pi;
    } else if (type == rt::AnalyticLightType::rect) {
        light.local_to_world_linear.col(1) = -Eigen::Vector3d::UnitY();
        light.width = 2.0;
        light.height = 2.0;
        light.world_area = 4.0;
    } else if (type == rt::AnalyticLightType::cylinder) {
        light.local_to_world_linear.col(0) = Eigen::Vector3d::UnitX();
        light.local_to_world_linear.col(1) = Eigen::Vector3d::UnitZ();
        light.local_to_world_linear.col(2) = Eigen::Vector3d::UnitY();
        light.radius = 0.75;
        light.length = 1.5;
        light.world_area = 2.0 * std::numbers::pi * light.radius * light.length;
    }
    return light;
}

Eigen::Vector3d fibonacci_sphere_direction(int index, int count) {
    constexpr double golden_angle = 2.3999632297286533222;
    const double z = 1.0 - 2.0 * (static_cast<double>(index) + 0.5) / count;
    const double radius = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = golden_angle * index;
    return {radius * std::cos(phi), radius * std::sin(phi), z};
}

double radical_inverse(std::uint32_t bits) {
    bits = (bits << 16U) | (bits >> 16U);
    bits = ((bits & 0x55555555U) << 1U) | ((bits & 0xAAAAAAAAU) >> 1U);
    bits = ((bits & 0x33333333U) << 2U) | ((bits & 0xCCCCCCCCU) >> 2U);
    bits = ((bits & 0x0F0F0F0FU) << 4U) | ((bits & 0xF0F0F0F0U) >> 4U);
    bits = ((bits & 0x00FF00FFU) << 8U) | ((bits & 0xFF00FF00U) >> 8U);
    return static_cast<double>(bits) * 2.3283064365386963e-10;
}

double directional_pdf_mass(const rt::CpuAnalyticLightSampler& sampler) {
    double mass = 0.0;
    for (int index = 0; index < kPdfIntegrationSamples; ++index) {
        const Eigen::Vector3d direction = fibonacci_sphere_direction(index, kPdfIntegrationSamples);
        rt::CpuAnalyticLightHit hit;
        if (sampler.intersect(Ray {Eigen::Vector3d::Zero(), direction}, Interval {1e-6, infinity},
                hit)) {
            mass += sampler.pdf_for_hit(hit, Eigen::Vector3d::Zero(), direction);
        }
    }
    return mass * (4.0 * std::numbers::pi) / kPdfIntegrationSamples;
}

double null_sample_mass(const rt::CpuAnalyticLightSampler& sampler) {
    int invalid = 0;
    for (int index = 0; index < kShapeSamples; ++index) {
        const double u0 = (static_cast<double>(index) + 0.5) / kShapeSamples;
        const double u1 = radical_inverse(static_cast<std::uint32_t>(index));
        if (!sampler.sample(Eigen::Vector3d::Zero(), 0.5, u0, u1).valid) {
            ++invalid;
        }
    }
    return static_cast<double>(invalid) / kShapeSamples;
}

pro::proxy<Hittable> make_cpu_receiver() {
    const Eigen::Vector3d albedo = Eigen::Vector3d::Constant(kReceiverAlbedo);
    const pro::proxy<Material> material = pro::make_proxy_shared<Material, Lambertion>(albedo);
    auto world = std::make_shared<HittableList>();
    world->add(pro::make_proxy_shared<Hittable, Quad>(Eigen::Vector3d {-2.0, -2.0, -4.0},
        Eigen::Vector3d {4.0, 0.0, 0.0}, Eigen::Vector3d {0.0, 4.0, 0.0}, material));
    return pro::make_proxy_shared<Hittable, HittableList>(*world);
}

Camera::SharedCameraRayConfig make_cpu_camera_config() {
    Camera::SharedCameraRayConfig config;
    config.origin = Eigen::Vector3d::Zero();
    config.camera_to_world.col(0) = Eigen::Vector3d::UnitX();
    config.camera_to_world.col(1) = -Eigen::Vector3d::UnitY();
    config.camera_to_world.col(2) = -Eigen::Vector3d::UnitZ();
    config.pinhole = rt::Pinhole32Params {1e6, 1e6, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0};
    return config;
}

double decode_cpu_channel(std::uint8_t value) {
    const double display = (static_cast<double>(value) + 0.5) / 256.0;
    return display * display;
}

Eigen::Vector3d render_cpu_estimate(const rt::AnalyticLightDesc& light) {
    Camera camera;
    camera.aspect_ratio = 1.0;
    camera.image_width = 1;
    camera.samples_per_pixel = kEstimatorSamples;
    camera.max_depth = 1;
    camera.background = Eigen::Vector3d::Zero();
    camera.defocus_angle = 0.0;
    camera.set_shared_camera_ray_config(make_cpu_camera_config());
    camera.render(make_cpu_receiver(), {}, {light});
    const cv::Vec3b pixel = camera.img.at<cv::Vec3b>(0, 0);
    return {decode_cpu_channel(pixel[2]), decode_cpu_channel(pixel[1]),
        decode_cpu_channel(pixel[0])};
}

rt::PackedCameraRig make_gpu_camera_rig() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params {1e6, 1e6, 0.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0},
        Sophus::SE3d(), 1, 1);
    return rig.pack();
}

Eigen::Vector3d render_gpu_estimate(const rt::AnalyticLightDesc& legacy_light) {
    rt::SceneDescription scene;
    const int receiver = scene.add_material(
        rt::LambertianMaterial {.albedo = Eigen::Vector3d::Constant(kReceiverAlbedo)});
    scene.add_quad(rt::QuadPrimitive {
        .material_index = receiver,
        .origin = rt::legacy_renderer_to_world(Eigen::Vector3d {-2.0, -2.0, -4.0}),
        .edge_u = rt::legacy_renderer_to_world(Eigen::Vector3d {4.0, 0.0, 0.0}),
        .edge_v = rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.0, 0.0}),
        .dynamic = false,
    });
    scene.add_analytic_light(map_light_to_world(legacy_light));

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = kEstimatorSamples;
    profile.max_bounces = 1;
    profile.rr_start_bounce = 2;
    profile.enable_denoise = false;
    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame =
        renderer.render_radiance(scene.pack(), make_gpu_camera_rig(), profile, 0);
    expect_true(frame.width == 1 && frame.height == 1 && frame.beauty_rgba.size() == 4,
        "GPU quantitative frame shape");
    const Eigen::Vector3d estimate {frame.beauty_rgba[0], frame.beauty_rgba[1],
        frame.beauty_rgba[2]};
    expect_true(estimate.allFinite() && (estimate.array() >= 0.0).all(),
        "GPU quantitative frame is finite and non-negative");
    return estimate;
}

void test_pdf_normalization() {
    constexpr std::array finite_types {
        rt::AnalyticLightType::sphere,
        rt::AnalyticLightType::disk,
        rt::AnalyticLightType::rect,
        rt::AnalyticLightType::cylinder,
    };
    for (const rt::AnalyticLightType type : finite_types) {
        const rt::CpuAnalyticLightSampler sampler {{make_normalization_light(type)}};
        const double directional_mass = directional_pdf_mass(sampler);
        const double null_mass = null_sample_mass(sampler);
        std::cout << "pdf_mass " << light_name(type) << " directional=" << directional_mass
                  << " null=" << null_mass << " total=" << directional_mass + null_mass << '\n';
        expect_near(directional_mass + null_mass, 1.0, 0.025,
            light_name(type) + " continuous PDF plus explicit null event normalizes");
    }

    rt::AnalyticLightDesc dome = make_normalization_light(rt::AnalyticLightType::dome);
    const rt::CpuAnalyticLightSampler dome_sampler {{dome}};
    for (int index = 0; index < 256; ++index) {
        const rt::CpuAnalyticLightSample sample = dome_sampler.sample(Eigen::Vector3d::Zero(), 0.5,
            (index + 0.5) / 256.0, radical_inverse(static_cast<std::uint32_t>(index)));
        expect_true(sample.valid && sample.infinite && !sample.delta,
            "dome continuous sample validity");
        expect_near(sample.pdf * 4.0 * std::numbers::pi, 1.0, 1e-12,
            "dome spherical PDF normalization");
    }

    rt::AnalyticLightDesc distant = make_normalization_light(rt::AnalyticLightType::distant);
    distant.local_to_world_linear.col(2) = -Eigen::Vector3d::UnitZ();
    distant.cos_theta_max = 0.9;
    const rt::CpuAnalyticLightSampler distant_sampler {{distant}};
    const double cone_measure = 2.0 * std::numbers::pi * (1.0 - distant.cos_theta_max);
    for (int index = 0; index < 256; ++index) {
        const rt::CpuAnalyticLightSample sample = distant_sampler.sample(Eigen::Vector3d::Zero(),
            0.5, (index + 0.5) / 256.0, radical_inverse(static_cast<std::uint32_t>(index)));
        expect_true(sample.valid && sample.infinite && !sample.delta,
            "distant cone sample validity");
        expect_near(sample.pdf * cone_measure, 1.0, 1e-12, "distant cone PDF normalization");
    }

    distant.delta = true;
    const rt::CpuAnalyticLightSample delta =
        rt::CpuAnalyticLightSampler {{distant}}.sample(Eigen::Vector3d::Zero(), 0.5, 0.25, 0.75);
    expect_true(delta.valid && delta.delta && delta.infinite,
        "delta distant light uses an explicit discrete measure");
    expect_near(delta.pdf, 1.0, 1e-12, "delta distant discrete probability mass");
}

void test_cpu_gpu_estimator_agreement() {
    constexpr std::array types {
        rt::AnalyticLightType::sphere,
        rt::AnalyticLightType::disk,
        rt::AnalyticLightType::rect,
        rt::AnalyticLightType::cylinder,
        rt::AnalyticLightType::distant,
        rt::AnalyticLightType::dome,
    };
    for (const rt::AnalyticLightType type : types) {
        const rt::AnalyticLightDesc light = make_estimator_light(type);
        const Eigen::Vector3d cpu = render_cpu_estimate(light);
        const Eigen::Vector3d gpu = render_gpu_estimate(light);
        const double scale = std::max({0.02, cpu.maxCoeff(), gpu.maxCoeff()});
        const double tolerance = std::max(0.004, 0.05 * scale);
        std::cout << "estimator " << light_name(type) << " cpu=" << cpu.transpose()
                  << " gpu=" << gpu.transpose() << " tolerance=" << tolerance << '\n';
        expect_true(cpu.allFinite() && (cpu.array() >= 0.0).all(),
            light_name(type) + " CPU estimate is finite and non-negative");
        expect_true(cpu.maxCoeff() <= kReceiverAlbedo + 0.04
                        && gpu.maxCoeff() <= kReceiverAlbedo + 0.04,
            light_name(type) + " unit-radiance estimate respects the finite-energy bound");
        expect_vec3_near(cpu, gpu, tolerance,
            light_name(type) + " CPU/GPU production estimator agreement");
        if (type == rt::AnalyticLightType::dome) {
            expect_vec3_near(cpu, Eigen::Vector3d::Constant(kReceiverAlbedo), 0.025,
                "CPU unit-environment furnace response");
            expect_vec3_near(gpu, Eigen::Vector3d::Constant(kReceiverAlbedo), 0.025,
                "GPU unit-environment furnace response");
        }
    }
}

} // namespace

int main() {
    tbb::global_control render_threads(tbb::global_control::max_allowed_parallelism, 1);
    test_pdf_normalization();
    test_cpu_gpu_estimator_agreement();
    return 0;
}
