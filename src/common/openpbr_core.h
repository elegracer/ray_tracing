#pragma once

#include <cmath>

#if defined(__CUDACC__)
#define RT_OPENPBR_HD     __host__ __device__
#define RT_OPENPBR_INLINE __forceinline__
#else
#define RT_OPENPBR_HD
#define RT_OPENPBR_INLINE inline
#endif

namespace rt {

inline constexpr float kOpenPbrPi = 3.14159265358979323846f;
inline constexpr float kOpenPbrInvPi = 1.0f / kOpenPbrPi;

struct OpenPbrVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct OpenPbrCoreMaterial {
    float base_weight = 1.0f;
    OpenPbrVec3 base_color {0.8f, 0.8f, 0.8f};
    float base_diffuse_roughness = 0.0f;
    float base_metalness = 0.0f;

    float specular_weight = 1.0f;
    OpenPbrVec3 specular_color {1.0f, 1.0f, 1.0f};
    float specular_roughness = 0.3f;
    float specular_ior = 1.5f;
    float specular_roughness_anisotropy = 0.0f;

    float transmission_weight = 0.0f;
    OpenPbrVec3 transmission_color {1.0f, 1.0f, 1.0f};
    float transmission_depth = 0.0f;

    float emission_luminance = 0.0f;
    OpenPbrVec3 emission_color {1.0f, 1.0f, 1.0f};

