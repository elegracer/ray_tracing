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
inline constexpr float kOpenPbrFraunhoferCnm = 656.3f;
inline constexpr float kOpenPbrFraunhoferDnm = 587.6f;
inline constexpr float kOpenPbrFraunhoferFnm = 486.1f;

struct OpenPbrVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct OpenPbrTransportContext {
    OpenPbrVec3 rgb_wavelengths_nm {
        kOpenPbrFraunhoferCnm, kOpenPbrFraunhoferDnm, kOpenPbrFraunhoferFnm};
    OpenPbrVec3 path_throughput {1.0f, 1.0f, 1.0f};
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
    float transmission_dispersion_scale = 0.0f;
    float transmission_dispersion_abbe_number = 20.0f;

    float subsurface_weight = 0.0f;
    OpenPbrVec3 subsurface_color {0.8f, 0.8f, 0.8f};
    float subsurface_radius = 1.0f;
    OpenPbrVec3 subsurface_radius_scale {1.0f, 0.5f, 0.25f};
    float subsurface_scatter_anisotropy = 0.0f;

    float fuzz_weight = 0.0f;
    OpenPbrVec3 fuzz_color {1.0f, 1.0f, 1.0f};
    float fuzz_roughness = 0.5f;

    float coat_weight = 0.0f;
    OpenPbrVec3 coat_color {1.0f, 1.0f, 1.0f};
    float coat_roughness = 0.0f;
    float coat_roughness_anisotropy = 0.0f;
    float coat_ior = 1.6f;
    float coat_darkening = 1.0f;

