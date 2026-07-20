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
        return 10.0 + origin.x() + 2.0 * origin.z() + 3.0 * direction.x()
               + 5.0 * direction.z();
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

    expect_near(rt::light_uniform_sphere_pdf() * 4.0 * std::numbers::pi, 1.0, 1e-6,
        "uniform sphere PDF normalization");
    expect_near(rt::light_area_to_solid_angle_pdf(2.0f, 8.0f, 0.5f), 8.0, 1e-6,
        "area-to-solid-angle Jacobian");
    expect_near(rt::light_power_heuristic(0.25f, 0.25f), 0.5, 1e-6, "balanced power heuristic");
    expect_near(rt::light_power_heuristic(0.2f, 0.7f) + rt::light_power_heuristic(0.7f, 0.2f), 1.0,
        1e-6, "complementary power heuristic weights");
    expect_near(rt::light_power_heuristic(0.0f, 1.0f), 0.0, 1e-6, "zero-density technique weight");

    const SamplingProbe probe;
    const pro::proxy<Hittable> probe_proxy =
        pro::make_proxy_shared<Hittable, SamplingProbe>(probe);
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
    const pro::proxy<PDF> bsdf_only =
        make_light_mis_pdf(bsdf_pdf, no_geometry, origin, false);
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
