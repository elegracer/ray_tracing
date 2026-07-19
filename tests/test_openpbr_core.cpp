#include "common/openpbr_core.h"
#include "scene/openpbr_core_adapter.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr rt::OpenPbrVec3 kNormal {0.0f, 0.0f, 1.0f};
constexpr rt::OpenPbrVec3 kTangent {1.0f, 0.0f, 0.0f};

void expect_vec_near(const rt::OpenPbrVec3& actual, const rt::OpenPbrVec3& expected,
    double tolerance, const std::string& label) {
    expect_near(actual.x, expected.x, tolerance, label + ".x");
    expect_near(actual.y, expected.y, tolerance, label + ".y");
    expect_near(actual.z, expected.z, tolerance, label + ".z");
}

void expect_relative(double actual, double expected, double tolerance, const std::string& label) {
    expect_near(actual, expected, tolerance * std::max(1.0, std::abs(expected)), label);
}

void expect_vec_relative(const rt::OpenPbrVec3& actual, const rt::OpenPbrVec3& expected,
    double tolerance, const std::string& label) {
    expect_relative(actual.x, expected.x, tolerance, label + ".x");
    expect_relative(actual.y, expected.y, tolerance, label + ".y");
    expect_relative(actual.z, expected.z, tolerance, label + ".z");
}

void expect_finite_nonnegative(const rt::OpenPbrVec3& value, const std::string& label) {
    expect_true(std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z),
        label + " is finite");
    expect_true(value.x >= 0.0f && value.y >= 0.0f && value.z >= 0.0f, label + " is nonnegative");
}

rt::OpenPbrVec3 normalized(const rt::OpenPbrVec3& value) {
    return rt::openpbr_normalize(value);
}

rt::OpenPbrVec3 uniform_sphere(std::size_t index, std::size_t count) {
    const double u = (static_cast<double>(index) + 0.5) / static_cast<double>(count);
    const double v = std::fmod((static_cast<double>(index) + 0.5) * 0.6180339887498949, 1.0);
    const double z = 1.0 - 2.0 * u;
    const double radius = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = 2.0 * static_cast<double>(rt::kOpenPbrPi) * v;
    return {static_cast<float>(radius * std::cos(phi)), static_cast<float>(radius * std::sin(phi)),
        static_cast<float>(z)};
}

void test_scene_contract_mapping() {
    rt::scene::SceneOpenPbrSurface authored;
    authored.base_weight = 0.7;
    authored.base_color = Eigen::Vector3d {0.2, 0.4, 0.6};
    authored.base_diffuse_roughness = 0.35;
    authored.base_metalness = 0.25;
    authored.specular_weight = 0.8;
    authored.specular_color = Eigen::Vector3d {0.9, 0.8, 0.7};
    authored.specular_roughness = 0.18;
    authored.specular_ior = 1.42;
    authored.specular_roughness_anisotropy = 0.55;
    authored.transmission_weight = 0.3;
    authored.transmission_color = Eigen::Vector3d {0.8, 0.9, 1.0};
    authored.transmission_depth = 2.5;
    authored.emission_luminance = 12.0;
    authored.emission_color = Eigen::Vector3d {1.2, 0.6, 0.3};
    authored.geometry_opacity = 0.75;
    authored.geometry_thin_walled = true;

    const rt::OpenPbrCoreMaterial material =
        rt::scene::compile_openpbr_core_material(authored).parameters;
    expect_near(material.base_weight, 0.7, 1e-6, "base weight");
    expect_vec_near(material.base_color, {0.2f, 0.4f, 0.6f}, 1e-6, "base color");
    expect_near(material.base_diffuse_roughness, 0.35, 1e-6, "diffuse roughness");
    expect_near(material.base_metalness, 0.25, 1e-6, "metalness");
    expect_near(material.specular_weight, 0.8, 1e-6, "specular weight");
    expect_vec_near(material.specular_color, {0.9f, 0.8f, 0.7f}, 1e-6, "specular color");
    expect_near(material.specular_roughness, 0.18, 1e-6, "specular roughness");
    expect_near(material.specular_ior, 1.42, 1e-6, "specular IOR");
    expect_near(material.specular_roughness_anisotropy, 0.55, 1e-6, "anisotropy");
    expect_near(material.transmission_weight, 0.3, 1e-6, "transmission weight");
    expect_vec_near(material.transmission_color, {0.8f, 0.9f, 1.0f}, 1e-6, "transmission color");
    expect_near(material.transmission_depth, 2.5, 1e-6, "transmission depth");
    expect_near(material.emission_luminance, 12.0, 1e-6, "emission luminance");
    expect_vec_near(material.emission_color, {1.2f, 0.6f, 0.3f}, 1e-6, "emission color");
    expect_near(material.geometry_opacity, 0.75, 1e-6, "opacity");
    expect_true(material.geometry_thin_walled == 1, "thin-walled maps to the core flag");

    authored.connections.push_back(rt::scene::SceneMaterialConnection {
        .input_name = "base_color",
        .input_type = rt::scene::SceneMaterialValueType::color3,
        .texture_path = "/World/Textures/Color",
    });
    bool rejected_connection = false;
    try {
        (void)rt::scene::compile_openpbr_core_material(authored);
    } catch (const std::invalid_argument& ex) {
        rejected_connection =
            std::string {ex.what()}
            == "connected OpenPBR inputs require a SceneIR v2 texture compilation context";
    }
    expect_true(rejected_connection,
        "connected inputs require the SceneIR v2 texture compilation context");
}