    float thin_film_weight = 0.0f;
    float thin_film_thickness = 0.5f;
    float thin_film_ior = 1.4f;

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

enum class OpenPbrScalarInput : int {
    base_metalness = 0,
    specular_roughness = 1,
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

struct OpenPbrScalarTextureBinding {
    int texture_index = -1;
};

struct OpenPbrScalarTextureBindings {
    OpenPbrScalarTextureBinding base_metalness {};
    OpenPbrScalarTextureBinding specular_roughness {};
};

struct OpenPbrCompiledMaterial {
    OpenPbrCoreMaterial parameters {};
    OpenPbrColorTextureBindings color_textures {};
    OpenPbrScalarTextureBindings scalar_textures {};
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
    coat_reflection = 6,
    fuzz_reflection = 7,
    subsurface_entry = 8,
    subsurface_exit = 9,
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
    float subsurface = 0.0f;
    float reflection = 0.0f;
    float transmission = 0.0f;
    float coat = 0.0f;
    float fuzz = 0.0f;
};

struct OpenPbrSubsurfaceMedium {
    OpenPbrVec3 extinction {};
    OpenPbrVec3 single_scattering_albedo {};
    float anisotropy = 0.0f;
    int active = 0;
};

struct OpenPbrSubsurfaceSegment {
    OpenPbrVec3 weight {1.0f, 1.0f, 1.0f};
    float distance = 0.0f;
    int scattered = 0;
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

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_component(
    const OpenPbrVec3& value, int channel) {
    return channel == 0 ? value.x : (channel == 1 ? value.y : value.z);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE void openpbr_set_component(
    OpenPbrVec3& value, int channel, float component) {
    if (channel == 0) {
        value.x = component;
    } else if (channel == 1) {
        value.y = component;
    } else {
        value.z = component;
    }
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_reciprocal(
    const OpenPbrVec3& value) {
    return {1.0f / openpbr_max(value.x, 1e-6f), 1.0f / openpbr_max(value.y, 1e-6f),
        1.0f / openpbr_max(value.z, 1e-6f)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_channel_probabilities(
    const OpenPbrTransportContext& context) {
    const OpenPbrVec3 throughput {openpbr_max(0.0f, context.path_throughput.x),
        openpbr_max(0.0f, context.path_throughput.y),
        openpbr_max(0.0f, context.path_throughput.z)};
    const float total = throughput.x + throughput.y + throughput.z;
    return total > 1e-8f ? openpbr_div(throughput, total)
                         : openpbr_make_vec3(1.0f / 3.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE int openpbr_sample_channel(
    const OpenPbrTransportContext& context, float sample) {
    const OpenPbrVec3 probabilities = openpbr_channel_probabilities(context);
    const float u = openpbr_clamp(sample, 0.0f, 0.99999994f);
    return u < probabilities.x ? 0 : (u < probabilities.x + probabilities.y ? 1 : 2);
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

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sqrt(const OpenPbrVec3& value) {
    return {openpbr_safe_sqrt(value.x), openpbr_safe_sqrt(value.y),
        openpbr_safe_sqrt(value.z)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_pow(const OpenPbrVec3& value,
    float exponent) {
    return {powf(openpbr_max(value.x, 0.0f), exponent),
        powf(openpbr_max(value.y, 0.0f), exponent),
        powf(openpbr_max(value.z, 0.0f), exponent)};
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

RT_OPENPBR_HD RT_OPENPBR_INLINE void openpbr_apply_scalar_input(OpenPbrCoreMaterial& material,
    OpenPbrScalarInput input, float value) {
    switch (input) {
        case OpenPbrScalarInput::base_metalness: material.base_metalness = value; break;
        case OpenPbrScalarInput::specular_roughness: material.specular_roughness = value; break;
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

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_subsurface_single_scattering_albedo(
    float observed_color, float anisotropy) {
    const float color = openpbr_saturate(observed_color);
    const float g = openpbr_clamp(anisotropy, -0.999f, 0.999f);
    const float s = openpbr_max(0.0f,
        4.09712f + 4.20863f * color
            - sqrtf(openpbr_max(
                0.0f, 9.59217f + 41.6808f * color + 17.7126f * color * color)));
    const float s_squared = s * s;
    return openpbr_saturate(
        (1.0f - s_squared) / openpbr_max(1.0f - g * s_squared, 1e-6f));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_subsurface_has_finite_mfp(
    const OpenPbrCoreMaterial& material) {
    const float radius = openpbr_max(0.0f, material.subsurface_radius);
    const OpenPbrVec3 scale = openpbr_clamp_nonnegative(material.subsurface_radius_scale);
    return radius * openpbr_max(scale.x, openpbr_max(scale.y, scale.z)) > 1e-6f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_subsurface_random_walk_enabled(
    const OpenPbrCoreMaterial& material) {
    return material.geometry_thin_walled == 0 && openpbr_saturate(material.subsurface_weight) > 0.0f
           && openpbr_saturate(material.base_metalness) < 1.0f
           && openpbr_saturate(material.transmission_weight) < 1.0f
           && openpbr_subsurface_has_finite_mfp(material);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_subsurface_thin_walled_enabled(
    const OpenPbrCoreMaterial& material) {
    return material.geometry_thin_walled != 0
           && openpbr_saturate(material.subsurface_weight) > 0.0f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_subsurface_thin_walled_probability(
    const OpenPbrCoreMaterial& material, bool transmission) {
    const float anisotropy =
        openpbr_clamp(material.subsurface_scatter_anisotropy, -1.0f, 1.0f);
    return 0.5f * (1.0f + (transmission ? anisotropy : -anisotropy));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrSubsurfaceMedium openpbr_subsurface_medium(
    const OpenPbrCoreMaterial& material) {
    OpenPbrSubsurfaceMedium medium {};
    if (!openpbr_subsurface_random_walk_enabled(material)) {
        return medium;
    }
    const float radius = openpbr_max(0.0f, material.subsurface_radius);
    const OpenPbrVec3 radius_scale =
        openpbr_clamp_nonnegative(material.subsurface_radius_scale);
    const OpenPbrVec3 mean_free_path {
        openpbr_max(radius * radius_scale.x, 1e-6f),
        openpbr_max(radius * radius_scale.y, 1e-6f),
        openpbr_max(radius * radius_scale.z, 1e-6f),
    };
    medium.extinction = openpbr_reciprocal(mean_free_path);
    const OpenPbrVec3 color = openpbr_clamp_unit(material.subsurface_color);
    medium.single_scattering_albedo = {
        openpbr_subsurface_single_scattering_albedo(
            color.x, material.subsurface_scatter_anisotropy),
        openpbr_subsurface_single_scattering_albedo(
            color.y, material.subsurface_scatter_anisotropy),
        openpbr_subsurface_single_scattering_albedo(
            color.z, material.subsurface_scatter_anisotropy),
    };
    medium.anisotropy =
        openpbr_clamp(material.subsurface_scatter_anisotropy, -0.999f, 0.999f);
    medium.active = 1;
    return medium;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_subsurface_channel_probabilities(
    const OpenPbrSubsurfaceMedium& medium, const OpenPbrTransportContext& context) {
    const OpenPbrVec3 contribution = openpbr_mul(
        openpbr_clamp_nonnegative(context.path_throughput), medium.single_scattering_albedo);
    const float total = contribution.x + contribution.y + contribution.z;
    return total > 1e-12f ? openpbr_div(contribution, total)
                          : openpbr_make_vec3(1.0f / 3.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_subsurface_transmittance(
    const OpenPbrSubsurfaceMedium& medium, float distance) {
    const float safe_distance = openpbr_max(0.0f, distance);
    return {expf(-medium.extinction.x * safe_distance),
        expf(-medium.extinction.y * safe_distance),
        expf(-medium.extinction.z * safe_distance)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrSubsurfaceSegment openpbr_sample_subsurface_segment(
    const OpenPbrSubsurfaceMedium& medium, float boundary_distance, float u,
    const OpenPbrTransportContext& context = {}) {
    OpenPbrSubsurfaceSegment result {};
    result.distance = openpbr_max(0.0f, boundary_distance);
    if (medium.active == 0 || result.distance <= 0.0f) {
        return result;
    }

    const OpenPbrVec3 probabilities =
        openpbr_subsurface_channel_probabilities(medium, context);
    const float choice = openpbr_clamp(u, 0.0f, 0.99999994f);
    int channel = 2;
    float channel_u = 0.0f;
    float cdf = 0.0f;
    for (int index = 0; index < 3; ++index) {
        const float probability = openpbr_component(probabilities, index);
        if (choice < cdf + probability || index == 2) {
            channel = index;
            channel_u = (choice - cdf) / openpbr_max(probability, 1e-8f);
            break;
        }
        cdf += probability;
    }

    const float extinction = openpbr_component(medium.extinction, channel);
    const float sampled_distance =
        -logf(openpbr_max(1.0f - openpbr_saturate(channel_u), 1e-7f))
        / openpbr_max(extinction, 1e-6f);
    result.scattered = sampled_distance < result.distance ? 1 : 0;
    result.distance = result.scattered != 0 ? sampled_distance : result.distance;

    const OpenPbrVec3 transmittance =
        openpbr_subsurface_transmittance(medium, result.distance);
    if (result.scattered != 0) {
        const OpenPbrVec3 density = openpbr_mul(medium.extinction, transmittance);
        const float mixture_pdf = openpbr_dot(probabilities, density);
        result.weight = openpbr_div(openpbr_mul(density, medium.single_scattering_albedo),
            openpbr_max(mixture_pdf, 1e-12f));
    } else {
        const float survival_pdf = openpbr_dot(probabilities, transmittance);
        result.weight = openpbr_div(transmittance, openpbr_max(survival_pdf, 1e-12f));
    }
    return result;
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

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sample_henyey_greenstein(
    const OpenPbrVec3& incident_direction, float anisotropy, float u1, float u2) {
    const float g = openpbr_clamp(anisotropy, -0.999f, 0.999f);
    float cosine = 1.0f - 2.0f * openpbr_saturate(u1);
    if (fabsf(g) > 1e-3f) {
        const float ratio = (1.0f - g * g)
                            / openpbr_max(1.0f - g + 2.0f * g * openpbr_saturate(u1), 1e-6f);
        cosine = openpbr_clamp((1.0f + g * g - ratio * ratio) / (2.0f * g), -1.0f, 1.0f);
    }
    const float sine = openpbr_safe_sqrt(1.0f - cosine * cosine);
    const float phi = 2.0f * kOpenPbrPi * openpbr_saturate(u2);
    const OpenPbrVec3 local {sine * cosf(phi), sine * sinf(phi), cosine};
    return openpbr_normalize(openpbr_to_world(
        make_openpbr_frame(openpbr_normalize(incident_direction), {}), local));
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

struct OpenPbrPolarizedReflectance {
    float p = 0.0f;
    float s = 0.0f;
};

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrPolarizedReflectance
openpbr_fresnel_dielectric_polarized(float cosine, float eta) {
    const float cosine2 = openpbr_square(openpbr_saturate(cosine));
    const float sine2 = 1.0f - cosine2;
    const float t0 = openpbr_max(eta * eta - sine2, 0.0f);
    const float t1 = t0 + cosine2;
    const float t2 = 2.0f * sqrtf(t0) * openpbr_saturate(cosine);
    const float rs = (t1 - t2) / openpbr_max(t1 + t2, 1e-8f);
    const float t3 = cosine2 * t0 + sine2 * sine2;
    const float t4 = t2 * sine2;
    const float rp = rs * (t3 - t4) / openpbr_max(t3 + t4, 1e-8f);
    return {rp, rs};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrPolarizedReflectance
openpbr_fresnel_real_ior_polarized(float cosine, float relative_ior) {
    const float cosine2 = openpbr_square(openpbr_saturate(cosine));
    const float sine2 = 1.0f - cosine2;
    const float ior2 = relative_ior * relative_ior;
    const float t0 = ior2 - sine2;
    const float magnitude = fabsf(t0);
    const float t1 = magnitude + cosine2;
    const float a = openpbr_safe_sqrt(0.5f * (magnitude + t0));
    const float t2 = 2.0f * a * openpbr_saturate(cosine);
    const float rs = (t1 - t2) / openpbr_max(t1 + t2, 1e-8f);
    const float t3 = cosine2 * magnitude + sine2 * sine2;
    const float t4 = t2 * sine2;
    const float rp = rs * (t3 - t4) / openpbr_max(t3 + t4, 1e-8f);
    return {rp, rs};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrPolarizedReflectance
openpbr_real_ior_reflection_phase(float cosine, float incident_ior, float transmitted_ior) {
    const float sine2 = 1.0f - openpbr_square(openpbr_saturate(cosine));
    const float transmitted2 = transmitted_ior * transmitted_ior;
    const float incident2 = incident_ior * incident_ior;
    const float a = transmitted2 - incident2 * sine2;
    const float u = openpbr_safe_sqrt(0.5f * (a + fabsf(a)));
    const float s_denominator = u * u - openpbr_square(incident_ior * cosine);
    const float p_denominator = openpbr_square(transmitted2 * cosine) - incident2 * u * u;
    return {p_denominator < 0.0f ? kOpenPbrPi : 0.0f,
        s_denominator < 0.0f ? kOpenPbrPi : 0.0f};
}

// Belcour/Barla spectral sensitivity fit with MaterialX's default two Airy orders.
RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_thin_film_sensitivity(float opd,
    const OpenPbrVec3& shift) {
    const float phase = 2.0f * kOpenPbrPi * opd;
    const OpenPbrVec3 value {5.4856e-13f, 4.4201e-13f, 5.2481e-13f};
    const OpenPbrVec3 position {1.6810e6f, 1.7953e6f, 2.2084e6f};
    const OpenPbrVec3 variance {4.3278e9f, 9.3046e9f, 6.6121e9f};
    OpenPbrVec3 xyz {
        value.x * sqrtf(2.0f * kOpenPbrPi * variance.x)
            * cosf(position.x * phase + shift.x) * expf(-variance.x * phase * phase),
        value.y * sqrtf(2.0f * kOpenPbrPi * variance.y)
            * cosf(position.y * phase + shift.y) * expf(-variance.y * phase * phase),
        value.z * sqrtf(2.0f * kOpenPbrPi * variance.z)
            * cosf(position.z * phase + shift.z) * expf(-variance.z * phase * phase),
    };
    xyz.x += 9.7470e-14f * sqrtf(2.0f * kOpenPbrPi * 4.5282e9f)
             * cosf(2.2399e6f * phase + shift.x) * expf(-4.5282e9f * phase * phase);
    return openpbr_div(xyz, 1.0685e-7f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_xyz_to_rgb(
    const OpenPbrVec3& xyz) {
    return {2.3706743f * xyz.x - 0.9000405f * xyz.y - 0.4706338f * xyz.z,
        -0.5138850f * xyz.x + 1.4253036f * xyz.y + 0.0885814f * xyz.z,
        0.0052982f * xyz.x - 0.0146949f * xyz.y + 1.0093968f * xyz.z};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_f0_to_ior(float f0) {
    const float root = sqrtf(openpbr_clamp(f0, 0.01f, 0.99f));
    return (1.0f + root) / openpbr_max(1.0f - root, 1e-5f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_thin_film_transmitted_cosine(float cosine,
    float incident_ior, float film_ior) {
    const float ratio = incident_ior / openpbr_max(film_ior, 1e-4f);
    const float sine2 = (1.0f - openpbr_square(openpbr_saturate(cosine))) * ratio * ratio;
    return sine2 < 1.0f ? openpbr_safe_sqrt(1.0f - sine2) : 0.0f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_thin_film_airy(float cosine,
    float thickness_nm, float incident_ior, float film_ior, const OpenPbrVec3& substrate_ior,
    const OpenPbrVec3& generalized_reflectance, bool generalized_schlick) {
    const float eta1 = openpbr_max(incident_ior, 1e-4f);
    const float eta2 = openpbr_max(film_ior, 1e-4f);
    const float cos_theta = openpbr_saturate(fabsf(cosine));
    const float cos_theta_t = openpbr_thin_film_transmitted_cosine(cos_theta, eta1, eta2);
    if (cos_theta_t <= 0.0f) {
        return openpbr_make_vec3(1.0f);
    }

    const OpenPbrPolarizedReflectance r12 =
        openpbr_fresnel_dielectric_polarized(cos_theta, eta2 / eta1);
    const OpenPbrPolarizedReflectance t121 {1.0f - r12.p, 1.0f - r12.s};
    OpenPbrVec3 r23p {};
    OpenPbrVec3 r23s {};
    OpenPbrVec3 phi23p {};
    OpenPbrVec3 phi23s {};
    for (int channel = 0; channel < 3; ++channel) {
        const float eta3 = channel == 0 ? substrate_ior.x
                           : channel == 1 ? substrate_ior.y
                                          : substrate_ior.z;
        const float generalized = channel == 0 ? generalized_reflectance.x
                                  : channel == 1 ? generalized_reflectance.y
                                                 : generalized_reflectance.z;
        if (generalized_schlick) {
            const float reflectance = 0.5f * openpbr_saturate(generalized);
            if (channel == 0) {
                r23p.x = reflectance;
                r23s.x = reflectance;
                phi23p.x = eta3 < eta2 ? kOpenPbrPi : 0.0f;
                phi23s.x = phi23p.x;
            } else if (channel == 1) {
                r23p.y = reflectance;
                r23s.y = reflectance;
                phi23p.y = eta3 < eta2 ? kOpenPbrPi : 0.0f;
                phi23s.y = phi23p.y;
            } else {
                r23p.z = reflectance;
                r23s.z = reflectance;
                phi23p.z = eta3 < eta2 ? kOpenPbrPi : 0.0f;
                phi23s.z = phi23p.z;
            }
        } else {
            const OpenPbrPolarizedReflectance reflectance =
                openpbr_fresnel_real_ior_polarized(cos_theta_t, eta3 / eta2);
            const OpenPbrPolarizedReflectance phase =
                openpbr_real_ior_reflection_phase(cos_theta_t, eta2, eta3);
            if (channel == 0) {
                r23p.x = reflectance.p;
                r23s.x = reflectance.s;
                phi23p.x = phase.p;
                phi23s.x = phase.s;
            } else if (channel == 1) {
                r23p.y = reflectance.p;
                r23s.y = reflectance.s;
                phi23p.y = phase.p;
                phi23s.y = phase.s;
            } else {
                r23p.z = reflectance.p;
                r23s.z = reflectance.s;
                phi23p.z = phase.p;
                phi23s.z = phase.s;
            }
        }
    }

    const float brewster_cosine = eta1 / sqrtf(eta1 * eta1 + eta2 * eta2);
    const OpenPbrPolarizedReflectance phi21 {
        cos_theta < brewster_cosine ? 0.0f : kOpenPbrPi, kOpenPbrPi};
    const OpenPbrVec3 r123p {sqrtf(openpbr_max(r12.p * r23p.x, 0.0f)),
        sqrtf(openpbr_max(r12.p * r23p.y, 0.0f)),
        sqrtf(openpbr_max(r12.p * r23p.z, 0.0f))};
    const OpenPbrVec3 r123s {sqrtf(openpbr_max(r12.s * r23s.x, 0.0f)),
        sqrtf(openpbr_max(r12.s * r23s.y, 0.0f)),
        sqrtf(openpbr_max(r12.s * r23s.z, 0.0f))};
    const float opd = 2.0f * eta2 * cos_theta_t * thickness_nm * 1e-9f;

    const OpenPbrVec3 rs {
        t121.p * t121.p * r23p.x / openpbr_max(1.0f - r12.p * r23p.x, 1e-6f),
        t121.p * t121.p * r23p.y / openpbr_max(1.0f - r12.p * r23p.y, 1e-6f),
        t121.p * t121.p * r23p.z / openpbr_max(1.0f - r12.p * r23p.z, 1e-6f),
    };
    OpenPbrVec3 result = openpbr_add(openpbr_make_vec3(r12.p), rs);
    OpenPbrVec3 coefficient = openpbr_sub(rs, openpbr_make_vec3(t121.p));
    for (int order = 1; order <= 2; ++order) {
        coefficient = openpbr_mul(coefficient, r123p);
        const OpenPbrVec3 shift = openpbr_mul(
            openpbr_add(phi23p, openpbr_make_vec3(phi21.p)), static_cast<float>(order));
        result = openpbr_add(result,
            openpbr_mul(coefficient,
                openpbr_mul(openpbr_thin_film_sensitivity(static_cast<float>(order) * opd, shift),
                    2.0f)));
    }

    const OpenPbrVec3 rp {
        t121.s * t121.s * r23s.x / openpbr_max(1.0f - r12.s * r23s.x, 1e-6f),
        t121.s * t121.s * r23s.y / openpbr_max(1.0f - r12.s * r23s.y, 1e-6f),
        t121.s * t121.s * r23s.z / openpbr_max(1.0f - r12.s * r23s.z, 1e-6f),
    };
    result = openpbr_add(result, openpbr_add(openpbr_make_vec3(r12.s), rp));
    coefficient = openpbr_sub(rp, openpbr_make_vec3(t121.s));
    for (int order = 1; order <= 2; ++order) {
        coefficient = openpbr_mul(coefficient, r123s);
        const OpenPbrVec3 shift = openpbr_mul(
            openpbr_add(phi23s, openpbr_make_vec3(phi21.s)), static_cast<float>(order));
        result = openpbr_add(result,
            openpbr_mul(coefficient,
                openpbr_mul(openpbr_thin_film_sensitivity(static_cast<float>(order) * opd, shift),
                    2.0f)));
    }

    return openpbr_clamp_unit(openpbr_xyz_to_rgb(openpbr_mul(result, 0.5f)));
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

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_dispersion_enabled(
    const OpenPbrCoreMaterial& material) {
    return material.transmission_dispersion_scale > 0.0f
           && openpbr_saturate(material.base_metalness) < 1.0f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_dispersion_adjusted_ior(float original_ior,
    float dispersion_scale, float abbe_number, float wavelength_nm) {
    const float scale = openpbr_saturate(dispersion_scale);
    if (scale <= 0.0f || fabsf(original_ior - 1.0f) <= 1e-8f) {
        return original_ior;
    }

    const bool reciprocal = original_ior < 1.0f;
    const float ior = reciprocal ? 1.0f / openpbr_max(original_ior, 1e-6f) : original_ior;
    const float effective_abbe = openpbr_max(abbe_number, 1e-4f) / scale;
    const float inverse_f2 = 1.0f / openpbr_square(kOpenPbrFraunhoferFnm);
    const float inverse_c2 = 1.0f / openpbr_square(kOpenPbrFraunhoferCnm);
    const float b = (ior - 1.0f) / (effective_abbe * (inverse_f2 - inverse_c2));
    const float a = ior - b / openpbr_square(kOpenPbrFraunhoferDnm);
    const float adjusted = a + b / openpbr_square(openpbr_max(wavelength_nm, 1e-4f));
    return reciprocal ? 1.0f / openpbr_max(adjusted, 1e-6f) : adjusted;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_dispersion_ior(
    const OpenPbrCoreMaterial& material, const OpenPbrTransportContext& context) {
    const float ior = openpbr_modulated_ior(material);
    if (!openpbr_dispersion_enabled(material)) {
        return openpbr_make_vec3(ior);
    }
    return {openpbr_dispersion_adjusted_ior(ior, material.transmission_dispersion_scale,
                material.transmission_dispersion_abbe_number, context.rgb_wavelengths_nm.x),
        openpbr_dispersion_adjusted_ior(ior, material.transmission_dispersion_scale,
            material.transmission_dispersion_abbe_number, context.rgb_wavelengths_nm.y),
        openpbr_dispersion_adjusted_ior(ior, material.transmission_dispersion_scale,
            material.transmission_dispersion_abbe_number, context.rgb_wavelengths_nm.z)};
}

struct OpenPbrRoughness {
    float alpha_x = 0.09f;
    float alpha_y = 0.09f;
};

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrRoughness openpbr_roughness(float roughness_value,
    float anisotropy_value) {
    const float roughness = openpbr_saturate(roughness_value);
    const float anisotropy = openpbr_saturate(anisotropy_value);
    const float aspect = 1.0f - anisotropy;
    const float scale = sqrtf(2.0f / openpbr_max(aspect * aspect + 1.0f, 1e-8f));
    const float alpha_x = openpbr_max(roughness * roughness * scale, 1e-4f);
    return {alpha_x, openpbr_max(aspect * alpha_x, 1e-4f)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_effective_specular_roughness(
    const OpenPbrCoreMaterial& material) {
    const float specular = openpbr_saturate(material.specular_roughness);
    const float coat = openpbr_saturate(material.coat_roughness);
    const float specular2 = specular * specular;
    const float coat2 = coat * coat;
    const float affected = sqrtf(sqrtf(openpbr_min(
        1.0f, specular2 * specular2 + 2.0f * coat2 * coat2)));
    const float weight = openpbr_saturate(material.coat_weight);
    return specular + (affected - specular) * weight;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrRoughness openpbr_roughness(
    const OpenPbrCoreMaterial& material) {
    return openpbr_roughness(openpbr_effective_specular_roughness(material),
        material.specular_roughness_anisotropy);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrRoughness openpbr_coat_roughness(
    const OpenPbrCoreMaterial& material) {
    return openpbr_roughness(material.coat_roughness, material.coat_roughness_anisotropy);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_effectively_smooth(
    const OpenPbrRoughness& roughness) {
    return openpbr_max(roughness.alpha_x, roughness.alpha_y) < 1e-3f;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE bool openpbr_effectively_smooth(
    const OpenPbrCoreMaterial& material) {
    return openpbr_effectively_smooth(openpbr_roughness(material));
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

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_coat_fresnel(
    const OpenPbrCoreMaterial& material, float cosine) {
    return openpbr_fresnel_dielectric(cosine, openpbr_max(material.coat_ior, 1e-4f));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_coat_directional_albedo(
    const OpenPbrCoreMaterial& material, float cosine) {
    const OpenPbrRoughness roughness = openpbr_coat_roughness(material);
    if (openpbr_effectively_smooth(roughness)) {
        return openpbr_coat_fresnel(material, cosine);
    }
    const float alpha = openpbr_average_alpha(roughness);
    const float color0 = openpbr_coat_fresnel(material, 1.0f);
    const float single_scatter = openpbr_ggx_directional_albedo(cosine, alpha, color0, 1.0f);
    const float white_single_scatter =
        openpbr_ggx_directional_albedo(cosine, alpha, 1.0f, 1.0f);
    const float compensation = 1.0f
                               + openpbr_coat_fresnel(material, cosine)
                                     * (1.0f - white_single_scatter)
                                     / openpbr_max(white_single_scatter, 1e-4f);
    return openpbr_saturate(single_scatter * compensation);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_coat_passage(
    const OpenPbrCoreMaterial& material, float cosine) {
    const float presence = openpbr_saturate(material.coat_weight);
    const OpenPbrVec3 tint = openpbr_clamp_unit(material.coat_color);
    if (presence <= 0.0f || cosine <= 0.0f
        || (tint.x >= 1.0f && tint.y >= 1.0f && tint.z >= 1.0f)) {
        return openpbr_make_vec3(1.0f);
    }
    const float eta = openpbr_max(material.coat_ior, 1e-4f);
    const float sin2_transmitted = openpbr_max(0.0f, 1.0f - cosine * cosine) / (eta * eta);
    const float cos_transmitted = openpbr_safe_sqrt(1.0f - openpbr_min(sin2_transmitted, 1.0f));
    const float distance_scale = 1.0f / openpbr_max(cos_transmitted, 1e-4f);
    return openpbr_lerp(openpbr_make_vec3(1.0f),
        openpbr_pow(openpbr_sqrt(tint), distance_scale), presence);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_coat_darkening(
    const OpenPbrCoreMaterial& material) {
    const float presence = openpbr_saturate(material.coat_weight)
                           * openpbr_saturate(material.coat_darkening);
    if (presence <= 0.0f) {
        return openpbr_make_vec3(1.0f);
    }
    const float eta = openpbr_max(material.coat_ior, 1e-4f);
    const float f0 = openpbr_coat_fresnel(material, 1.0f);
    const float internal_reflection =
        openpbr_saturate(1.0f - (1.0f - f0) / openpbr_max(eta * eta, 1e-6f));
    const OpenPbrVec3 dielectric_albedo = openpbr_mul(openpbr_clamp_unit(material.base_color),
        openpbr_saturate(material.base_weight));
    const OpenPbrVec3 metal_albedo =
        openpbr_mul(dielectric_albedo, openpbr_saturate(material.specular_weight));
    const OpenPbrVec3 base_albedo = openpbr_lerp(dielectric_albedo, metal_albedo,
        openpbr_saturate(material.base_metalness));
    const float numerator = 1.0f - internal_reflection;
    const OpenPbrVec3 darkening {
        numerator / openpbr_max(1.0f - base_albedo.x * internal_reflection, 1e-5f),
        numerator / openpbr_max(1.0f - base_albedo.y * internal_reflection, 1e-5f),
        numerator / openpbr_max(1.0f - base_albedo.z * internal_reflection, 1e-5f),
    };
    return openpbr_lerp(openpbr_make_vec3(1.0f), openpbr_clamp_unit(darkening), presence);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_fuzz_directional_albedo(float cosine,
    float roughness_value) {
    const float x = openpbr_saturate(cosine);
    const float y = openpbr_clamp(roughness_value, 0.01f, 1.0f);
    const float s = y * (0.0206607f + 1.58491f * y)
                    / (0.0379424f + y * (1.32227f + y));
    const float y2 = y * y;
    const float m = y * (-0.193854f + y * (-1.14885f + y * (1.7932f - 0.95943f * y2)))
                    / (0.046391f + y);
    const float o = y * (0.000654023f + (-0.0207818f + 0.119681f * y) * y)
                    / (1.26264f + y * (-1.92021f + y));
    const float normalized = (x - m) / openpbr_max(s, 1e-5f);
    const float gaussian = expf(-0.5f * normalized * normalized)
                           / (openpbr_max(s, 1e-5f) * sqrtf(2.0f * kOpenPbrPi));
    return openpbr_saturate(gaussian + o);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrFrame openpbr_fuzz_frame(const OpenPbrVec3& wo) {
    const OpenPbrVec3 projected {wo.x, wo.y, 0.0f};
    const OpenPbrVec3 tangent = openpbr_length_squared(projected) > 1e-12f
                                    ? openpbr_normalize(projected)
                                    : OpenPbrVec3 {1.0f, 0.0f, 0.0f};
    return {tangent, {-tangent.y, tangent.x, 0.0f}, {0.0f, 0.0f, 1.0f}};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_fuzz_ltc_coefficients(float cosine,
    float roughness_value) {
    const float x = openpbr_saturate(cosine);
    const float y = openpbr_clamp(roughness_value, 0.01f, 1.0f);
    const float a_inverse = (2.58126f * x + 0.813703f * y) * y
                            / (1.0f + 0.310327f * x * x + 2.60994f * x * y);
    const float b_inverse = openpbr_safe_sqrt(1.0f - x) * (y - 1.0f) * y * y * y
                            / (0.0000254053f + 1.71228f * x - 1.71506f * x * y
                                + 1.34174f * y * y);
    return {openpbr_max(a_inverse, 1e-5f), b_inverse,
        openpbr_fuzz_directional_albedo(x, y)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_fuzz_pdf(const OpenPbrCoreMaterial& material,
    const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    if (wo.z <= 0.0f || wi.z <= 0.0f) {
        return 0.0f;
    }
    const OpenPbrFrame frame = openpbr_fuzz_frame(wo);
    const OpenPbrVec3 aligned = openpbr_to_local(frame, wi);
    const OpenPbrVec3 coefficients =
        openpbr_fuzz_ltc_coefficients(wo.z, material.fuzz_roughness);
    const OpenPbrVec3 transformed {coefficients.x * aligned.x + coefficients.y * aligned.z,
        coefficients.x * aligned.y, aligned.z};
    const float length_squared = openpbr_length_squared(transformed);
    if (length_squared <= 1e-16f || transformed.z <= 0.0f) {
        return 0.0f;
    }
    const float jacobian_scale = coefficients.x / length_squared;
    return transformed.z * kOpenPbrInvPi * jacobian_scale * jacobian_scale;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sample_fuzz(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo, float u1, float u2) {
    const float radius = sqrtf(openpbr_saturate(u1));
    const float phi = 2.0f * kOpenPbrPi * openpbr_saturate(u2);
    const OpenPbrVec3 cosine_sample {radius * cosf(phi), radius * sinf(phi),
        openpbr_safe_sqrt(1.0f - radius * radius)};
    const OpenPbrVec3 coefficients =
        openpbr_fuzz_ltc_coefficients(wo.z, material.fuzz_roughness);
    const float inverse_a = 1.0f / coefficients.x;
    const OpenPbrVec3 aligned = openpbr_normalize(
        {cosine_sample.x * inverse_a - cosine_sample.z * coefficients.y * inverse_a,
            cosine_sample.y * inverse_a, cosine_sample.z});
    return openpbr_normalize(openpbr_to_world(openpbr_fuzz_frame(wo), aligned));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_fuzz_value(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    const float presence = openpbr_saturate(material.fuzz_weight);
    if (presence <= 0.0f || wo.z <= 0.0f || wi.z <= 0.0f) {
        return {};
    }
    const float reflected = openpbr_fuzz_directional_albedo(wo.z, material.fuzz_roughness);
    const float density = openpbr_fuzz_pdf(material, wo, wi);
    return openpbr_mul(openpbr_clamp_unit(material.fuzz_color),
        presence * reflected * density / openpbr_max(wi.z, 1e-6f));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_fuzz_base_scale(
    const OpenPbrCoreMaterial& material, float outgoing_cosine, float incoming_cosine,
    bool exterior_layers) {
    if (!exterior_layers) {
        return 1.0f;
    }
    const float presence = openpbr_saturate(material.fuzz_weight);
    const float outgoing = 1.0f
                           - presence * openpbr_fuzz_directional_albedo(
                                            outgoing_cosine, material.fuzz_roughness);
    const float incoming = incoming_cosine > 0.0f
                               ? 1.0f
                                     - presence * openpbr_fuzz_directional_albedo(
                                                      incoming_cosine, material.fuzz_roughness)
                               : 1.0f;
    return openpbr_saturate(outgoing) * openpbr_saturate(incoming);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_coat_base_scale(
    const OpenPbrCoreMaterial& material, float outgoing_cosine, float incoming_cosine,
    bool exterior_layers) {
    const float presence = openpbr_saturate(material.coat_weight);
    if (presence <= 0.0f) {
        return openpbr_make_vec3(1.0f);
    }
    if (!exterior_layers) {
        return incoming_cosine < 0.0f ? openpbr_coat_passage(material, -incoming_cosine)
                                      : openpbr_make_vec3(1.0f);
    }
    const float reflected_out =
        presence * openpbr_coat_directional_albedo(material, outgoing_cosine);
    const OpenPbrVec3 outgoing = openpbr_mul(openpbr_coat_passage(material, outgoing_cosine),
        openpbr_saturate(1.0f - reflected_out));
    OpenPbrVec3 incoming = openpbr_make_vec3(1.0f);
    if (incoming_cosine > 0.0f) {
        const float reflected_in =
            presence * openpbr_coat_directional_albedo(material, incoming_cosine);
        incoming = openpbr_mul(openpbr_coat_passage(material, incoming_cosine),
            openpbr_saturate(1.0f - reflected_in));
    }
    return openpbr_mul(openpbr_mul(outgoing, incoming), openpbr_coat_darkening(material));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_substrate_scale(
    const OpenPbrCoreMaterial& material, float outgoing_cosine, float incoming_cosine,
    bool exterior_layers) {
    return openpbr_mul(openpbr_coat_base_scale(material, outgoing_cosine, incoming_cosine,
                           exterior_layers),
        openpbr_fuzz_base_scale(material, outgoing_cosine, incoming_cosine, exterior_layers));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_coat_reflection_value(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    const float presence = openpbr_saturate(material.coat_weight);
    if (presence <= 0.0f || wo.z <= 0.0f || wi.z <= 0.0f) {
        return {};
    }
    const OpenPbrRoughness roughness = openpbr_coat_roughness(material);
    if (openpbr_effectively_smooth(roughness)) {
        return {};
    }
    const OpenPbrVec3 sum = openpbr_add(wi, wo);
    if (openpbr_length_squared(sum) <= 1e-16f) {
        return {};
    }
    OpenPbrVec3 wm = openpbr_normalize(sum);
    if (wm.z < 0.0f) {
        wm = openpbr_negate(wm);
    }
    const float wo_dot_m = openpbr_dot(wo, wm);
    if (wo_dot_m <= 0.0f) {
        return {};
    }
    const float distribution = openpbr_ggx_d(roughness, wm);
    const float masking = openpbr_ggx_g(roughness, wo, wi);
    const float fresnel = openpbr_coat_fresnel(material, wo_dot_m);
    const float compensation = openpbr_ggx_energy_compensation(wo.z,
        openpbr_average_alpha(roughness), openpbr_make_vec3(fresnel)).x;
    return openpbr_make_vec3(presence * fresnel * compensation * distribution * masking
                             / openpbr_max(4.0f * wo.z * wi.z, 1e-8f));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_coat_reflection_pdf(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo, const OpenPbrVec3& wi) {
    if (wo.z <= 0.0f || wi.z <= 0.0f) {
        return 0.0f;
    }
    const OpenPbrRoughness roughness = openpbr_coat_roughness(material);
    if (openpbr_effectively_smooth(roughness)) {
        return 0.0f;
    }
    const OpenPbrVec3 sum = openpbr_add(wi, wo);
    if (openpbr_length_squared(sum) <= 1e-16f) {
        return 0.0f;
    }
    OpenPbrVec3 wm = openpbr_normalize(sum);
    if (wm.z < 0.0f) {
        wm = openpbr_negate(wm);
    }
    const float wo_dot_m = openpbr_dot(wo, wm);
    if (wo_dot_m <= 0.0f) {
        return 0.0f;
    }
    return openpbr_ggx_visible_normal_pdf(roughness, wo, wm)
           / openpbr_max(4.0f * fabsf(wo_dot_m), 1e-8f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_dielectric_reflectance(
    const OpenPbrCoreMaterial& material, float cosine, bool entering,
    const OpenPbrTransportContext& context);
RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_metal_reflectance(
    const OpenPbrCoreMaterial& material, float cosine);

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrLobeProbabilities openpbr_lobe_probabilities(
    const OpenPbrCoreMaterial& material, float outgoing_cosine = 1.0f,
    bool exterior_layers = true, const OpenPbrTransportContext& context = {}) {
    const float metalness = openpbr_saturate(material.base_metalness);
    const float transmission = openpbr_saturate(material.transmission_weight);
    const float base_weight = openpbr_saturate(material.base_weight);
    const float metal_specular_weight = openpbr_saturate(material.specular_weight);
    const float eta = openpbr_modulated_ior(material);
    const float eta_ratio = (eta - 1.0f) / (eta + 1.0f);
    const float dielectric_f0 = eta_ratio * eta_ratio;

    float dielectric_sampling_reflectance = dielectric_f0;
    float metal_sampling_reflectance =
        metal_specular_weight
        * openpbr_luminance(openpbr_mul(openpbr_clamp_unit(material.base_color), base_weight));
    if ((material.thin_film_weight > 0.0f && material.thin_film_thickness > 0.0f)
        || openpbr_dispersion_enabled(material)) {
        dielectric_sampling_reflectance = openpbr_luminance(
            openpbr_dielectric_reflectance(material, outgoing_cosine, true, context));
        metal_sampling_reflectance =
            openpbr_luminance(openpbr_metal_reflectance(material, outgoing_cosine));
    }

    const float opaque_transmission = (1.0f - metalness) * (1.0f - transmission)
                                      * openpbr_saturate(
                                          1.0f - dielectric_sampling_reflectance);
    const float subsurface = openpbr_saturate(material.subsurface_weight);
    float diffuse_score = 0.0f;
    float subsurface_score = 0.0f;
    if (subsurface <= 0.0f) {
        diffuse_score = opaque_transmission * base_weight
                        * openpbr_luminance(openpbr_clamp_unit(material.base_color));
    } else {
        const bool random_walk = openpbr_subsurface_random_walk_enabled(material);
        const bool thin_walled = openpbr_subsurface_thin_walled_enabled(material);
        const float local_subsurface = random_walk || thin_walled ? 0.0f : subsurface;
        const OpenPbrVec3 local_diffuse_color = openpbr_add(
            openpbr_mul(
                openpbr_clamp_unit(material.base_color), base_weight * (1.0f - subsurface)),
            openpbr_mul(openpbr_clamp_unit(material.subsurface_color), local_subsurface));
        diffuse_score = opaque_transmission * openpbr_luminance(local_diffuse_color);
        if (random_walk || thin_walled) {
            subsurface_score = opaque_transmission * subsurface
                               * openpbr_luminance(
                                   openpbr_clamp_unit(material.subsurface_color));
        }
    }
    const float dielectric_reflection_score =
        (1.0f - metalness) * dielectric_sampling_reflectance
        * openpbr_luminance(openpbr_clamp_unit(material.specular_color));
    const float metal_reflection_score = metalness * metal_sampling_reflectance;
    const float transmission_score =
        (1.0f - metalness) * transmission
        * openpbr_saturate(1.0f - dielectric_sampling_reflectance)
        * openpbr_luminance(openpbr_surface_transmission_color(material));

    const float coat_score = exterior_layers
                                 ? openpbr_saturate(material.coat_weight)
                                       * openpbr_coat_directional_albedo(material, outgoing_cosine)
                                 : 0.0f;
    const float fuzz_score = exterior_layers
                                 ? openpbr_saturate(material.fuzz_weight)
                                       * openpbr_fuzz_directional_albedo(
                                           outgoing_cosine, material.fuzz_roughness)
                                       * openpbr_luminance(openpbr_clamp_unit(material.fuzz_color))
                                 : 0.0f;
    const float substrate_score_scale =
        openpbr_saturate(1.0f - coat_score) * openpbr_saturate(1.0f - fuzz_score);
    const float scaled_diffuse_score = diffuse_score * substrate_score_scale;
    const float scaled_subsurface_score = subsurface_score * substrate_score_scale;
    const float reflection_score =
        (dielectric_reflection_score + metal_reflection_score) * substrate_score_scale;
    const float scaled_transmission_score = transmission_score * substrate_score_scale;
    const float total = scaled_diffuse_score + scaled_subsurface_score + reflection_score
                        + scaled_transmission_score + coat_score + fuzz_score;
    if (total <= 1e-10f) {
        return {};
    }
    return {scaled_diffuse_score / total, scaled_subsurface_score / total,
        reflection_score / total, scaled_transmission_score / total, coat_score / total,
        fuzz_score / total};
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
    const float subsurface = openpbr_saturate(material.subsurface_weight);
    float local_mix = openpbr_saturate(material.base_weight);
    OpenPbrVec3 color = openpbr_clamp_unit(material.base_color);
    if (subsurface > 0.0f) {
        const float base_mix = openpbr_saturate(material.base_weight) * (1.0f - subsurface);
        const float subsurface_mix = openpbr_subsurface_random_walk_enabled(material)
                                             || openpbr_subsurface_thin_walled_enabled(material)
                                         ? 0.0f
                                         : subsurface;
        local_mix = base_mix + subsurface_mix;
        color = openpbr_clamp_unit(openpbr_div(openpbr_add(
            openpbr_mul(openpbr_clamp_unit(material.base_color), base_mix),
            openpbr_mul(openpbr_clamp_unit(material.subsurface_color), subsurface_mix)),
            openpbr_max(local_mix, 1e-8f)));
    }
    const float coefficient = local_mix * (1.0f - metalness) * (1.0f - transmission);
    if (coefficient <= 0.0f) {
        return {};
    }

    constexpr float kFujiiConstant1 = 0.5f - 2.0f / (3.0f * kOpenPbrPi);
    const float sigma = openpbr_saturate(material.base_diffuse_roughness);
    const float coefficient_a = 1.0f / (1.0f + kFujiiConstant1 * sigma);
    const float s = openpbr_dot(wi, wo) - wi.z * wo.z;
    const float s_over_t = s > 0.0f ? s / openpbr_max(wi.z, wo.z) : s;
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

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_thin_walled_subsurface_value(
    const OpenPbrCoreMaterial& material, const OpenPbrVec3& wo,
    const OpenPbrVec3& wi_same_hemisphere, bool transmission) {
    if (!openpbr_subsurface_thin_walled_enabled(material)) {
        return {};
    }
    OpenPbrCoreMaterial diffuse = material;
    diffuse.base_weight = openpbr_saturate(material.subsurface_weight)
                          * openpbr_subsurface_thin_walled_probability(material, transmission);
    diffuse.base_color = openpbr_clamp_unit(material.subsurface_color);
    diffuse.subsurface_weight = 0.0f;
    return openpbr_diffuse_value(diffuse, wo, wi_same_hemisphere);
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

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_weighted_coat_ior(
    const OpenPbrCoreMaterial& material) {
    return 1.0f + openpbr_saturate(material.coat_weight)
                      * (openpbr_max(material.coat_ior, 1e-4f) - 1.0f);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_thin_wall_window_reflectance(
    const OpenPbrVec3& interface_reflectance) {
    const OpenPbrVec3 r = openpbr_clamp_unit(interface_reflectance);
    return {openpbr_saturate(r.x + openpbr_square(1.0f - r.x) * r.x
                                       / openpbr_max(1.0f - r.x * r.x, 1e-6f)),
        openpbr_saturate(r.y + openpbr_square(1.0f - r.y) * r.y
                                  / openpbr_max(1.0f - r.y * r.y, 1e-6f)),
        openpbr_saturate(r.z + openpbr_square(1.0f - r.z) * r.z
                                  / openpbr_max(1.0f - r.z * r.z, 1e-6f))};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_apply_thin_film(
    const OpenPbrCoreMaterial& material, float cosine, const OpenPbrVec3& base_reflectance,
    float incident_ior, const OpenPbrVec3& substrate_ior,
    const OpenPbrVec3& generalized_reflectance, bool generalized_schlick) {
    const float weight = openpbr_saturate(material.thin_film_weight);
    const float thickness_nm = openpbr_max(material.thin_film_thickness, 0.0f) * 1000.0f;
    if (weight <= 0.0f || thickness_nm <= 0.0f) {
        return base_reflectance;
    }
    OpenPbrVec3 film = openpbr_thin_film_airy(cosine, thickness_nm, incident_ior,
        material.thin_film_ior, substrate_ior, generalized_reflectance, generalized_schlick);
    if (material.geometry_thin_walled != 0 && !generalized_schlick) {
        film = openpbr_thin_wall_window_reflectance(film);
    }
    return openpbr_lerp(base_reflectance, film, weight);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_dielectric_reflectance(
    const OpenPbrCoreMaterial& material, float cosine, bool entering,
    const OpenPbrTransportContext& context) {
    if (!openpbr_dispersion_enabled(material)) {
        const float base_reflectance = openpbr_dielectric_fresnel(material, cosine, entering);
        const float base_ior = openpbr_modulated_ior(material);
        const float coat_ior = openpbr_weighted_coat_ior(material);
        const float incident_ior = entering ? coat_ior : base_ior;
        const float substrate_ior = entering ? base_ior : coat_ior;
        return openpbr_apply_thin_film(material, cosine,
            openpbr_make_vec3(base_reflectance), incident_ior,
            openpbr_make_vec3(substrate_ior), {}, false);
    }

    const OpenPbrVec3 base_iors = openpbr_dispersion_ior(material, context);
    const OpenPbrVec3 eta_path = entering ? base_iors : openpbr_reciprocal(base_iors);
    OpenPbrVec3 base_reflectance {
        openpbr_fresnel_dielectric(cosine, eta_path.x),
        openpbr_fresnel_dielectric(cosine, eta_path.y),
        openpbr_fresnel_dielectric(cosine, eta_path.z),
    };
    if (material.geometry_thin_walled != 0) {
        base_reflectance = openpbr_thin_wall_window_reflectance(base_reflectance);
    }

    const float coat_ior = openpbr_weighted_coat_ior(material);
    if (entering) {
        return openpbr_apply_thin_film(material, cosine, base_reflectance, coat_ior,
            base_iors, {}, false);
    }

    OpenPbrVec3 result {};
    for (int channel = 0; channel < 3; ++channel) {
        const OpenPbrVec3 film = openpbr_apply_thin_film(material, cosine, base_reflectance,
            openpbr_component(base_iors, channel), openpbr_make_vec3(coat_ior), {}, false);
        openpbr_set_component(result, channel, openpbr_component(film, channel));
    }
    return result;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_dielectric_reflectance(
    const OpenPbrCoreMaterial& material, float cosine, bool entering) {
    return openpbr_dielectric_reflectance(material, cosine, entering, {});
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_metal_reflectance(
    const OpenPbrCoreMaterial& material, float cosine) {
    const OpenPbrVec3 base_reflectance = openpbr_metal_fresnel(material, cosine);
    const float incident_ior = openpbr_weighted_coat_ior(material);
    const float cos_theta_t = openpbr_thin_film_transmitted_cosine(
        cosine, incident_ior, material.thin_film_ior);
    const OpenPbrVec3 reflectance_at_film = openpbr_metal_fresnel(material, cos_theta_t);
    const OpenPbrVec3 normal_reflectance = openpbr_metal_fresnel(material, 1.0f);
    const OpenPbrVec3 substrate_ior {
        incident_ior * openpbr_f0_to_ior(normal_reflectance.x),
        incident_ior * openpbr_f0_to_ior(normal_reflectance.y),
        incident_ior * openpbr_f0_to_ior(normal_reflectance.z),
    };
    return openpbr_apply_thin_film(material, cosine, base_reflectance, incident_ior,
        substrate_ior, reflectance_at_film, true);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_dielectric_directional_albedo_rgb(
    const OpenPbrCoreMaterial& material, float cosine, bool entering,
    const OpenPbrTransportContext& context = {}) {
    if ((material.thin_film_weight <= 0.0f || material.thin_film_thickness <= 0.0f)
        && !openpbr_dispersion_enabled(material)) {
        const OpenPbrRoughness roughness = openpbr_roughness(material);
        const float alpha = openpbr_average_alpha(roughness);
        if (openpbr_effectively_smooth(material)) {
            return openpbr_make_vec3(openpbr_dielectric_fresnel(material, cosine, entering));
        }
        const float color0 = openpbr_dielectric_fresnel(material, 1.0f, entering);
        if (color0 <= 1e-8f) {
            return {};
        }
        const float single_scatter =
            openpbr_ggx_directional_albedo(cosine, alpha, color0, 1.0f);
        const float white_single_scatter =
            openpbr_ggx_directional_albedo(cosine, alpha, 1.0f, 1.0f);
        const float directional_fresnel = openpbr_dielectric_fresnel(material, cosine, entering);
        const float compensation = 1.0f
                                   + directional_fresnel * (1.0f - white_single_scatter)
                                         / openpbr_max(white_single_scatter, 1e-4f);
        return openpbr_make_vec3(openpbr_saturate(single_scatter * compensation));
    }

    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const float alpha = openpbr_average_alpha(roughness);
    const OpenPbrVec3 mirror =
        openpbr_dielectric_reflectance(material, cosine, entering, context);
    if (openpbr_effectively_smooth(material)) {
        return mirror;
    }
    const OpenPbrVec3 color0 =
        openpbr_dielectric_reflectance(material, 1.0f, entering, context);
    const OpenPbrVec3 rough {
        openpbr_ggx_directional_albedo(cosine, alpha, color0.x, 1.0f),
        openpbr_ggx_directional_albedo(cosine, alpha, color0.y, 1.0f),
        openpbr_ggx_directional_albedo(cosine, alpha, color0.z, 1.0f),
    };
    return openpbr_lerp(mirror, rough, sqrtf(openpbr_saturate(alpha)));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float openpbr_dielectric_directional_albedo(
    const OpenPbrCoreMaterial& material, float cosine, bool entering,
    const OpenPbrTransportContext& context = {}) {
    return openpbr_luminance(
        openpbr_dielectric_directional_albedo_rgb(material, cosine, entering, context));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_reflection_fresnel(
    const OpenPbrCoreMaterial& material, float microfacet_cosine, float outgoing_cosine,
    bool entering, const OpenPbrTransportContext& context = {}) {
    const float metalness = openpbr_saturate(material.base_metalness);
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const float alpha = openpbr_average_alpha(roughness);

    const OpenPbrVec3 dielectric_f =
        openpbr_dielectric_reflectance(material, microfacet_cosine, entering, context);
    const OpenPbrVec3 dielectric_single = openpbr_mul(
        entering || material.geometry_thin_walled != 0 ? openpbr_clamp_unit(material.specular_color)
                                                       : openpbr_make_vec3(1.0f),
        dielectric_f);
    const OpenPbrVec3 dielectric_compensation =
        openpbr_ggx_energy_compensation(outgoing_cosine, alpha, dielectric_f);
    const OpenPbrVec3 dielectric = openpbr_mul(dielectric_single, dielectric_compensation);

    const OpenPbrVec3 metal_single = openpbr_metal_reflectance(material, microfacet_cosine);
    const OpenPbrVec3 metal_compensation =
        openpbr_ggx_energy_compensation(outgoing_cosine, alpha, metal_single);
    const OpenPbrVec3 metal = openpbr_mul(metal_single, metal_compensation);
    return openpbr_lerp(dielectric, metal, metalness);
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrEvaluation evaluate_openpbr_core(
    const OpenPbrCoreMaterial& material, OpenPbrFrame frame, const OpenPbrVec3& wo_world,
    const OpenPbrVec3& wi_world, const OpenPbrTransportContext& context = {}) {
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
    const bool exterior_layers = entering;
    const OpenPbrLobeProbabilities probabilities =
        openpbr_lobe_probabilities(material, wo.z, exterior_layers, context);
    const OpenPbrRoughness roughness = openpbr_roughness(material);
    const bool smooth = openpbr_effectively_smooth(roughness);
    const bool reflection = wi.z * wo.z > 0.0f;

    if (reflection) {
        const OpenPbrVec3 substrate_attenuation = openpbr_sub(openpbr_make_vec3(1.0f),
            openpbr_dielectric_directional_albedo_rgb(material, wo.z, entering, context));
        result.value = openpbr_add(openpbr_diffuse_value(material, wo, wi),
            openpbr_thin_walled_subsurface_value(material, wo, wi, false));
        result.value =
            openpbr_mul(result.value, openpbr_clamp_unit(substrate_attenuation));
        result.pdf = probabilities.diffuse * wi.z * kOpenPbrInvPi;
        if (openpbr_subsurface_thin_walled_enabled(material)) {
            result.pdf += probabilities.subsurface
                          * openpbr_subsurface_thin_walled_probability(material, false) * wi.z
                          * kOpenPbrInvPi;
        }

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
                    openpbr_reflection_fresnel(material, wo_dot_m, wo.z, entering, context);
                const OpenPbrVec3 specular = openpbr_mul(fresnel,
                    distribution * masking / openpbr_max(4.0f * wo.z * wi.z, 1e-8f));
                result.value = openpbr_add(result.value, specular);
                const float microfacet_pdf = openpbr_ggx_visible_normal_pdf(roughness, wo, wm)
                                             / openpbr_max(4.0f * fabsf(wo_dot_m), 1e-8f);
                result.pdf += probabilities.reflection * microfacet_pdf;
            }
        }
        result.value = openpbr_mul(result.value,
            openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));

        if (exterior_layers) {
            const OpenPbrVec3 coat = openpbr_mul(openpbr_coat_reflection_value(material, wo, wi),
                openpbr_fuzz_base_scale(material, wo.z, wi.z, true));
            result.value = openpbr_add(result.value, coat);
            result.value = openpbr_add(result.value, openpbr_fuzz_value(material, wo, wi));
            result.pdf += probabilities.coat * openpbr_coat_reflection_pdf(material, wo, wi);
            result.pdf += probabilities.fuzz * openpbr_fuzz_pdf(material, wo, wi);
        }
    } else if (openpbr_subsurface_thin_walled_enabled(material)
               && probabilities.subsurface > 0.0f) {
        const OpenPbrVec3 wi_reflected {wi.x, wi.y, -wi.z};
        const OpenPbrVec3 substrate_attenuation = openpbr_sub(openpbr_make_vec3(1.0f),
            openpbr_dielectric_directional_albedo_rgb(material, wo.z, true, context));
        result.value = openpbr_thin_walled_subsurface_value(
            material, wo, wi_reflected, true);
        result.value = openpbr_mul(result.value,
            openpbr_mul(openpbr_clamp_unit(substrate_attenuation),
                openpbr_substrate_scale(material, wo.z, wi.z, true)));
        result.pdf = probabilities.subsurface
                     * openpbr_subsurface_thin_walled_probability(material, true) * fabsf(wi.z)
                     * kOpenPbrInvPi;
    } else if (!smooth && material.geometry_thin_walled == 0
               && probabilities.transmission > 0.0f) {
        if (!openpbr_dispersion_enabled(material)) {
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
                    const OpenPbrVec3 reflectance =
                        openpbr_dielectric_reflectance(material, wo_dot_m, entering);
                    const float half_denominator = wi_dot_m + wo_dot_m / eta_path;
                    const float denominator_squared = half_denominator * half_denominator;
                    if (denominator_squared > 1e-16f) {
                        const float distribution = openpbr_ggx_d(roughness, wm);
                        const float masking = openpbr_ggx_g(roughness, wo, wi);
                        float transmission =
                            distribution * masking
                            * fabsf(wi_dot_m * wo_dot_m / (wi.z * wo.z * denominator_squared));
                        transmission /= eta_path * eta_path;
                        transmission *= (1.0f - openpbr_saturate(material.base_metalness))
                                        * openpbr_saturate(material.transmission_weight);
                        result.value = openpbr_mul(openpbr_mul(
                            openpbr_surface_transmission_color(material),
                            openpbr_clamp_unit(openpbr_sub(openpbr_make_vec3(1.0f), reflectance))),
                            transmission);

                        const float derivative = fabsf(wi_dot_m) / denominator_squared;
                        const float microfacet_pdf =
                            openpbr_ggx_visible_normal_pdf(roughness, wo, wm) * derivative;
                        result.pdf = probabilities.transmission * microfacet_pdf;
                    }
                }
            }
        } else {
            const OpenPbrVec3 iors = openpbr_dispersion_ior(material, context);
            const OpenPbrVec3 eta_paths = entering ? iors : openpbr_reciprocal(iors);
            const OpenPbrVec3 transmission_color = openpbr_surface_transmission_color(material);
            const float material_scale = (1.0f - openpbr_saturate(material.base_metalness))
                                         * openpbr_saturate(material.transmission_weight);
            OpenPbrVec3 conditional_pdfs {};
            for (int channel = 0; channel < 3; ++channel) {
                const float eta_path = openpbr_component(eta_paths, channel);
                const OpenPbrVec3 sum = openpbr_add(openpbr_mul(wi, eta_path), wo);
                if (openpbr_length_squared(sum) <= 1e-16f) {
                    continue;
                }
                OpenPbrVec3 wm = openpbr_normalize(sum);
                if (wm.z < 0.0f) {
                    wm = openpbr_negate(wm);
                }
                const float wi_dot_m = openpbr_dot(wi, wm);
                const float wo_dot_m = openpbr_dot(wo, wm);
                if (wi_dot_m * wi.z <= 0.0f || wo_dot_m * wo.z <= 0.0f) {
                    continue;
                }
                const float half_denominator = wi_dot_m + wo_dot_m / eta_path;
                const float denominator_squared = half_denominator * half_denominator;
                if (denominator_squared <= 1e-16f) {
                    continue;
                }
                const float distribution = openpbr_ggx_d(roughness, wm);
                const float masking = openpbr_ggx_g(roughness, wo, wi);
                float transmission = distribution * masking
                                     * fabsf(wi_dot_m * wo_dot_m
                                         / (wi.z * wo.z * denominator_squared));
                transmission *= material_scale / (eta_path * eta_path);
                const OpenPbrVec3 reflectance =
                    openpbr_dielectric_reflectance(material, wo_dot_m, entering, context);
                const float value = openpbr_component(transmission_color, channel)
                                    * openpbr_saturate(
                                        1.0f - openpbr_component(reflectance, channel))
                                    * transmission;
                openpbr_set_component(result.value, channel, value);

                const float derivative = fabsf(wi_dot_m) / denominator_squared;
                openpbr_set_component(conditional_pdfs, channel,
                    openpbr_ggx_visible_normal_pdf(roughness, wo, wm) * derivative);
            }
            result.pdf = probabilities.transmission
                         * openpbr_dot(conditional_pdfs,
                             openpbr_channel_probabilities(context));
        }
        result.value = openpbr_mul(result.value,
            openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));
    }

    result.value = openpbr_mul(result.value, opacity);
    result.pdf *= opacity;
    return result;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE float pdf_openpbr_core(const OpenPbrCoreMaterial& material,
    OpenPbrFrame frame, const OpenPbrVec3& wo, const OpenPbrVec3& wi,
    const OpenPbrTransportContext& context = {}) {
    return evaluate_openpbr_core(material, frame, wo, wi, context).pdf;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 emission_openpbr_core(
    const OpenPbrCoreMaterial& material) {
    OpenPbrVec3 result = openpbr_mul(openpbr_clamp_nonnegative(material.emission_color),
        openpbr_max(0.0f, material.emission_luminance)
            * openpbr_saturate(material.geometry_opacity));
    const float coat_transmission = 1.0f
                                    - openpbr_saturate(material.coat_weight)
                                          * openpbr_coat_directional_albedo(material, 1.0f);
    result = openpbr_mul(result, openpbr_mul(openpbr_coat_passage(material, 1.0f),
                                      openpbr_saturate(coat_transmission)));
    const float fuzz_transmission = 1.0f
                                    - openpbr_saturate(material.fuzz_weight)
                                          * openpbr_fuzz_directional_albedo(
                                              1.0f, material.fuzz_roughness);
    return openpbr_mul(result, openpbr_saturate(fuzz_transmission));
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrVec3 openpbr_sample_cosine_hemisphere(float u1, float u2) {
    const float radius = sqrtf(openpbr_saturate(u1));
    const float phi = 2.0f * kOpenPbrPi * openpbr_saturate(u2);
    return {radius * cosf(phi), radius * sinf(phi), openpbr_safe_sqrt(1.0f - radius * radius)};
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrCoreMaterial openpbr_subsurface_boundary_material(
    const OpenPbrCoreMaterial& material) {
    OpenPbrCoreMaterial boundary = material;
    boundary.base_weight = 0.0f;
    boundary.base_metalness = 0.0f;
    boundary.transmission_weight = 1.0f;
    boundary.transmission_color = openpbr_make_vec3(1.0f);
    boundary.transmission_depth = 0.0f;
    boundary.transmission_dispersion_scale = 0.0f;
    boundary.subsurface_weight = 0.0f;
    boundary.fuzz_weight = 0.0f;
    boundary.coat_weight = 0.0f;
    boundary.geometry_opacity = 1.0f;
    return boundary;
}

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrSample openpbr_sample_subsurface_boundary(
    const OpenPbrCoreMaterial& material, OpenPbrFrame frame, const OpenPbrVec3& wo_world,
    float u_lobe, float u1, float u2, const OpenPbrTransportContext& context = {}) {
    OpenPbrSample sample {};
    const OpenPbrVec3 wo_normalized = openpbr_normalize(wo_world);
    if (openpbr_length_squared(wo_normalized) <= 0.0f) {
        return sample;
    }
    const OpenPbrFrame authored_frame = frame;
    const bool entering = openpbr_dot(frame.normal, wo_normalized) >= 0.0f;
    frame = openpbr_face_frame(frame, wo_normalized);
    const OpenPbrVec3 wo = openpbr_to_local(frame, wo_normalized);
    if (wo.z <= 1e-7f) {
        return sample;
    }

    const OpenPbrLobeProbabilities probabilities =
        openpbr_lobe_probabilities(material, wo.z, false, context);
    const float choice = openpbr_clamp(u_lobe, 0.0f, 0.99999994f);
    OpenPbrVec3 wi {};
    if (choice < probabilities.reflection) {
        if (openpbr_effectively_smooth(material)) {
            wi = {-wo.x, -wo.y, wo.z};
            sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
            sample.discrete_pdf = probabilities.reflection;
            if (sample.discrete_pdf <= 0.0f) {
                return OpenPbrSample {};
            }
            sample.weight = openpbr_div(
                openpbr_reflection_fresnel(material, wo.z, wo.z, entering, context),
                sample.discrete_pdf);
            sample.event = OpenPbrScatterEvent::glossy_reflection;
            sample.delta = 1;
            sample.valid = 1;
            return sample;
        }
        const OpenPbrVec3 wm =
            openpbr_sample_visible_normal(openpbr_roughness(material), wo, u1, u2);
        wi = openpbr_reflect(wo, wm);
        if (wi.z <= 1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::glossy_reflection;
    } else {
        const float eta = openpbr_modulated_ior(material);
        const float material_eta_path = entering ? eta : 1.0f / eta;
        float eta_path = 1.0f;
        OpenPbrVec3 wm {0.0f, 0.0f, 1.0f};
        if (!openpbr_effectively_smooth(material)) {
            wm = openpbr_sample_visible_normal(openpbr_roughness(material), wo, u1, u2);
        }
        if (!openpbr_refract(wo, wm, material_eta_path, eta_path, wi)
            || (!openpbr_effectively_smooth(material) && wi.z >= -1e-7f)) {
            return sample;
        }
        if (openpbr_effectively_smooth(material)) {
            const OpenPbrVec3 transmission = openpbr_clamp_unit(openpbr_sub(
                openpbr_make_vec3(1.0f),
                openpbr_dielectric_reflectance(material, wo.z, entering, context)));
            sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
            sample.discrete_pdf = probabilities.transmission;
            if (sample.discrete_pdf <= 0.0f) {
                return OpenPbrSample {};
            }
            sample.weight = openpbr_div(openpbr_mul(transmission, 1.0f / (eta_path * eta_path)),
                sample.discrete_pdf);
            sample.event = OpenPbrScatterEvent::glossy_transmission;
            sample.delta = 1;
            sample.valid = 1;
            return sample;
        }
        sample.event = OpenPbrScatterEvent::glossy_transmission;
    }

    sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
    const OpenPbrEvaluation evaluation =
        evaluate_openpbr_core(material, authored_frame, wo_normalized, sample.wi, context);
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

RT_OPENPBR_HD RT_OPENPBR_INLINE OpenPbrSample sample_openpbr_core(
    const OpenPbrCoreMaterial& material, OpenPbrFrame frame, const OpenPbrVec3& wo_world,
    float u_lobe, float u1, float u2, const OpenPbrTransportContext& context = {}) {
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


    const float surface_choice =
        openpbr_clamp((u_lobe - passthrough_probability) / opacity, 0.0f, 0.99999994f);
    if (!entering && openpbr_subsurface_random_walk_enabled(material)) {
        const OpenPbrCoreMaterial boundary = openpbr_subsurface_boundary_material(material);
        sample = openpbr_sample_subsurface_boundary(
            boundary, authored_frame, wo_normalized, surface_choice, u1, u2, context);
        if (sample.valid == 0) {
            return sample;
        }
        const OpenPbrVec3 scale = openpbr_substrate_scale(material, wo.z,
            openpbr_dot(frame.normal, sample.wi), false);
        sample.value = openpbr_mul(sample.value, openpbr_mul(scale, opacity));
        sample.weight = openpbr_mul(sample.weight, scale);
        sample.pdf *= opacity;
        sample.discrete_pdf *= opacity;
        if (sample.event == OpenPbrScatterEvent::glossy_transmission) {
            sample.event = OpenPbrScatterEvent::subsurface_exit;
        }
        return sample;
    }

    const bool exterior_layers = entering;
    const OpenPbrLobeProbabilities probabilities =
        openpbr_lobe_probabilities(material, wo.z, exterior_layers, context);
    if (probabilities.diffuse + probabilities.subsurface + probabilities.reflection
            + probabilities.transmission + probabilities.coat + probabilities.fuzz
        <= 0.0f) {
        return sample;
    }
    OpenPbrVec3 wi {};
    const float fuzz_end = probabilities.fuzz;
    const float coat_end = fuzz_end + probabilities.coat;
    const float diffuse_end = coat_end + probabilities.diffuse;
    const float subsurface_end = diffuse_end + probabilities.subsurface;
    const float reflection_end = subsurface_end + probabilities.reflection;

    if (surface_choice < fuzz_end) {
        wi = openpbr_sample_fuzz(material, wo, u1, u2);
        if (wi.z <= 1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::fuzz_reflection;
    } else if (surface_choice < coat_end) {
        const OpenPbrRoughness coat_roughness = openpbr_coat_roughness(material);
        if (openpbr_effectively_smooth(coat_roughness)) {
            wi = {-wo.x, -wo.y, wo.z};
            OpenPbrVec3 coefficient = openpbr_make_vec3(openpbr_saturate(material.coat_weight)
                                                        * openpbr_coat_fresnel(material, wo.z)
                                                        * opacity);
            coefficient = openpbr_mul(coefficient,
                openpbr_fuzz_base_scale(material, wo.z, wi.z, exterior_layers));
            sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
            sample.discrete_pdf = opacity * probabilities.coat;
            if (sample.discrete_pdf <= 0.0f) {
                return OpenPbrSample {};
            }
            sample.weight = openpbr_div(coefficient, sample.discrete_pdf);
            sample.event = OpenPbrScatterEvent::coat_reflection;
            sample.delta = 1;
            sample.valid = 1;
            return sample;
        }
        const OpenPbrVec3 wm = openpbr_sample_visible_normal(coat_roughness, wo, u1, u2);
        wi = openpbr_reflect(wo, wm);
        if (wi.z <= 1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::coat_reflection;
    } else if (surface_choice < diffuse_end) {
        wi = openpbr_sample_cosine_hemisphere(u1, u2);
        sample.event = OpenPbrScatterEvent::diffuse_reflection;
    } else if (surface_choice < subsurface_end) {
        if (openpbr_subsurface_thin_walled_enabled(material)) {
            const float conditional_choice = openpbr_clamp(
                (surface_choice - diffuse_end) / openpbr_max(probabilities.subsurface, 1e-8f),
                0.0f, 0.99999994f);
            const float reflection_probability =
                openpbr_subsurface_thin_walled_probability(material, false);
            wi = openpbr_sample_cosine_hemisphere(u1, u2);
            if (conditional_choice < reflection_probability) {
                sample.event = OpenPbrScatterEvent::diffuse_reflection;
            } else {
                wi.z = -wi.z;
                sample.event = OpenPbrScatterEvent::thin_walled_transmission;
            }
        } else {
            const OpenPbrCoreMaterial boundary = openpbr_subsurface_boundary_material(material);
            const OpenPbrLobeProbabilities boundary_probabilities =
                openpbr_lobe_probabilities(boundary, wo.z, false, context);
            if (boundary_probabilities.transmission <= 0.0f
                || probabilities.subsurface <= 0.0f) {
                return sample;
            }
            const float conditional_choice = openpbr_clamp(
                (surface_choice - diffuse_end) / probabilities.subsurface, 0.0f, 0.99999994f);
            const float boundary_choice = boundary_probabilities.reflection
                                          + conditional_choice * boundary_probabilities.transmission;
            sample = openpbr_sample_subsurface_boundary(
                boundary, authored_frame, wo_normalized, boundary_choice, u1, u2, context);
            if (sample.valid == 0 || sample.event != OpenPbrScatterEvent::glossy_transmission) {
                return OpenPbrSample {};
            }
            const float material_scale = (1.0f - openpbr_saturate(material.base_metalness))
                                         * (1.0f - openpbr_saturate(material.transmission_weight))
                                         * openpbr_saturate(material.subsurface_weight);
            const OpenPbrVec3 layer_scale = openpbr_substrate_scale(material, wo.z,
                openpbr_dot(frame.normal, sample.wi), true);
            const OpenPbrVec3 weight_scale = openpbr_mul(layer_scale,
                material_scale * boundary_probabilities.transmission
                    / openpbr_max(probabilities.subsurface, 1e-8f));
            sample.value =
                openpbr_mul(sample.value, openpbr_mul(layer_scale, material_scale * opacity));
            sample.weight = openpbr_mul(sample.weight, weight_scale);
            sample.pdf *= opacity * probabilities.subsurface
                          / boundary_probabilities.transmission;
            sample.discrete_pdf *= opacity * probabilities.subsurface
                                   / boundary_probabilities.transmission;
            sample.event = OpenPbrScatterEvent::subsurface_entry;
            return sample;
        }
    } else if (surface_choice < reflection_end) {
        if (openpbr_effectively_smooth(material)) {
            wi = {-wo.x, -wo.y, wo.z};
            OpenPbrVec3 coefficient =
                openpbr_mul(
                    openpbr_reflection_fresnel(material, wo.z, wo.z, entering, context), opacity);
            coefficient = openpbr_mul(coefficient,
                openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));
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
        const OpenPbrVec3 transmission = openpbr_clamp_unit(openpbr_sub(openpbr_make_vec3(1.0f),
            openpbr_dielectric_reflectance(material, wo.z, true, context)));
        OpenPbrVec3 coefficient = openpbr_mul(openpbr_mul(
            openpbr_surface_transmission_color(material), transmission),
            opacity * (1.0f - openpbr_saturate(material.base_metalness))
                * openpbr_saturate(material.transmission_weight));
        coefficient = openpbr_mul(coefficient,
            openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));
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
        if (openpbr_dispersion_enabled(material)) {
            const float channel_u = openpbr_clamp(
                (surface_choice - reflection_end)
                    / openpbr_max(probabilities.transmission, 1e-8f),
                0.0f, 0.99999994f);
            const int channel = openpbr_sample_channel(context, channel_u);
            const OpenPbrVec3 iors = openpbr_dispersion_ior(material, context);
            const float material_eta_path = entering
                                                ? openpbr_component(iors, channel)
                                                : 1.0f / openpbr_component(iors, channel);
            float refracted_eta_path = 1.0f;
            if (!openpbr_refract(
                    wo, {0.0f, 0.0f, 1.0f}, material_eta_path, refracted_eta_path, wi)) {
                return sample;
            }
            const OpenPbrVec3 transmission = openpbr_clamp_unit(
                openpbr_sub(openpbr_make_vec3(1.0f),
                    openpbr_dielectric_reflectance(material, wo.z, entering, context)));
            const OpenPbrVec3 transmission_color = openpbr_surface_transmission_color(material);
            OpenPbrVec3 coefficient {};
            openpbr_set_component(coefficient, channel,
                openpbr_component(transmission_color, channel)
                    * openpbr_component(transmission, channel) * opacity
                    * (1.0f - openpbr_saturate(material.base_metalness))
                    * openpbr_saturate(material.transmission_weight)
                    / (refracted_eta_path * refracted_eta_path));
            coefficient = openpbr_mul(coefficient,
                openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));
            const float channel_probability = openpbr_component(
                openpbr_channel_probabilities(context), channel);
            sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
            sample.discrete_pdf =
                opacity * probabilities.transmission * channel_probability;
            if (sample.discrete_pdf <= 0.0f) {
                return OpenPbrSample {};
            }
            sample.weight = openpbr_div(coefficient, sample.discrete_pdf);
            sample.event = OpenPbrScatterEvent::glossy_transmission;
            sample.delta = 1;
            sample.valid = 1;
            return sample;
        }
        const float material_eta_path =
            entering ? openpbr_modulated_ior(material) : 1.0f / openpbr_modulated_ior(material);
        float refracted_eta_path = 1.0f;
        if (!openpbr_refract(wo, {0.0f, 0.0f, 1.0f}, material_eta_path, refracted_eta_path, wi)) {
            return sample;
        }
        const OpenPbrVec3 transmission = openpbr_clamp_unit(openpbr_sub(openpbr_make_vec3(1.0f),
            openpbr_dielectric_reflectance(material, wo.z, entering, context)));
        OpenPbrVec3 coefficient = openpbr_mul(openpbr_mul(
            openpbr_surface_transmission_color(material), transmission),
            opacity * (1.0f - openpbr_saturate(material.base_metalness))
                * openpbr_saturate(material.transmission_weight)
                / (refracted_eta_path * refracted_eta_path));
        coefficient = openpbr_mul(coefficient,
            openpbr_substrate_scale(material, wo.z, wi.z, exterior_layers));
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
        float material_eta_path =
            entering ? openpbr_modulated_ior(material) : 1.0f / openpbr_modulated_ior(material);
        if (openpbr_dispersion_enabled(material)) {
            const float channel_u = openpbr_clamp(
                (surface_choice - reflection_end)
                    / openpbr_max(probabilities.transmission, 1e-8f),
                0.0f, 0.99999994f);
            const int channel = openpbr_sample_channel(context, channel_u);
            const OpenPbrVec3 iors = openpbr_dispersion_ior(material, context);
            material_eta_path = entering ? openpbr_component(iors, channel)
                                         : 1.0f / openpbr_component(iors, channel);
        }
        float eta_path = 1.0f;
        if (!openpbr_refract(wo, wm, material_eta_path, eta_path, wi) || wi.z >= -1e-7f) {
            return sample;
        }
        sample.event = OpenPbrScatterEvent::glossy_transmission;
    }

    sample.wi = openpbr_normalize(openpbr_to_world(frame, wi));
    const OpenPbrEvaluation evaluation =
        evaluate_openpbr_core(material, authored_frame, wo_normalized, sample.wi, context);
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
