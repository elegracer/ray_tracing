#pragma once

#include <cmath>

namespace rt {

#if defined(__CUDACC__)
#define RT_LIGHT_HD     __host__ __device__
#define RT_LIGHT_INLINE __forceinline__
#else
#define RT_LIGHT_HD
#define RT_LIGHT_INLINE inline
#endif

enum class PackedLightType : int {
    sphere = 0,
    quad = 1,
    triangle = 2,
    environment = 3,
};

struct PackedLight {
    PackedLightType type = PackedLightType::sphere;
    int primitive_index = -1;
    float selection_pdf = 0.0f;
    float cdf = 0.0f;
};

enum class PackedAnalyticLightType : int {
    sphere = 0,
    disk = 1,
    rect = 2,
    cylinder = 3,
    distant = 4,
    dome = 5,
};

struct PackedLightVector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct PackedAnalyticLight {
    PackedAnalyticLightType type = PackedAnalyticLightType::sphere;
    PackedLightVector3 position;
    PackedLightVector3 basis_x;
    PackedLightVector3 basis_y;
    PackedLightVector3 basis_z;
    PackedLightVector3 radiance;
    float radius = 0.5f;
    float width = 1.0f;
    float height = 1.0f;
    float length = 1.0f;
    float world_area = 0.0f;
    float cos_theta_max = 1.0f;
    float selection_pdf = 0.0f;
    float cdf = 0.0f;
    int delta = 0;
    int treat_as_point = 0;
    int treat_as_line = 0;
};

RT_LIGHT_HD RT_LIGHT_INLINE float light_uniform_sphere_pdf() {
    return 0.07957747154594767f;
}

RT_LIGHT_HD RT_LIGHT_INLINE float light_uniform_cone_pdf(float cos_theta_max) {
    const float solid_angle = 6.28318530717958648f * (1.0f - cos_theta_max);
    return solid_angle > 1e-12f ? 1.0f / solid_angle : 0.0f;
}

RT_LIGHT_HD RT_LIGHT_INLINE float light_area_to_solid_angle_pdf(float area, float distance_squared,
    float abs_cosine_at_light) {
    if (area <= 1e-12f || distance_squared <= 0.0f || abs_cosine_at_light <= 1e-8f) {
        return 0.0f;
    }
    return distance_squared / (area * abs_cosine_at_light);
}

RT_LIGHT_HD RT_LIGHT_INLINE float light_power_heuristic(float pdf_a, float pdf_b) {
    if (pdf_a <= 0.0f) {
        return 0.0f;
    }
    const float a2 = pdf_a * pdf_a;
    const float b2 = pdf_b * pdf_b;
    return a2 / (a2 + b2);
}

RT_LIGHT_HD RT_LIGHT_INLINE int sample_packed_light(const PackedLight* lights, int light_count,
    float u) {
    if (lights == nullptr || light_count <= 0) {
        return -1;
    }
    const float sample = u < 0.0f ? 0.0f : (u < 1.0f ? u : 0.99999994f);
    for (int i = 0; i < light_count; ++i) {
        if (sample < lights[i].cdf) {
            return i;
        }
    }
    return light_count - 1;
}

RT_LIGHT_HD RT_LIGHT_INLINE int sample_packed_analytic_light(const PackedAnalyticLight* lights,
    int light_count, float u) {
    if (lights == nullptr || light_count <= 0) {
        return -1;
    }
    const float sample = u < 0.0f ? 0.0f : (u < 1.0f ? u : 0.99999994f);
    for (int i = 0; i < light_count; ++i) {
        if (sample < lights[i].cdf) {
            return i;
        }
    }
    return light_count - 1;
}

RT_LIGHT_HD RT_LIGHT_INLINE void sample_uniform_triangle(float u0, float u1, float& w0, float& w1,
    float& w2) {
    const float sqrt_u0 = sqrtf(u0 < 0.0f ? 0.0f : (u0 < 1.0f ? u0 : 1.0f));
    w0 = 1.0f - sqrt_u0;
    w1 = sqrt_u0 * (1.0f - u1);
    w2 = sqrt_u0 * u1;
}

#undef RT_LIGHT_HD
#undef RT_LIGHT_INLINE

} // namespace rt