rt::scene::SceneIRv2 connected_color_scene(std::vector<std::string> input_names,
    rt::scene::SceneColorSpace color_space) {
    rt::scene::SceneIRv2 scene;
    scene.add_prim(rt::scene::ScenePrim {.path = "/World"});
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Textures"});
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Textures/Color",
        .kind = rt::scene::ScenePrimKind::texture,
        .texture = rt::scene::SceneTexture {.color_space = color_space},
        .compatibility_source_index = 0,
    });
    scene.add_prim(rt::scene::ScenePrim {.path = "/World/Materials"});
    rt::scene::SceneOpenPbrSurface surface;
    for (std::string& input_name : input_names) {
        surface.connections.push_back(rt::scene::SceneMaterialConnection {
            .input_name = std::move(input_name),
            .input_type = rt::scene::SceneMaterialValueType::color3,
            .texture_path = "/World/Textures/Color",
            .channel = rt::scene::SceneTextureChannel::rgb,
        });
    }
    scene.add_prim(rt::scene::ScenePrim {
        .path = "/World/Materials/Surface",
        .kind = rt::scene::ScenePrimKind::material,
        .material = surface,
        .compatibility_source_index = 0,
    });
    return scene;
}

void test_color_connections_and_color_spaces() {
    rt::scene::SceneIRv2 scene = connected_color_scene(
        {"base_color", "specular_color", "transmission_color", "emission_color"},
        rt::scene::SceneColorSpace::srgb_texture);

    const auto table = rt::scene::compile_openpbr_core_material_table(scene, 1, 1);
    expect_true(table.size() == 1 && table[0].has_value(), "connected material table entry");
    const rt::OpenPbrColorTextureBindings& bindings = table[0]->color_textures;
    expect_true(bindings.base_color.texture_index == 0 && bindings.specular_color.texture_index == 0
                    && bindings.transmission_color.texture_index == 0
                    && bindings.emission_color.texture_index == 0,
        "supported color3 inputs bind the compatibility texture");
    expect_true(bindings.base_color.source_color_space == rt::OpenPbrSourceColorSpace::srgb_texture,
        "compiled binding retains source color space");

    const rt::OpenPbrVec3 raw =
        rt::openpbr_source_to_linear({0.04045f, 0.5f, 1.0f}, rt::OpenPbrSourceColorSpace::raw);
    const rt::OpenPbrVec3 linear = rt::openpbr_source_to_linear({0.04045f, 0.5f, 1.0f},
        rt::OpenPbrSourceColorSpace::linear_srgb);
    const rt::OpenPbrVec3 srgb = rt::openpbr_source_to_linear({0.04045f, 0.5f, 1.0f},
        rt::OpenPbrSourceColorSpace::srgb_texture);
    expect_vec_near(raw, {0.04045f, 0.5f, 1.0f}, 1e-7, "raw source bypass");
    expect_vec_near(linear, {0.04045f, 0.5f, 1.0f}, 1e-7, "linear source bypass");
    expect_vec_near(srgb, {0.0031308f, 0.21404114f, 1.0f}, 1e-6,
        "sRGB texture source decodes to linear sRGB");

    bool unsupported_input = false;
    try {
        (void)rt::scene::compile_openpbr_core_material_table(
            connected_color_scene({"coat_color"}, rt::scene::SceneColorSpace::linear_srgb), 1, 1);
    } catch (const std::invalid_argument& ex) {
        unsupported_input =
            std::string {ex.what()}.find("supports RGB connections only") != std::string::npos;
    }
    expect_true(unsupported_input, "unsupported active color connection fails explicitly");

    bool unsupported_color_space = false;
    try {
        (void)rt::scene::compile_openpbr_core_material_table(
            connected_color_scene({"base_color"}, rt::scene::SceneColorSpace::acescg), 1, 1);
    } catch (const std::invalid_argument& ex) {
        unsupported_color_space =
            std::string {ex.what()}
            == "OpenPBR production core does not yet support ACEScg source conversion";
    }
    expect_true(unsupported_color_space, "unsupported color conversion fails explicitly");
}

