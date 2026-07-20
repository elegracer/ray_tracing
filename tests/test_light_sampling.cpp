#include "common/cpu_analytic_light.h"
#include "common/light_sampling.h"
#include "common/hittable.h"
#include "common/pdf.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <numbers>

namespace {

struct ConstantPdf {
    double density = 0.0;
    Vec3d direction = Vec3d::UnitZ();

    double value(const Vec3d&) const { return density; }
    Vec3d generate() const { return direction; }
};

struct SamplingProbe {
    bool hit(const Ray&, const Interval&, HitRecord&) const { return false; }
    AABB bounding_box() const { return AABB {Vec3d {-1.0, -1.0, -1.0}, Vec3d::Ones()}; }
    double pdf_value(const Vec3d& origin, const Vec3d& direction) const {
        return 10.0 + origin.x() + 2.0 * origin.z() + 3.0 * direction.x() + 5.0 * direction.z();
    }
    Vec3d random(const Vec3d&) const { return {1.0, 2.0, 3.0}; }
};

} // namespace

int main() {
    const std::array<rt::PackedLight, 3> lights {{
        {.type = rt::PackedLightType::sphere,
            .primitive_index = 0,
            .selection_pdf = 0.2f,
            .cdf = 0.2f},
        {.type = rt::PackedLightType::quad,
            .primitive_index = 1,
            .selection_pdf = 0.3f,
            .cdf = 0.5f},
        {.type = rt::PackedLightType::environment,
            .primitive_index = -1,
            .selection_pdf = 0.5f,
            .cdf = 1.0f},
    }};

    expect_true(rt::sample_packed_light(lights.data(), 3, 0.0f) == 0, "CDF lower boundary");
    expect_true(rt::sample_packed_light(lights.data(), 3, 0.2f) == 1, "CDF first boundary");
    expect_true(rt::sample_packed_light(lights.data(), 3, 0.5f) == 2, "CDF second boundary");
    expect_true(rt::sample_packed_light(lights.data(), 3, 1.0f) == 2, "CDF upper clamp");
    expect_true(rt::sample_packed_light(nullptr, 0, 0.5f) == -1, "empty distribution");

    const std::array<rt::PackedAnalyticLight, 3> analytic_lights {{
        {.type = rt::PackedAnalyticLightType::sphere, .cdf = 0.25f},
        {.type = rt::PackedAnalyticLightType::rect, .cdf = 0.75f},
        {.type = rt::PackedAnalyticLightType::dome, .cdf = 1.0f},
    }};
    expect_true(rt::sample_packed_analytic_light(analytic_lights.data(), 3, 0.0f) == 0,
        "analytic CDF lower boundary");
    expect_true(rt::sample_packed_analytic_light(analytic_lights.data(), 3, 0.25f) == 1,
        "analytic CDF first boundary");
    expect_true(rt::sample_packed_analytic_light(analytic_lights.data(), 3, 0.75f) == 2,
        "analytic CDF second boundary");
    expect_true(rt::sample_packed_analytic_light(analytic_lights.data(), 3, 1.0f) == 2,
        "analytic CDF upper clamp");
    expect_true(rt::sample_packed_analytic_light(nullptr, 0, 0.5f) == -1,
        "empty analytic distribution");

    rt::AnalyticLightDesc cpu_sphere;
    cpu_sphere.type = rt::AnalyticLightType::sphere;
    cpu_sphere.position = {0.0, 0.0, -4.0};
    cpu_sphere.radiance = Eigen::Vector3d::Ones();
    cpu_sphere.radius = 0.5;
    cpu_sphere.world_area = 4.0 * std::numbers::pi * cpu_sphere.radius * cpu_sphere.radius;
    cpu_sphere.selection_pdf = 1.0;
    cpu_sphere.cdf = 1.0;
    const rt::CpuAnalyticLightSampler cpu_sphere_sampler {{cpu_sphere}};
    const rt::CpuAnalyticLightSample cpu_sphere_sample =
        cpu_sphere_sampler.sample(Eigen::Vector3d::Zero(), 0.5, 0.25, 0.75);
    expect_true(cpu_sphere_sample.valid && cpu_sphere_sample.pdf > 0.0,
        "CPU analytic sphere produces a finite solid-angle sample");
    rt::CpuAnalyticLightHit cpu_sphere_hit;
    expect_true(
        cpu_sphere_sampler.intersect(Ray {Eigen::Vector3d::Zero(), cpu_sphere_sample.direction},
            Interval {0.001, infinity}, cpu_sphere_hit),
        "CPU analytic sphere sample intersects its emissive surface");
    const double cpu_sphere_pdf = cpu_sphere_sampler.pdf_for_hit(cpu_sphere_hit,
        Eigen::Vector3d::Zero(), cpu_sphere_sample.direction);
    expect_near(cpu_sphere_pdf, cpu_sphere_sample.pdf, 1e-10,
        "CPU analytic sphere sampling and hit PDFs match");
    expect_near(cpu_sphere_sampler.emission_mis_weight(cpu_sphere_hit, Eigen::Vector3d::Zero(),
                    cpu_sphere_sample.direction, cpu_sphere_pdf, true, false),
        0.5, 1e-12, "CPU analytic sphere uses the matched power heuristic");

    rt::AnalyticLightDesc cpu_rect;
    cpu_rect.type = rt::AnalyticLightType::rect;
    cpu_rect.position = {0.0, 0.0, -4.0};
    cpu_rect.local_to_world_linear.col(0) = Eigen::Vector3d::UnitX();
    cpu_rect.local_to_world_linear.col(1) = -Eigen::Vector3d::UnitY();
    cpu_rect.radiance = Eigen::Vector3d::Ones();
    cpu_rect.width = 2.0;
    cpu_rect.height = 2.0;
    cpu_rect.world_area = 4.0;
    cpu_rect.selection_pdf = 1.0;
    cpu_rect.cdf = 1.0;
    const rt::CpuAnalyticLightSampler cpu_rect_sampler {{cpu_rect}};
    const rt::CpuAnalyticLightSample cpu_rect_sample =
        cpu_rect_sampler.sample(Eigen::Vector3d::Zero(), 0.5, 0.5, 0.5);
    expect_true(cpu_rect_sample.valid, "CPU analytic rect produces a visible area sample");
    expect_near(cpu_rect_sample.pdf, 4.0, 1e-12,
        "CPU analytic rect applies the area-to-solid-angle Jacobian");
    rt::CpuAnalyticLightHit cpu_rect_hit;
    expect_true(cpu_rect_sampler.intersect(Ray {Eigen::Vector3d::Zero(), cpu_rect_sample.direction},
                    Interval {0.001, infinity}, cpu_rect_hit),
        "CPU analytic rect sample intersects its one-sided surface");

    rt::AnalyticLightDesc cpu_distant;
    cpu_distant.type = rt::AnalyticLightType::distant;
    cpu_distant.local_to_world_linear.col(2) = -Eigen::Vector3d::UnitZ();
    cpu_distant.radiance = Eigen::Vector3d {0.25, 0.5, 1.0};
    cpu_distant.selection_pdf = 0.25;
    cpu_distant.cdf = 0.25;
    cpu_distant.delta = true;
    rt::AnalyticLightDesc cpu_dome;
    cpu_dome.type = rt::AnalyticLightType::dome;
    cpu_dome.radiance = Eigen::Vector3d {0.5, 0.25, 0.125};
    cpu_dome.selection_pdf = 0.75;
    cpu_dome.cdf = 1.0;
    const rt::CpuAnalyticLightSampler cpu_infinite_sampler {{cpu_distant, cpu_dome}};
    const rt::CpuAnalyticLightSample cpu_distant_sample =
        cpu_infinite_sampler.sample(Eigen::Vector3d::Zero(), 0.1, 0.2, 0.3);
    expect_true(cpu_distant_sample.valid && cpu_distant_sample.delta && cpu_distant_sample.infinite,
        "CPU delta distant light reports a discrete infinite sample");
    expect_vec3_near(
        cpu_infinite_sampler.infinite_radiance(-Eigen::Vector3d::UnitZ(), 0.0, false, false),
        cpu_distant.radiance + cpu_dome.radiance, 1e-12,
        "CPU distant and dome lights contribute matching miss radiance");
    const rt::CpuAnalyticLightSample cpu_dome_sample =
        cpu_infinite_sampler.sample(Eigen::Vector3d::Zero(), 0.5, 0.2, 0.3);
    expect_true(cpu_dome_sample.valid && cpu_dome_sample.infinite && !cpu_dome_sample.delta,
        "CPU dome produces a continuous infinite sample");
    expect_near(cpu_dome_sample.pdf, cpu_dome.selection_pdf / (4.0 * std::numbers::pi), 1e-12,
        "CPU dome sample uses its selection-weighted spherical PDF");

    expect_near(rt::light_uniform_sphere_pdf() * 4.0 * std::numbers::pi, 1.0, 1e-6,
        "uniform sphere PDF normalization");
    expect_near(rt::light_area_to_solid_angle_pdf(2.0f, 8.0f, 0.5f), 8.0, 1e-6,
        "area-to-solid-angle Jacobian");
    expect_near(rt::light_power_heuristic(0.25f, 0.25f), 0.5, 1e-6, "balanced power heuristic");
    expect_near(rt::light_power_heuristic(0.2f, 0.7f) + rt::light_power_heuristic(0.7f, 0.2f), 1.0,
        1e-6, "complementary power heuristic weights");
    expect_near(rt::light_power_heuristic(0.0f, 1.0f), 0.0, 1e-6, "zero-density technique weight");

    const SamplingProbe probe;
    const pro::proxy<Hittable> probe_proxy = pro::make_proxy_shared<Hittable, SamplingProbe>(probe);
    const Vec3d origin {4.0, 2.0, 1.0};
    const Vec3d direction {-2.0, 1.0, -3.0};

    const Vec3d offset {1.5, -0.5, 2.0};
    const Translate translated {probe_proxy, offset};
    expect_near(translated.pdf_value(origin, direction),
        probe.pdf_value(origin - offset, direction), 1e-12,
        "translated light evaluates its PDF in object space");
    expect_vec3_near(translated.random(origin), probe.random(origin - offset), 1e-12,
        "translated light samples an object-space direction");

    const RotateY rotated {probe_proxy, 90.0};
    const Vec3d local_origin {-origin.z(), origin.y(), origin.x()};
    const Vec3d local_direction {-direction.z(), direction.y(), direction.x()};
    expect_near(rotated.pdf_value(origin, direction),
        probe.pdf_value(local_origin, local_direction), 1e-12,
        "rotated light evaluates its PDF in object space");
    expect_vec3_near(rotated.random(origin), Vec3d {3.0, 2.0, -1.0}, 1e-12,
        "rotated light maps sampled directions back to world space");

    const pro::proxy<PDF> bsdf_pdf =
        pro::make_proxy_shared<PDF, ConstantPdf>(ConstantPdf {.density = 0.2});
    const pro::proxy<PDF> geometry_and_environment =
        make_light_mis_pdf(bsdf_pdf, probe_proxy, origin, true);
    const double geometry_pdf = probe.pdf_value(origin, direction);
    const double environment_pdf = 1.0 / (4.0 * std::numbers::pi);
    expect_near(geometry_and_environment->value(direction),
        0.5 * 0.2 + 0.5 * (0.5 * geometry_pdf + 0.5 * environment_pdf), 1e-12,
        "CPU BSDF, geometry, and environment mixture reports its sampling PDF");

    const pro::proxy<Hittable> no_geometry;
    const pro::proxy<PDF> environment_only =
        make_light_mis_pdf(bsdf_pdf, no_geometry, origin, true);
    expect_near(environment_only->value(direction), 0.5 * 0.2 + 0.5 * environment_pdf, 1e-12,
        "CPU environment mixture stays normalized without geometry lights");
    const pro::proxy<PDF> bsdf_only = make_light_mis_pdf(bsdf_pdf, no_geometry, origin, false);
    expect_near(bsdf_only->value(direction), 0.2, 1e-12,
        "black-environment no-light path preserves the BSDF PDF exactly");

    float w0 = 0.0f;
    float w1 = 0.0f;
    float w2 = 0.0f;
    rt::sample_uniform_triangle(0.36f, 0.25f, w0, w1, w2);
    expect_near(w0 + w1 + w2, 1.0, 1e-6, "triangle barycentrics normalize");
    expect_true(w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f, "triangle barycentrics are non-negative");
    return 0;
}
