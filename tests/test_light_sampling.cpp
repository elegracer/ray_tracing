#include "common/light_sampling.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <numbers>

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
    expect_near(rt::light_power_heuristic(0.0f, 1.0f), 0.0, 1e-6, "zero-density technique weight");

    float w0 = 0.0f;
    float w1 = 0.0f;
    float w2 = 0.0f;
    rt::sample_uniform_triangle(0.36f, 0.25f, w0, w1, w2);
    expect_near(w0 + w1 + w2, 1.0, 1e-6, "triangle barycentrics normalize");
    expect_true(w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f, "triangle barycentrics are non-negative");
    return 0;
}