void test_frame_normal_and_tangent_semantics() {
    const rt::OpenPbrFrame frame =
        rt::make_openpbr_frame(normalized({0.2f, -0.1f, 1.0f}), normalized({1.0f, 0.3f, 0.2f}));
    expect_near(rt::openpbr_length_squared(frame.normal), 1.0, 1e-5, "normal length");
    expect_near(rt::openpbr_length_squared(frame.tangent), 1.0, 1e-5, "tangent length");
    expect_near(rt::openpbr_length_squared(frame.bitangent), 1.0, 1e-5, "bitangent length");
    expect_near(rt::openpbr_dot(frame.normal, frame.tangent), 0.0, 1e-5,
        "normal and tangent orthogonal");
    expect_near(rt::openpbr_dot(frame.normal, frame.bitangent), 0.0, 1e-5,
        "normal and bitangent orthogonal");
    expect_near(rt::openpbr_dot(frame.tangent, frame.bitangent), 0.0, 1e-5,
        "tangent and bitangent orthogonal");
    expect_vec_near(rt::openpbr_to_local(frame, frame.tangent), {1.0f, 0.0f, 0.0f}, 1e-5,
        "tangent local axis");
}

void test_evaluate_sample_pdf_contract() {
    rt::OpenPbrCoreMaterial material;
    material.base_color = {0.65f, 0.35f, 0.2f};
    material.base_diffuse_roughness = 0.45f;
    material.base_metalness = 0.2f;
    material.specular_color = {0.95f, 0.9f, 0.85f};
    material.specular_roughness = 0.32f;
    material.specular_roughness_anisotropy = 0.4f;
    material.transmission_weight = 0.25f;
    material.transmission_color = {0.8f, 0.9f, 1.0f};

    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.25f, -0.1f, 1.0f});
    int continuous_samples = 0;
    for (int index = 0; index < 2048; ++index) {
        const float u_lobe = (static_cast<float>(index) + 0.5f) / 2048.0f;
        const float u1 = std::fmod(0.754877666f * static_cast<float>(index + 1), 1.0f);
        const float u2 = std::fmod(0.569840296f * static_cast<float>(index + 1), 1.0f);
        const rt::OpenPbrSample sample =
            rt::sample_openpbr_core(material, frame, wo, u_lobe, u1, u2);
        if (sample.valid == 0 || sample.delta != 0) {
            continue;
        }
        const rt::OpenPbrEvaluation evaluation =
            rt::evaluate_openpbr_core(material, frame, wo, sample.wi);
        expect_relative(sample.pdf, evaluation.pdf, 5e-5, "sample/evaluate PDF agreement");
        expect_vec_relative(sample.value, evaluation.value, 5e-5,
            "sample/evaluate value agreement");
        const float abs_cosine = std::abs(rt::openpbr_dot(frame.normal, sample.wi));
        expect_vec_relative(sample.weight,
            rt::openpbr_mul(evaluation.value, abs_cosine / evaluation.pdf), 3e-5,
            "sample throughput weight");
        expect_finite_nonnegative(sample.value, "sample value");
        expect_true(std::isfinite(sample.pdf) && sample.pdf > 0.0f, "sample PDF is finite");
        ++continuous_samples;
    }
    expect_true(continuous_samples > 1800, "mixed material produces enough valid samples");
}