    float geometry_opacity = 1.0f;
    int geometry_thin_walled = 0;
};

enum class OpenPbrSourceColorSpace : int {
    raw = 0,
    linear_srgb = 1,
    srgb_texture = 2,
};

enum class OpenPbrColorInput : int {
    base_color = 0,
    specular_color = 1,
    transmission_color = 2,
    emission_color = 3,
};

struct OpenPbrColorTextureBinding {
    int texture_index = -1;
    OpenPbrSourceColorSpace source_color_space = OpenPbrSourceColorSpace::linear_srgb;
};

struct OpenPbrColorTextureBindings {
    OpenPbrColorTextureBinding base_color {};
    OpenPbrColorTextureBinding specular_color {};
    OpenPbrColorTextureBinding transmission_color {};
    OpenPbrColorTextureBinding emission_color {};
};

struct OpenPbrCompiledMaterial {
    OpenPbrCoreMaterial parameters {};
    OpenPbrColorTextureBindings color_textures {};
};

struct OpenPbrFrame {
    OpenPbrVec3 tangent {1.0f, 0.0f, 0.0f};
    OpenPbrVec3 bitangent {0.0f, 1.0f, 0.0f};
    OpenPbrVec3 normal {0.0f, 0.0f, 1.0f};
};

enum class OpenPbrScatterEvent : int {
    none = 0,
    diffuse_reflection = 1,
    glossy_reflection = 2,
    glossy_transmission = 3,
    thin_walled_transmission = 4,
    opacity_passthrough = 5,
};

struct OpenPbrEvaluation {
    OpenPbrVec3 value {};
    float pdf = 0.0f;
};

struct OpenPbrSample {
    OpenPbrVec3 wi {};
    OpenPbrVec3 value {};
    OpenPbrVec3 weight {};
    float pdf = 0.0f;
    float discrete_pdf = 0.0f;
    OpenPbrScatterEvent event = OpenPbrScatterEvent::none;
    int delta = 0;
    int valid = 0;
};

struct OpenPbrLobeProbabilities {
    float diffuse = 0.0f;
    float reflection = 0.0f;
    float transmission = 0.0f;
};

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_min(float a, float b) {
    return a < b ? a : b;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_max(float a, float b) {
    return a > b ? a : b;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_clamp(float value, float low, float high) {
    return openpbr_min(openpbr_max(value, low), high);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_saturate(float value) {
    return openpbr_clamp(value, 0.0f, 1.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_square(float value) {
    return value * value;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_safe_sqrt(float value) {
    return sqrtf(openpbr_max(0.0f, value));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_make_vec3(float value) {
    return {value, value, value};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_add(const OpenPbrVec3& a,
    const OpenPbrVec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sub(const OpenPbrVec3& a,
    const OpenPbrVec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_mul(const OpenPbrVec3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_mul(const OpenPbrVec3& a,
    const OpenPbrVec3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_div(const OpenPbrVec3& value, float scale) {
    const float inverse = 1.0f / scale;
    return openpbr_mul(value, inverse);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_negate(const OpenPbrVec3& value) {
    return {-value.x, -value.y, -value.z};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_dot(const OpenPbrVec3& a, const OpenPbrVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_cross(const OpenPbrVec3& a,
    const OpenPbrVec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_length_squared(const OpenPbrVec3& value) {
    return openpbr_dot(value, value);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_normalize(const OpenPbrVec3& value) {
    const float length_squared = openpbr_length_squared(value);
    if (length_squared <= 1e-20f) {
        return {};
    }
    return openpbr_mul(value, 1.0f / sqrtf(length_squared));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_lerp(const OpenPbrVec3& a, const OpenPbrVec3& b,
    float t) {
    return openpbr_add(openpbr_mul(a, 1.0f - t), openpbr_mul(b, t));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_clamp_nonnegative(const OpenPbrVec3& value) {
    return {openpbr_max(0.0f, value.x), openpbr_max(0.0f, value.y), openpbr_max(0.0f, value.z)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_clamp_unit(const OpenPbrVec3& value) {
    return {openpbr_saturate(value.x), openpbr_saturate(value.y), openpbr_saturate(value.z)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_srgb_channel_to_linear(float value) {
    return value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_source_to_linear(const OpenPbrVec3& value,
    OpenPbrSourceColorSpace source_color_space) {
    if (source_color_space != OpenPbrSourceColorSpace::srgb_texture) {
        return value;
    }
    return {openpbr_srgb_channel_to_linear(value.x), openpbr_srgb_channel_to_linear(value.y),
        openpbr_srgb_channel_to_linear(value.z)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE void openpbr_apply_color_input(OpenPbrCoreMaterial& material,
    OpenPbrColorInput input, const OpenPbrVec3& source_value,
    OpenPbrSourceColorSpace source_color_space) {
    const OpenPbrVec3 value = openpbr_source_to_linear(source_value, source_color_space);
    switch (input) {
        case OpenPbrColorInput::base_color: material.base_color = value; break;
        case OpenPbrColorInput::specular_color: material.specular_color = value; break;
        case OpenPbrColorInput::transmission_color: material.transmission_color = value; break;
        case OpenPbrColorInput::emission_color: material.emission_color = value; break;
    }
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_luminance(const OpenPbrVec3& value) {
    return 0.2126f * openpbr_max(0.0f, value.x) + 0.7152f * openpbr_max(0.0f, value.y)
           + 0.0722f * openpbr_max(0.0f, value.z);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_surface_transmission_color(
    const OpenPbrCoreMaterial& material) {
    if (material.geometry_thin_walled != 0 || material.transmission_depth <= 0.0f) {
        return openpbr_clamp_unit(material.transmission_color);
    }
    return openpbr_make_vec3(1.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_transmission_extinction(
    const OpenPbrCoreMaterial& material) {
    const float depth = openpbr_max(0.0f, material.transmission_depth);
    if (material.geometry_thin_walled != 0 || depth <= 0.0f) {
        return {};
    }
    const OpenPbrVec3 color = openpbr_clamp_unit(material.transmission_color);
    return {-logf(openpbr_max(color.x, 1e-6f)) / depth, -logf(openpbr_max(color.y, 1e-6f)) / depth,
        -logf(openpbr_max(color.z, 1e-6f)) / depth};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_transmission_at_distance(
    const OpenPbrCoreMaterial& material, float distance) {
    const OpenPbrVec3 extinction = openpbr_transmission_extinction(material);
    const float safe_distance = openpbr_max(0.0f, distance);
    return {expf(-extinction.x * safe_distance), expf(-extinction.y * safe_distance),
        expf(-extinction.z * safe_distance)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_pow5(float value) {
    const float square = value * value;
    return square * square * value;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_schlick(const OpenPbrVec3& color0,
    const OpenPbrVec3& color90, float cosine) {
    const float factor = openpbr_pow5(1.0f - openpbr_saturate(cosine));
    return openpbr_lerp(color0, color90, factor);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrFrame make_openpbr_frame(const OpenPbrVec3& normal,
    const OpenPbrVec3& tangent) {
    OpenPbrFrame frame {};
    frame.normal = openpbr_normalize(normal);
    if (openpbr_length_squared(frame.normal) <= 0.0f) {
        frame.normal = {0.0f, 0.0f, 1.0f};
    }

    const OpenPbrVec3 projected_tangent =
        openpbr_sub(tangent, openpbr_mul(frame.normal, openpbr_dot(frame.normal, tangent)));
    if (openpbr_length_squared(projected_tangent) > 1e-12f) {
        frame.tangent = openpbr_normalize(projected_tangent);
    } else {
        const OpenPbrVec3 helper = fabsf(frame.normal.z) < 0.999f ? OpenPbrVec3 {0.0f, 0.0f, 1.0f}
                                                                  : OpenPbrVec3 {0.0f, 1.0f, 0.0f};
        frame.tangent = openpbr_normalize(openpbr_cross(helper, frame.normal));
    }
    frame.bitangent = openpbr_normalize(openpbr_cross(frame.normal, frame.tangent));
    frame.tangent = openpbr_normalize(openpbr_cross(frame.bitangent, frame.normal));
    return frame;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrFrame openpbr_face_frame(OpenPbrFrame frame,
    const OpenPbrVec3& wo) {
    if (openpbr_dot(frame.normal, wo) < 0.0f) {
        frame.normal = openpbr_negate(frame.normal);
        frame.bitangent = openpbr_negate(frame.bitangent);
    }
    return frame;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_to_local(const OpenPbrFrame& frame,
    const OpenPbrVec3& value) {
    return {openpbr_dot(value, frame.tangent), openpbr_dot(value, frame.bitangent),
        openpbr_dot(value, frame.normal)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_to_world(const OpenPbrFrame& frame,
    const OpenPbrVec3& value) {
    return openpbr_add(
        openpbr_add(openpbr_mul(frame.tangent, value.x), openpbr_mul(frame.bitangent, value.y)),
        openpbr_mul(frame.normal, value.z));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_fresnel_dielectric(float cosine, float eta) {
    cosine = openpbr_clamp(cosine, -1.0f, 1.0f);
    eta = openpbr_max(eta, 1e-4f);
    if (cosine < 0.0f) {
        eta = 1.0f / eta;
        cosine = -cosine;
    }
    const float sin2_incident = openpbr_max(0.0f, 1.0f - cosine * cosine);
    const float sin2_transmitted = sin2_incident / (eta * eta);
    if (sin2_transmitted >= 1.0f) {
        return 1.0f;
    }
    const float cos_transmitted = openpbr_safe_sqrt(1.0f - sin2_transmitted);
    const float parallel =
        (eta * cosine - cos_transmitted) / openpbr_max(eta * cosine + cos_transmitted, 1e-8f);
    const float perpendicular =
        (cosine - eta * cos_transmitted) / openpbr_max(cosine + eta * cos_transmitted, 1e-8f);
    return 0.5f * (parallel * parallel + perpendicular * perpendicular);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_modulated_ior(const OpenPbrCoreMaterial& material) {
    const float eta = openpbr_max(material.specular_ior, 1e-4f);
    const float ratio = (eta - 1.0f) / (eta + 1.0f);
    const float scaled_f0 =
        openpbr_clamp(openpbr_max(0.0f, material.specular_weight) * ratio * ratio, 0.0f, 0.9999f);
    const float sign = eta < 1.0f ? -1.0f : (eta > 1.0f ? 1.0f : 0.0f);
    const float epsilon = sign * sqrtf(scaled_f0);
    return openpbr_max((1.0f + epsilon) / openpbr_max(1.0f - epsilon, 1e-4f), 1e-4f);
}

struct OpenPbrRoughness {
    float alpha_x = 0.09f;
    float alpha_y = 0.09f;
};

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrRoughness openpbr_roughness(
    const OpenPbrCoreMaterial& material) {
    const float roughness = openpbr_saturate(material.specular_roughness);
    const float anisotropy = openpbr_saturate(material.specular_roughness_anisotropy);
    const float aspect = 1.0f - anisotropy;
    const float scale = sqrtf(2.0f / openpbr_max(aspect * aspect + 1.0f, 1e-8f));
    const float alpha_x = openpbr_max(roughness * roughness * scale, 1e-4f);
    return {alpha_x, openpbr_max(aspect * alpha_x, 1e-4f)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_effectively_smooth(
    const OpenPbrCoreMaterial& material) {
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    return openpbr_max(roughness.alpha_x, roughness.alpha_y) < 1e-3f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_average_alpha(const OpenPbrRoughness& roughness) {
    return sqrtf(roughness.alpha_x * roughness.alpha_y);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_directional_albedo(float cosine, float alpha,
    float color0, float color90) {
    const float x = openpbr_saturate(cosine);
    const float y = openpbr_saturate(alpha);
    const float x2 = x * x;
    const float y2 = y * y;
    const float numerator_a = 0.1003f - 0.6303f * x + 9.748f * y - 2.038f * x * y + 29.34f * x2
                              - 8.245f * y2 - 26.44f * x2 * y + 19.99f * x * y2 - 5.448f * x2 * y2;
    const float numerator_b = 0.9345f - 2.323f * x + 2.229f * y - 3.748f * x * y + 1.424f * x2
                              - 0.7684f * y2 + 1.436f * x2 * y + 0.2913f * x * y2
                              + 0.6286f * x2 * y2;
    const float denominator_a = 1.0f - 1.765f * x + 8.263f * y + 11.53f * x * y + 28.96f * x2
                                - 7.507f * y2 - 36.11f * x2 * y + 15.86f * x * y2
                                + 33.37f * x2 * y2;
    const float denominator_b = 1.0f + 0.2281f * x + 15.94f * y - 55.83f * x * y + 13.08f * x2
                                + 41.26f * y2 + 54.9f * x2 * y + 300.2f * x * y2 - 285.1f * x2 * y2;
    const float coefficient_a = openpbr_saturate(numerator_a / openpbr_max(denominator_a, 1e-6f));
    const float coefficient_b = openpbr_saturate(numerator_b / openpbr_max(denominator_b, 1e-6f));
    return openpbr_saturate(color0) * coefficient_a + openpbr_saturate(color90) * coefficient_b;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_ggx_energy_compensation(float cosine,
    float alpha, const OpenPbrVec3& single_scatter_fresnel) {
    if (alpha < 1e-3f) {
        return openpbr_make_vec3(1.0f);
    }
    const float single_scatter_albedo = openpbr_ggx_directional_albedo(cosine, alpha, 1.0f, 1.0f);
    const float missing_ratio =
        (1.0f - single_scatter_albedo) / openpbr_max(single_scatter_albedo, 1e-4f);
    return openpbr_add(openpbr_make_vec3(1.0f),
        openpbr_mul(openpbr_clamp_unit(single_scatter_fresnel), missing_ratio));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_d(const OpenPbrRoughness& roughness,
    const OpenPbrVec3& wm) {
    if (wm.z <= 0.0f) {
        return 0.0f;
    }
    const float x = wm.x / roughness.alpha_x;
    const float y = wm.y / roughness.alpha_y;
    const float denominator = x * x + y * y + wm.z * wm.z;
    return 1.0f / (kOpenPbrPi * roughness.alpha_x * roughness.alpha_y * denominator * denominator);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_lambda(const OpenPbrRoughness& roughness,
    const OpenPbrVec3& value) {
    const float abs_z = fabsf(value.z);
    if (abs_z <= 1e-8f) {
        return 1e20f;
    }
    const float projected = roughness.alpha_x * roughness.alpha_x * value.x * value.x
                            + roughness.alpha_y * roughness.alpha_y * value.y * value.y;
    const float alpha_tan2 = projected / (abs_z * abs_z);
    return 0.5f * (sqrtf(1.0f + alpha_tan2) - 1.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_g1(const OpenPbrRoughness& roughness,
    const OpenPbrVec3& value) {
    return 1.0f / (1.0f + openpbr_ggx_lambda(roughness, value));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_g(const OpenPbrRoughness& roughness,
    const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    return 1.0f / (1.0f + openpbr_ggx_lambda(roughness, wo) + openpbr_ggx_lambda(roughness, wi));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_ggx_visible_normal_pdf(
    const OpenPbrRoughness& roughness, const OpenPbrVec3& wo, const OpenPbrVec3& wm) {
    const float abs_cos_o = fabsf(wo.z);
    if (abs_cos_o <= 1e-8f) {
        return 0.0f;
    }
    return openpbr_ggx_g1(roughness, wo) * openpbr_ggx_d(roughness, wm) * fabsf(openpbr_dot(wo, wm))
           / abs_cos_o;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sample_visible_normal(
    const OpenPbrRoughness& roughness, const OpenPbrVec3& wo, float u1, float u2) {
    OpenPbrVec3 wh = openpbr_normalize({roughness.alpha_x * wo.x, roughness.alpha_y * wo.y, wo.z});
    if (wh.z < 0.0f) {
        wh = openpbr_negate(wh);
    }

    const OpenPbrVec3 t1 = wh.z < 0.99999f
                               ? openpbr_normalize(openpbr_cross({0.0f, 0.0f, 1.0f}, wh))
                               : OpenPbrVec3 {1.0f, 0.0f, 0.0f};
    const OpenPbrVec3 t2 = openpbr_cross(wh, t1);

    const float radius = sqrtf(openpbr_saturate(u1));
    const float phi = 2.0f * kOpenPbrPi * openpbr_saturate(u2);
    const float disk_x = radius * cosf(phi);
    float disk_y = radius * sinf(phi);
    const float horizon = openpbr_safe_sqrt(1.0f - disk_x * disk_x);
    const float warp = 0.5f * (1.0f + wh.z);
    disk_y = horizon * (1.0f - warp) + disk_y * warp;

    const float disk_z = openpbr_safe_sqrt(1.0f - disk_x * disk_x - disk_y * disk_y);
    const OpenPbrVec3 nh = openpbr_add(
        openpbr_add(openpbr_mul(t1, disk_x), openpbr_mul(t2, disk_y)), openpbr_mul(wh, disk_z));
    return openpbr_normalize(
        {roughness.alpha_x * nh.x, roughness.alpha_y * nh.y, openpbr_max(1e-6f, nh.z)});
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_reflect(const OpenPbrVec3& wo,
    const OpenPbrVec3& wm) {
    return openpbr_sub(openpbr_mul(wm, 2.0f * openpbr_dot(wo, wm)), wo);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_refract(const OpenPbrVec3& wo, OpenPbrVec3 wm,
    float eta, float& eta_path, OpenPbrVec3& wi) {
    float cos_incident = openpbr_dot(wm, wo);
    eta_path = openpbr_max(eta, 1e-4f);
    if (cos_incident < 0.0f) {
        wm = openpbr_negate(wm);
        cos_incident = -cos_incident;
        eta_path = 1.0f / eta_path;
    }
    const float sin2_incident = openpbr_max(0.0f, 1.0f - cos_incident * cos_incident);
    const float sin2_transmitted = sin2_incident / (eta_path * eta_path);
    if (sin2_transmitted >= 1.0f) {
        return false;
    }
    const float cos_transmitted = openpbr_safe_sqrt(1.0f - sin2_transmitted);
    wi = openpbr_add(openpbr_mul(wo, -1.0f / eta_path),
        openpbr_mul(wm, cos_incident / eta_path - cos_transmitted));
    wi = openpbr_normalize(wi);
    return true;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrLobeProbabilities openpbr_lobe_probabilities(
    const OpenPbrCoreMaterial& material) {
    const float metalness = openpbr_saturate(material.base_metalness);
    const float transmission = openpbr_saturate(material.transmission_weight);
    const float base_weight = openpbr_saturate(material.base_weight);
    const float metal_specular_weight = openpbr_saturate(material.specular_weight);
    const float eta = openpbr_modulated_ior(material);
    const float eta_ratio = (eta - 1.0f) / (eta + 1.0f);
    const float dielectric_f0 = eta_ratio * eta_ratio;

    const float diffuse_score = (1.0f - metalness) * (1.0f - transmission) * (1.0f - dielectric_f0)
                                * base_weight
                                * openpbr_luminance(openpbr_clamp_unit(material.base_color));
    const float dielectric_reflection_score =
        (1.0f - metalness) * dielectric_f0
        * openpbr_luminance(openpbr_clamp_unit(material.specular_color));
    const float metal_reflection_score =
        metalness * metal_specular_weight
        * openpbr_luminance(openpbr_mul(openpbr_clamp_unit(material.base_color), base_weight));
    const float transmission_score =
        (1.0f - metalness) * transmission * (1.0f - dielectric_f0)
        * openpbr_luminance(openpbr_surface_transmission_color(material));

    const float reflection_score = dielectric_reflection_score + metal_reflection_score;
    const float total = diffuse_score + reflection_score + transmission_score;
    if (total <= 1e-10f) {
        return {};
    }
    return {diffuse_score / total, reflection_score / total, transmission_score / total};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_oren_nayar_directional_albedo(float cosine,
    float roughness) {
    constexpr float kFujiiConstant1 = 0.5f - 2.0f / (3.0f * kOpenPbrPi);
    const float safe_cosine = openpbr_clamp(cosine, 1e-6f, 1.0f);
    const float sigma = openpbr_saturate(roughness);
    const float coefficient_a = 1.0f / (1.0f + kFujiiConstant1 * sigma);
    const float coefficient_b = sigma * coefficient_a;
    const float sine = openpbr_safe_sqrt(1.0f - safe_cosine * safe_cosine);
    const float g = sine * (acosf(safe_cosine) - sine * safe_cosine)
                    + 2.0f * ((sine / safe_cosine) * (1.0f - sine * sine * sine) - sine) / 3.0f;
    return openpbr_saturate(coefficient_a + coefficient_b * g * kOpenPbrInvPi);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_oren_nayar_average_albedo(float roughness) {
    constexpr float kFujiiConstant1 = 0.5f - 2.0f / (3.0f * kOpenPbrPi);
    constexpr float kFujiiConstant2 = 2.0f / 3.0f - 28.0f / (15.0f * kOpenPbrPi);
    const float sigma = openpbr_saturate(roughness);
    const float coefficient_a = 1.0f / (1.0f + kFujiiConstant1 * sigma);
    return openpbr_saturate(coefficient_a * (1.0f + kFujiiConstant2 * sigma));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_diffuse_value(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    if (wo.z <= 0.0f || wi.z <= 0.0f) {
        return {};
    }
    const float metalness = openpbr_saturate(material.base_metalness);
    const float transmission = openpbr_saturate(material.transmission_weight);
    const float coefficient =
        openpbr_saturate(material.base_weight) * (1.0f - metalness) * (1.0f - transmission);
    if (coefficient <= 0.0f) {
        return {};
    }

    constexpr float kFujiiConstant1 = 0.5f - 2.0f / (3.0f * kOpenPbrPi);
    const float sigma = openpbr_saturate(material.base_diffuse_roughness);
    const float coefficient_a = 1.0f / (1.0f + kFujiiConstant1 * sigma);
    const float s = openpbr_dot(wi, wo) - wi.z * wo.z;
    const float s_over_t = s > 0.0f ? s / openpbr_max(wi.z, wo.z) : s;
    const OpenPbrVec3 color = openpbr_clamp_unit(material.base_color);
    const OpenPbrVec3 single_scatter =
        openpbr_mul(color, coefficient_a * (1.0f + sigma * s_over_t));

    const float directional_in = openpbr_oren_nayar_directional_albedo(wi.z, sigma);
    const float directional_out = openpbr_oren_nayar_directional_albedo(wo.z, sigma);
    const float average = openpbr_oren_nayar_average_albedo(sigma);
    const float missing_average = openpbr_max(1.0f - average, 1e-6f);
    const OpenPbrVec3 color_squared = openpbr_mul(color, color);
    const OpenPbrVec3 multi_scatter_color {
        color_squared.x * average / openpbr_max(1.0f - color.x * (1.0f - average), 1e-6f),
        color_squared.y * average / openpbr_max(1.0f - color.y * (1.0f - average), 1e-6f),
        color_squared.z * average / openpbr_max(1.0f - color.z * (1.0f - average), 1e-6f),
    };
    const float multi_scatter_scale =
        (1.0f - directional_in) * (1.0f - directional_out) / missing_average;
    const OpenPbrVec3 multi_scatter = openpbr_mul(multi_scatter_color, multi_scatter_scale);
    return openpbr_mul(openpbr_add(single_scatter, multi_scatter), coefficient * kOpenPbrInvPi);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_dielectric_fresnel(
    const OpenPbrCoreMaterial& material, float cosine, bool entering) {
    float eta = openpbr_modulated_ior(material);
    if (material.geometry_thin_walled == 0 && !entering) {
        eta = 1.0f / eta;
    }
    float fresnel = openpbr_fresnel_dielectric(cosine, eta);
    if (material.geometry_thin_walled != 0 && fresnel < 1.0f) {
        const float transmission = 1.0f - fresnel;
        fresnel +=
            transmission * transmission * fresnel / openpbr_max(1.0f - fresnel * fresnel, 1e-6f);
    }
    return openpbr_saturate(fresnel);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_metal_fresnel(
    const OpenPbrCoreMaterial& material, float cosine) {
    constexpr float kEdgeCosine = 1.0f / 7.0f;
    const float mu = openpbr_saturate(fabsf(cosine));
    const OpenPbrVec3 color0 = openpbr_mul(openpbr_clamp_unit(material.base_color),
        openpbr_saturate(material.base_weight));
    const OpenPbrVec3 color90 = openpbr_make_vec3(1.0f);
    const OpenPbrVec3 schlick = openpbr_schlick(color0, color90, mu);
    const OpenPbrVec3 schlick_at_edge = openpbr_schlick(color0, color90, kEdgeCosine);
    const OpenPbrVec3 desired_edge =
        openpbr_mul(schlick_at_edge, openpbr_clamp_unit(material.specular_color));
    const float one_minus_mu = 1.0f - mu;
    const float one_minus_edge = 1.0f - kEdgeCosine;
    const float correction = mu * openpbr_pow5(one_minus_mu) * one_minus_mu
                             / (kEdgeCosine * openpbr_pow5(one_minus_edge) * one_minus_edge);
    const OpenPbrVec3 f82 =
        openpbr_sub(schlick, openpbr_mul(openpbr_sub(schlick_at_edge, desired_edge), correction));
    return openpbr_mul(openpbr_clamp_unit(f82), openpbr_saturate(material.specular_weight));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_dielectric_directional_albedo(
    const OpenPbrCoreMaterial& material, float cosine, bool entering) {
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const float alpha = openpbr_average_alpha(roughness);
    if (openpbr_effectively_smooth(material)) {
        return openpbr_dielectric_fresnel(material, cosine, entering);
    }
    const float color0 = openpbr_dielectric_fresnel(material, 1.0f, entering);
    if (color0 <= 1e-8f) {
        return 0.0f;
    }
    const float single_scatter = openpbr_ggx_directional_albedo(cosine, alpha, color0, 1.0f);
    const float white_single_scatter = openpbr_ggx_directional_albedo(cosine, alpha, 1.0f, 1.0f);
    const float directional_fresnel = openpbr_dielectric_fresnel(material, cosine, entering);
    const float compensation = 1.0f
                               + directional_fresnel * (1.0f - white_single_scatter)
                                     / openpbr_max(white_single_scatter, 1e-4f);
    return openpbr_saturate(single_scatter * compensation);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_reflection_fresnel(
    const OpenPbrCoreMaterial& material, float microfacet_cosine, float outgoing_cosine,
    bool entering) {
    const float metalness = openpbr_saturate(material.base_metalness);
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const float alpha = openpbr_average_alpha(roughness);

    const float dielectric_f = openpbr_dielectric_fresnel(material, microfacet_cosine, entering);
    const OpenPbrVec3 dielectric_single = openpbr_mul(
        entering || material.geometry_thin_walled != 0 ? openpbr_clamp_unit(material.specular_color)
                                                       : openpbr_make_vec3(1.0f),
        dielectric_f);
    const OpenPbrVec3 dielectric_compensation =
        openpbr_ggx_energy_compensation(outgoing_cosine, alpha, openpbr_make_vec3(dielectric_f));
    const OpenPbrVec3 dielectric = openpbr_mul(dielectric_single, dielectric_compensation);

    const OpenPbrVec3 metal_single = openpbr_metal_fresnel(material, microfacet_cosine);
    const OpenPbrVec3 metal_compensation =
        openpbr_ggx_energy_compensation(outgoing_cosine, alpha, metal_single);
    const OpenPbrVec3 metal = openpbr_mul(metal_single, metal_compensation);
    return openpbr_lerp(dielectric, metal, metalness);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrEvaluation evaluate_openpbr_core(
    const OpenPbrCoreMaterial& material, OpenPbrFrame frame, const OpenPbrVec3& wo_world,
    const OpenPbrVec3& wi_world) {
    OpenPbrEvaluation result {};
    const OpenPbrVec3 wo_normalized = openpbr_normalize(wo_world);
    const OpenPbrVec3 wi_normalized = openpbr_normalize(wi_world);
    if (openpbr_length_squared(wo_normalized) <= 0.0f
        || openpbr_length_squared(wi_normalized) <= 0.0f) {
        return result;
    }

    const bool entering =
        material.geometry_thin_walled != 0 || openpbr_dot(frame.normal, wo_normalized) >= 0.0f;
    frame = openpbr_face_frame(frame, wo_normalized);
    const OpenPbrVec3 wo = openpbr_to_local(frame, wo_normalized);
    const OpenPbrVec3 wi = openpbr_to_local(frame, wi_normalized);
    if (wo.z <= 1e-7f || fabsf(wi.z) <= 1e-7f) {
        return result;
    }

    const float opacity = openpbr_saturate(material.geometry_opacity);
    const OpenPbrLobeProbabilities probabilities = openpbr_lobe_probabilities(material);
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const bool smooth = openpbr_effectively_smooth(material);
    const bool reflection = wi.z * wo.z > 0.0f;

    if (reflection) {
        const float substrate_attenuation =
            1.0f - openpbr_dielectric_directional_albedo(material, wo.z, entering);
        result.value = openpbr_mul(openpbr_diffuse_value(material, wo, wi), substrate_attenuation);
        result.pdf = probabilities.diffuse * wi.z * kOpenPbrInvPi;

        const OpenPbrVec3 sum = openpbr_add(wi, wo);
        if (!smooth && openpbr_length_squared(sum) > 1e-16f) {
            OpenPbrVec3 wm = openpbr_normalize(sum);
            if (wm.z < 0.0f) {
                wm = openpbr_negate(wm);
            }
            const float wo_dot_m = openpbr_dot(wo, wm);
            if (wo_dot_m > 0.0f) {
                const float distribution = openpbr_ggx_d(roughness, wm);
                const float masking = openpbr_ggx_g(roughness, wo, wi);
                const OpenPbrVec3 fresnel =
                    openpbr_reflection_fresnel(material, wo_dot_m, wo.z, entering);
                const OpenPbrVec3 specular = openpbr_mul(fresnel,
                    distribution * masking / openpbr_max(4.0f * wo.z * wi.z, 1e-8f));
                result.value = openpbr_add(result.value, specular);
                const float microfacet_pdf = openpbr_ggx_visible_normal_pdf(roughness, wo, wm)
                                             / openpbr_max(4.0f * fabsf(wo_dot_m), 1e-8f);
                result.pdf += probabilities.reflection * microfacet_pdf;
            }
        }
    } else if (!smooth && material.geometry_thin_walled == 0 && probabilities.transmission > 0.0f) {
        const float eta = openpbr_modulated_ior(material);
        const float eta_path = entering ? eta : 1.0f / eta;
        const OpenPbrVec3 sum = openpbr_add(openpbr_mul(wi, eta_path), wo);
        if (openpbr_length_squared(sum) > 1e-16f) {
            OpenPbrVec3 wm = openpbr_normalize(sum);
            if (wm.z < 0.0f) {
                wm = openpbr_negate(wm);
            }
            const float wi_dot_m = openpbr_dot(wi, wm);
            const float wo_dot_m = openpbr_dot(wo, wm);
            if (wi_dot_m * wi.z > 0.0f && wo_dot_m * wo.z > 0.0f) {
                const float fresnel = openpbr_fresnel_dielectric(wo_dot_m, eta_path);
                const float half_denominator = wi_dot_m + wo_dot_m / eta_path;
                const float denominator_squared = half_denominator * half_denominator;
                if (denominator_squared > 1e-16f) {
                    const float distribution = openpbr_ggx_d(roughness, wm);
                    const float masking = openpbr_ggx_g(roughness, wo, wi);
                    float transmission =
                        distribution * (1.0f - fresnel) * masking
                        * fabsf(wi_dot_m * wo_dot_m / (wi.z * wo.z * denominator_squared));
                    transmission /= eta_path * eta_path;
                    transmission *= (1.0f - openpbr_saturate(material.base_metalness))
                                    * openpbr_saturate(material.transmission_weight);
                    result.value =
                        openpbr_mul(openpbr_surface_transmission_color(material), transmission);

                    const float derivative = fabsf(wi_dot_m) / denominator_squared;
                    const float microfacet_pdf =
                        openpbr_ggx_visible_normal_pdf(roughness, wo, wm) * derivative;
                    result.pdf = probabilities.transmission * microfacet_pdf;
                }
            }
        }
    }

    result.value = openpbr_mul(result.value, opacity);
    result.pdf *= opacity;
    return result;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float pdf_openpbr_core(const OpenPbrCoreMaterial& material,
    OpenPbrFrame frame, const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    return evaluate_openpbr_core(material, frame, wo, wi).pdf;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 emission_openpbr_core(
    const OpenPbrCoreMaterial& material) {
    return openpbr_mul(openpbr_clamp_nonnegative(material.emission_color),
        openpbr_max(0.0f, material.emission_luminance)
            * openpbr_saturate(material.geometry_opacity));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sample_cosine_hemisphere(float u1, float u2) {
    const float radius = sqrtf(openpbr_saturate(u1));
    const float phi = 2.0f * kOpenPbrPi * openpbr_saturate(u2);
    return {radius * cosf(phi), radius * sinf(phi), openpbr_safe_sqrt(1.0f - radius * radius)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrSample sample_openpbr_core(
    const OpenPbrCoreMaterial& material, OpenPbrFrame frame, const OpenPbrVec3& wo_world,
    float u_lobe, float u1, float u2) {
    OpenPbrSample sample {};
    const OpenPbrVec3 wo_normalized = openpbr_normalize(wo_world);
    if (openpbr_length_squared(wo_normalized) <= 0.0f) {
        return sample;
    }
    const OpenPbrFrame authored_frame = frame;
    const bool entering =
        material.geometry_thin_walled != 0 || openpbr_dot(frame.normal, wo_normalized) >= 0.0f;
    frame = openpbr_face_frame(frame, wo_normalized);
    const OpenPbrVec3 wo = openpbr_to_local(frame, wo_normalized);
    if (wo.z <= 1e-7f) {
        return sample;
    }

    const float opacity = openpbr_saturate(material.geometry_opacity);
    const float passthrough_probability = 1.0f - opacity;
    u_lobe = openpbr_saturate(u_lobe);
    if (u_lobe < passthrough_probability) {
        sample.wi = openpbr_negate(wo_normalized);
        sample.weight = openpbr_make_vec3(1.0f);
        sample.discrete_pdf = passthrough_probability;
        sample.event = OpenPbrScatterEvent::opacity_passthrough;
        sample.delta = 1;
        sample.valid = 1;
        return sample;
    }
    if (opacity <= 0.0f) {
        return sample;
    }

    const OpenPbrLobeProbabilities probabilities = openpbr_lobe_probabilities(material);
    if (probabilities.diffuse + probabilities.reflection + probabilities.transmission <= 0.0f) {
        return sample;
    }
    const float surface_choice =
        openpbr_clamp((u_lobe - passthrough_probability) / opacity, 0.0f, 0.99999994f);
    OpenPbrVec3 wi {};

    if (surface_choice < probabilities.diffuse) {
        wi = openpbr_sample_cosine_hemisphere(u1, u2);
        sample.event = OpenPbrScatterEvent::diffuse_reflection;
    } else if (surface_choice < probabilities.diffuse + probabilities.reflection) {
        if (openpbr_effectively_smooth(material)) {
            wi = {-wo.x, -wo.y, wo.z};
            const OpenPbrVec3 coefficient =
                openpbr_mul(openpbr_reflection_fresnel(material, wo.z, wo.z, entering), opacity);
            sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
            sample.discrete_pdf = opacity * probabilities.reflection;
            if (sample.discrete_pdf <= 0.0f) {
                return OpenPbrSample {};
            }
            sample.weight = openpbr_div(coefficient, sample.discrete_pdf);
            sample.event = OpenPbrScatterEvent::glossy_reflection;
            sample.delta = 1;
            sample.valid = 1;
            return sample;
        }
        const OpenPbrRoughness roughness = openpbr_roughness(material);
        const OpenPbrVec3 wm = openpbr_sample_visible_normal(roughness, wo, u1, u2);
        wi = openpbr_reflect(wo, wm);
        if (wi.z <= 1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::glossy_reflection;
    } else if (material.geometry_thin_walled != 0) {
        wi = openpbr_negate(wo);
        const float reflection = openpbr_dielectric_fresnel(material, wo.z, true);
        const OpenPbrVec3 coefficient = openpbr_mul(openpbr_surface_transmission_color(material),
            opacity * (1.0f - openpbr_saturate(material.base_metalness))
                * openpbr_saturate(material.transmission_weight) * (1.0f - reflection));
        sample.wi = openpbr_to_world(frame, wi);
        sample.discrete_pdf = opacity * probabilities.transmission;
        if (sample.discrete_pdf <= 0.0f) {
            return OpenPbrSample {};
        }
        sample.weight = openpbr_div(coefficient, sample.discrete_pdf);
        sample.event = OpenPbrScatterEvent::thin_walled_transmission;
        sample.delta = 1;
        sample.valid = 1;
        return sample;
    } else if (openpbr_effectively_smooth(material)) {
        const float material_eta_path =
            entering ? openpbr_modulated_ior(material) : 1.0f / openpbr_modulated_ior(material);
        float refracted_eta_path = 1.0f;
        if (!openpbr_refract(wo, {0.0f, 0.0f, 1.0f}, material_eta_path, refracted_eta_path, wi)) {
            return sample;
        }
        const float fresnel = openpbr_fresnel_dielectric(wo.z, material_eta_path);
        const OpenPbrVec3 coefficient = openpbr_mul(openpbr_surface_transmission_color(material),
            opacity * (1.0f - openpbr_saturate(material.base_metalness))
                * openpbr_saturate(material.transmission_weight) * (1.0f - fresnel)
                / (refracted_eta_path * refracted_eta_path));
        sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
        sample.discrete_pdf = opacity * probabilities.transmission;
        if (sample.discrete_pdf <= 0.0f) {
            return OpenPbrSample {};
        }
        sample.weight = openpbr_div(coefficient, sample.discrete_pdf);
        sample.event = OpenPbrScatterEvent::glossy_transmission;
        sample.delta = 1;
        sample.valid = 1;
        return sample;
    } else {
        const OpenPbrRoughness roughness = openpbr_roughness(material);
        const OpenPbrVec3 wm = openpbr_sample_visible_normal(roughness, wo, u1, u2);
        const float material_eta_path =
            entering ? openpbr_modulated_ior(material) : 1.0f / openpbr_modulated_ior(material);
        float eta_path = 1.0f;
        if (!openpbr_refract(wo, wm, material_eta_path, eta_path, wi) || wi.z >= -1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::glossy_transmission;
    }

    sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
    const OpenPbrEvaluation evaluation =
        evaluate_openpbr_core(material, authored_frame, wo_normalized, sample.wi);
    if (evaluation.pdf <= 1e-12f) {
        return OpenPbrSample {};
    }
    sample.value = evaluation.value;
    sample.pdf = evaluation.pdf;
    sample.weight =
        openpbr_mul(evaluation.value, fabsf(openpbr_dot(frame.normal, sample.wi)) / evaluation.pdf);
    sample.valid = 1;
    return sample;
}

} // namespace rt

#undef RT_OPENPBR_HD
#undef RT_OPENPBR_INLINE