double integrate_pdf(const rt::OpenPbrCoreMaterial& material, const rt::OpenPbrFrame& frame,
    const rt::OpenPbrVec3& wo, std::size_t count) {
    double integral = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const rt::OpenPbrVec3 wi = uniform_sphere(index, count);
        integral += rt::pdf_openpbr_core(material, frame, wo, wi);
    }
    return integral * (4.0 * static_cast<double>(rt::kOpenPbrPi)) / static_cast<double>(count);
}

rt::OpenPbrVec3 integrate_furnace(const rt::OpenPbrCoreMaterial& material,
    const rt::OpenPbrFrame& frame, const rt::OpenPbrVec3& wo, std::size_t count) {
    rt::OpenPbrVec3 integral {};
    const double scale = 4.0 * static_cast<double>(rt::kOpenPbrPi) / static_cast<double>(count);
    for (std::size_t index = 0; index < count; ++index) {
        const rt::OpenPbrVec3 wi = uniform_sphere(index, count);
        const rt::OpenPbrEvaluation evaluation = rt::evaluate_openpbr_core(material, frame, wo, wi);
        const float cosine = std::abs(rt::openpbr_dot(frame.normal, wi));
        integral = rt::openpbr_add(integral,
            rt::openpbr_mul(evaluation.value, static_cast<float>(scale) * cosine));
    }
    return integral;
}

void test_pdf_normalization_and_furnace_energy() {
    constexpr std::size_t kIntegrationSamples = 131072;
    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.2f, 0.1f, 1.0f});

    rt::OpenPbrCoreMaterial diffuse;
    diffuse.base_color = {0.8f, 0.6f, 0.4f};
    diffuse.specular_weight = 0.0f;
    expect_near(integrate_pdf(diffuse, frame, wo, kIntegrationSamples), 1.0, 4e-3,
        "diffuse PDF normalization");

    rt::OpenPbrCoreMaterial mixed;
    mixed.base_color = {0.6f, 0.7f, 0.8f};
    mixed.base_diffuse_roughness = 0.5f;
    mixed.base_metalness = 0.15f;
    mixed.specular_roughness = 0.4f;
    mixed.specular_roughness_anisotropy = 0.5f;
    mixed.transmission_weight = 0.2f;
    mixed.transmission_color = {0.9f, 0.95f, 1.0f};
    const double mixed_pdf = integrate_pdf(mixed, frame, wo, kIntegrationSamples);
    expect_true(mixed_pdf > 0.96 && mixed_pdf < 1.02,
        "mixed continuous PDF integrates to approximately one");

    for (const rt::OpenPbrCoreMaterial& material : {diffuse, mixed}) {
        const rt::OpenPbrVec3 energy = integrate_furnace(material, frame, wo, kIntegrationSamples);
        expect_finite_nonnegative(energy, "furnace energy");
        expect_true(energy.x <= 1.02f && energy.y <= 1.02f && energy.z <= 1.02f,
            "layered core does not create energy in a unit furnace");
    }

    rt::OpenPbrCoreMaterial rough_white;
    rough_white.base_color = {1.0f, 1.0f, 1.0f};
    rough_white.base_diffuse_roughness = 1.0f;
    rough_white.specular_weight = 0.0f;
    const rt::OpenPbrVec3 rough_white_energy =
        integrate_furnace(rough_white, frame, wo, kIntegrationSamples);
    expect_vec_near(rough_white_energy, {1.0f, 1.0f, 1.0f}, 8e-3,
        "energy-compensated Oren-Nayar passes the white furnace");
}

void test_openpbr_reference_parameter_semantics() {
    rt::OpenPbrCoreMaterial metal;
    metal.base_weight = 0.8f;
    metal.base_color = {0.25f, 0.5f, 0.75f};
    metal.base_metalness = 1.0f;
    metal.specular_weight = 0.7f;
    metal.specular_color = {0.2f, 0.5f, 0.9f};

    constexpr float kEdgeCosine = 1.0f / 7.0f;
    const rt::OpenPbrVec3 color0 = rt::openpbr_mul(metal.base_color, metal.base_weight);
    const rt::OpenPbrVec3 schlick_at_edge =
        rt::openpbr_schlick(color0, {1.0f, 1.0f, 1.0f}, kEdgeCosine);
    expect_vec_near(rt::openpbr_metal_fresnel(metal, kEdgeCosine),
        rt::openpbr_mul(rt::openpbr_mul(schlick_at_edge, metal.specular_color),
            metal.specular_weight),
        2e-5, "metal specular color is the OpenPBR 82-degree tint");
    expect_vec_near(rt::openpbr_metal_fresnel(metal, 1.0f),
        rt::openpbr_mul(color0, metal.specular_weight), 1e-6,
        "metal normal-incidence color remains base color");
    expect_vec_near(rt::openpbr_metal_fresnel(metal, 0.0f),
        {metal.specular_weight, metal.specular_weight, metal.specular_weight}, 1e-6,
        "metal grazing reflectance remains white");

    rt::OpenPbrCoreMaterial volume;
    volume.transmission_weight = 1.0f;
    volume.transmission_color = {0.25f, 0.5f, 1.0f};
    volume.transmission_depth = 2.0f;
    expect_vec_near(rt::openpbr_surface_transmission_color(volume), {1.0f, 1.0f, 1.0f}, 1e-6,
        "positive depth moves transmission color into Beer absorption");
    expect_vec_near(rt::openpbr_transmission_at_distance(volume, volume.transmission_depth),
        volume.transmission_color, 1e-6, "transmission color is reached at authored depth");

    volume.geometry_thin_walled = 1;
    expect_vec_near(rt::openpbr_surface_transmission_color(volume), volume.transmission_color, 1e-6,
        "thin-walled transmission uses its color as a sheet tint");
    expect_vec_near(rt::openpbr_transmission_extinction(volume), {}, 1e-6,
        "thin-walled transmission has no interior extinction distance");

    rt::OpenPbrCoreMaterial dielectric;
    dielectric.specular_ior = 1.5f;
    expect_true(rt::openpbr_dielectric_fresnel(dielectric, 0.5f, false) > 0.999f,
        "bulk dielectric uses the reciprocal IOR and reaches total internal reflection when "
        "exiting");
    expect_true(rt::openpbr_dielectric_fresnel(dielectric, 0.5f, true) < 1.0f,
        "the same direction still refracts when entering");
}

void test_anisotropy_rotates_with_tangent() {
    rt::OpenPbrCoreMaterial material;
    material.base_weight = 1.0f;
    material.base_metalness = 1.0f;
    material.base_color = {0.9f, 0.7f, 0.3f};
    material.specular_roughness = 0.35f;
    material.specular_roughness_anisotropy = 0.8f;
    const rt::OpenPbrVec3 wo = normalized({0.0f, 0.0f, 1.0f});
    const rt::OpenPbrVec3 wi_x = normalized({0.55f, 0.0f, 1.0f});
    const rt::OpenPbrVec3 wi_y = normalized({0.0f, 0.55f, 1.0f});

    const rt::OpenPbrFrame frame_x = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrFrame frame_y = rt::make_openpbr_frame(kNormal, {0.0f, 1.0f, 0.0f});
    const float x_aligned = rt::evaluate_openpbr_core(material, frame_x, wo, wi_x).value.x;
    const float y_aligned = rt::evaluate_openpbr_core(material, frame_x, wo, wi_y).value.x;
    const float rotated_x = rt::evaluate_openpbr_core(material, frame_y, wo, wi_x).value.x;
    expect_true(std::abs(x_aligned - y_aligned) > 1e-4f,
        "anisotropy distinguishes tangent and bitangent directions: x=" + std::to_string(x_aligned)
            + " y=" + std::to_string(y_aligned));
    expect_near(rotated_x, y_aligned, 2e-5, "rotating the tangent rotates the anisotropic lobe");
}

void test_opacity_thin_walled_and_emission() {
    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.2f, 0.0f, 1.0f});

    rt::OpenPbrCoreMaterial cutout;
    cutout.geometry_opacity = 0.35f;
    const rt::OpenPbrSample passthrough =
        rt::sample_openpbr_core(cutout, frame, wo, 0.2f, 0.3f, 0.7f);
    expect_true(passthrough.valid != 0 && passthrough.delta != 0,
        "opacity creates a discrete passthrough event");
    expect_true(passthrough.event == rt::OpenPbrScatterEvent::opacity_passthrough,
        "opacity event kind");
    expect_vec_near(passthrough.wi, rt::openpbr_negate(wo), 1e-6,
        "opacity preserves the incident ray direction");
    expect_vec_near(passthrough.weight, {1.0f, 1.0f, 1.0f}, 1e-6, "opacity mixture is unbiased");
    expect_near(passthrough.discrete_pdf, 0.65, 1e-6, "opacity discrete probability");

    rt::OpenPbrCoreMaterial thin;
    thin.base_weight = 0.0f;
    thin.specular_roughness = 0.25f;
    thin.transmission_weight = 1.0f;
    thin.transmission_color = {0.8f, 0.9f, 1.0f};
    thin.geometry_thin_walled = 1;
    const rt::OpenPbrSample transmitted =
        rt::sample_openpbr_core(thin, frame, wo, 0.99f, 0.2f, 0.8f);
    expect_true(transmitted.valid != 0 && transmitted.delta != 0,
        "thin-walled transmission is a discrete straight-through event");
    expect_true(transmitted.event == rt::OpenPbrScatterEvent::thin_walled_transmission,
        "thin-walled event kind");
    expect_vec_near(transmitted.wi, rt::openpbr_negate(wo), 1e-6,
        "thin-walled transmission does not bend the ray");
    expect_finite_nonnegative(transmitted.weight, "thin-walled throughput");

    rt::OpenPbrCoreMaterial smooth_glass;
    smooth_glass.base_weight = 0.0f;
    smooth_glass.specular_roughness = 0.0f;
    smooth_glass.specular_ior = 1.5f;
    smooth_glass.transmission_weight = 1.0f;
    const rt::OpenPbrSample refracted =
        rt::sample_openpbr_core(smooth_glass, frame, wo, 0.9f, 0.1f, 0.2f);
    expect_true(refracted.valid != 0 && refracted.delta != 0,
        "zero-roughness dielectric uses a delta measure");
    expect_true(refracted.event == rt::OpenPbrScatterEvent::glossy_transmission,
        "smooth dielectric transmits through the Fresnel branch");
    expect_true(rt::openpbr_dot(frame.normal, refracted.wi) < 0.0f,
        "smooth dielectric transmission enters the opposite hemisphere");
    expect_finite_nonnegative(refracted.weight, "smooth dielectric throughput");

    rt::OpenPbrCoreMaterial smooth_metal;
    smooth_metal.base_metalness = 1.0f;
    smooth_metal.base_color = {0.9f, 0.7f, 0.2f};
    smooth_metal.specular_roughness = 0.0f;
    const rt::OpenPbrSample reflected =
        rt::sample_openpbr_core(smooth_metal, frame, wo, 0.5f, 0.3f, 0.4f);
    expect_true(reflected.valid != 0 && reflected.delta != 0,
        "zero-roughness metal uses a delta measure");
    expect_true(reflected.event == rt::OpenPbrScatterEvent::glossy_reflection,
        "smooth metal selects reflection");
    expect_true(rt::openpbr_dot(frame.normal, reflected.wi) > 0.0f,
        "smooth metal reflection stays in the incident hemisphere");
    expect_finite_nonnegative(reflected.weight, "smooth metal throughput");

    rt::OpenPbrCoreMaterial emissive;
    emissive.emission_luminance = 10.0f;
    emissive.emission_color = {1.2f, 0.5f, 0.25f};
    emissive.geometry_opacity = 0.4f;
    expect_vec_near(rt::emission_openpbr_core(emissive), {4.8f, 2.0f, 1.0f}, 1e-6,
        "HDR emission uses luminance and material opacity");
}

} // namespace

int main() {
    test_scene_contract_mapping();
    test_color_connections_and_color_spaces();
    test_frame_normal_and_tangent_semantics();
    test_evaluate_sample_pdf_contract();
    test_pdf_normalization_and_furnace_energy();
    test_openpbr_reference_parameter_semantics();
    test_anisotropy_rotates_with_tangent();
    test_opacity_thin_walled_and_emission();
    return 0;
}
