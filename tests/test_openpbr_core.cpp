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
    authored.transmission_dispersion_scale = 0.75;
    authored.transmission_dispersion_abbe_number = 32.0;
    authored.fuzz_weight = 0.45;
    authored.fuzz_color = Eigen::Vector3d {0.7, 0.8, 0.9};
    authored.fuzz_roughness = 0.6;
    authored.coat_weight = 0.55;
    authored.coat_color = Eigen::Vector3d {0.6, 0.8, 1.0};
    authored.coat_roughness = 0.2;
    authored.coat_roughness_anisotropy = 0.35;
    authored.coat_ior = 1.6;
    authored.coat_darkening = 0.7;
    authored.thin_film_weight = 0.65;
    authored.thin_film_thickness = 0.3;
    authored.thin_film_ior = 1.35;
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
    expect_near(material.transmission_dispersion_scale, 0.75, 1e-6, "dispersion scale");
    expect_near(material.transmission_dispersion_abbe_number, 32.0, 1e-6,
        "dispersion Abbe number");
    expect_near(material.fuzz_weight, 0.45, 1e-6, "fuzz weight");
    expect_vec_near(material.fuzz_color, {0.7f, 0.8f, 0.9f}, 1e-6, "fuzz color");
    expect_near(material.fuzz_roughness, 0.6, 1e-6, "fuzz roughness");
    expect_near(material.coat_weight, 0.55, 1e-6, "coat weight");
    expect_vec_near(material.coat_color, {0.6f, 0.8f, 1.0f}, 1e-6, "coat color");
    expect_near(material.coat_roughness, 0.2, 1e-6, "coat roughness");
    expect_near(material.coat_roughness_anisotropy, 0.35, 1e-6, "coat anisotropy");
    expect_near(material.coat_ior, 1.6, 1e-6, "coat IOR");
    expect_near(material.coat_darkening, 0.7, 1e-6, "coat darkening");
    expect_near(material.thin_film_weight, 0.65, 1e-6, "thin-film weight");
    expect_near(material.thin_film_thickness, 0.3, 1e-6, "thin-film thickness in micrometers");
    expect_near(material.thin_film_ior, 1.35, 1e-6, "thin-film IOR");
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
    material.coat_weight = 0.45f;
    material.coat_color = {0.7f, 0.85f, 1.0f};
    material.coat_roughness = 0.2f;
    material.coat_roughness_anisotropy = 0.3f;
    material.fuzz_weight = 0.35f;
    material.fuzz_color = {0.9f, 0.5f, 0.25f};
    material.fuzz_roughness = 0.55f;

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
    const rt::OpenPbrVec3& wo, std::size_t count,
    const rt::OpenPbrTransportContext& context = {}) {
    double integral = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const rt::OpenPbrVec3 wi = uniform_sphere(index, count);
        integral += rt::pdf_openpbr_core(material, frame, wo, wi, context);
    }
    return integral * (4.0 * static_cast<double>(rt::kOpenPbrPi)) / static_cast<double>(count);
}

rt::OpenPbrVec3 integrate_furnace(const rt::OpenPbrCoreMaterial& material,
    const rt::OpenPbrFrame& frame, const rt::OpenPbrVec3& wo, std::size_t count,
    const rt::OpenPbrTransportContext& context = {}) {
    rt::OpenPbrVec3 integral {};
    const double scale = 4.0 * static_cast<double>(rt::kOpenPbrPi) / static_cast<double>(count);
    for (std::size_t index = 0; index < count; ++index) {
        const rt::OpenPbrVec3 wi = uniform_sphere(index, count);
        const rt::OpenPbrEvaluation evaluation =
            rt::evaluate_openpbr_core(material, frame, wo, wi, context);
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

void test_coat_and_fuzz_reference_semantics() {
    constexpr std::size_t kIntegrationSamples = 131072;
    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.6614378f, 0.0f, 0.75f});

    expect_near(rt::openpbr_fuzz_directional_albedo(0.75f, 0.5f), 0.053199173129,
        2e-6, "MaterialX Zeltner directional-albedo fit");
    const rt::OpenPbrVec3 ltc = rt::openpbr_fuzz_ltc_coefficients(0.75f, 0.5f);
    expect_near(ltc.x, 0.544004842830, 2e-6, "MaterialX Zeltner LTC a inverse fit");
    expect_near(ltc.y, -0.032001297492, 2e-6, "MaterialX Zeltner LTC b inverse fit");

    rt::OpenPbrCoreMaterial coat_only;
    coat_only.base_weight = 0.0f;
    coat_only.specular_weight = 0.0f;
    coat_only.coat_weight = 1.0f;
    coat_only.coat_roughness = 0.0f;
    coat_only.coat_ior = 1.6f;
    const rt::OpenPbrSample coat_sample =
        rt::sample_openpbr_core(coat_only, frame, wo, 0.5f, 0.3f, 0.7f);
    expect_true(coat_sample.valid != 0 && coat_sample.delta != 0,
        "smooth coat is a discrete reflection");
    expect_true(coat_sample.event == rt::OpenPbrScatterEvent::coat_reflection,
        "smooth coat preserves its event identity");
    expect_near(coat_sample.discrete_pdf, 1.0, 1e-6, "pure coat selection probability");
    expect_vec_near(coat_sample.weight,
        rt::openpbr_make_vec3(rt::openpbr_coat_fresnel(coat_only, wo.z)), 2e-6,
        "pure smooth coat carries dielectric Fresnel throughput");

    rt::OpenPbrCoreMaterial fuzz_only;
    fuzz_only.base_weight = 0.0f;
    fuzz_only.specular_weight = 0.0f;
    fuzz_only.fuzz_weight = 1.0f;
    fuzz_only.fuzz_color = {0.8f, 0.4f, 0.2f};
    fuzz_only.fuzz_roughness = 0.5f;
    expect_near(integrate_pdf(fuzz_only, frame, wo, kIntegrationSamples), 1.0, 8e-3,
        "Zeltner fuzz PDF normalization");
    const rt::OpenPbrVec3 fuzz_energy =
        integrate_furnace(fuzz_only, frame, wo, kIntegrationSamples);
    const rt::OpenPbrVec3 expected_fuzz_energy = rt::openpbr_mul(fuzz_only.fuzz_color,
        rt::openpbr_fuzz_directional_albedo(wo.z, fuzz_only.fuzz_roughness));
    expect_vec_near(fuzz_energy, expected_fuzz_energy, 8e-3,
        "Zeltner fuzz integrates to its fitted directional albedo");

    int fuzz_samples = 0;
    for (int index = 0; index < 2048; ++index) {
        const float u1 = std::fmod(0.754877666f * static_cast<float>(index + 1), 1.0f);
        const float u2 = std::fmod(0.569840296f * static_cast<float>(index + 1), 1.0f);
        const rt::OpenPbrSample sample =
            rt::sample_openpbr_core(fuzz_only, frame, wo, 0.5f, u1, u2);
        expect_true(sample.valid != 0 && sample.delta == 0,
            "pure fuzz produces a continuous sample");
        expect_true(sample.event == rt::OpenPbrScatterEvent::fuzz_reflection,
            "pure fuzz preserves its event identity");
        const rt::OpenPbrEvaluation evaluation =
            rt::evaluate_openpbr_core(fuzz_only, frame, wo, sample.wi);
        expect_relative(sample.pdf, evaluation.pdf, 5e-5, "fuzz sample/evaluate PDF agreement");
        expect_vec_relative(sample.value, evaluation.value, 5e-5,
            "fuzz sample/evaluate value agreement");
        ++fuzz_samples;
    }
    expect_true(fuzz_samples == 2048, "all pure fuzz samples are valid");

    rt::OpenPbrCoreMaterial layered;
    layered.base_color = {1.0f, 1.0f, 1.0f};
    layered.base_diffuse_roughness = 0.6f;
    layered.specular_roughness = 0.35f;
    layered.coat_weight = 1.0f;
    layered.coat_roughness = 0.3f;
    layered.coat_ior = 1.6f;
    layered.fuzz_weight = 1.0f;
    layered.fuzz_roughness = 0.5f;
    const rt::OpenPbrVec3 layered_energy =
        integrate_furnace(layered, frame, wo, kIntegrationSamples);
    expect_finite_nonnegative(layered_energy, "coat/fuzz layered furnace energy");
    expect_true(layered_energy.x <= 1.02f && layered_energy.y <= 1.02f
                    && layered_energy.z <= 1.02f,
        "coat/fuzz layering does not create energy in a unit furnace");

    rt::OpenPbrCoreMaterial tinted_coat;
    tinted_coat.specular_weight = 0.0f;
    tinted_coat.base_color = {1.0f, 1.0f, 1.0f};
    tinted_coat.coat_weight = 1.0f;
    tinted_coat.coat_color = {0.25f, 0.64f, 1.0f};
    tinted_coat.coat_roughness = 0.35f;
    const rt::OpenPbrEvaluation tinted = rt::evaluate_openpbr_core(tinted_coat, frame, wo,
        normalized({-0.6f, 0.2f, 1.0f}));
    expect_true(tinted.value.x < tinted.value.y && tinted.value.y < tinted.value.z,
        "coat absorption tints the substrate in two passages");

    rt::OpenPbrCoreMaterial unchanged;
    unchanged.coat_color = {0.1f, 0.2f, 0.3f};
    unchanged.coat_roughness = 0.9f;
    unchanged.fuzz_color = {0.2f, 0.4f, 0.8f};
    unchanged.fuzz_roughness = 0.1f;
    unchanged.thin_film_thickness = 0.85f;
    unchanged.thin_film_ior = 2.1f;
    const rt::OpenPbrCoreMaterial defaults;
    const rt::OpenPbrVec3 wi = normalized({-0.3f, 0.2f, 1.0f});
    const rt::OpenPbrEvaluation unchanged_eval =
        rt::evaluate_openpbr_core(unchanged, frame, wo, wi);
    const rt::OpenPbrEvaluation default_eval =
        rt::evaluate_openpbr_core(defaults, frame, wo, wi);
    expect_vec_near(unchanged_eval.value, default_eval.value, 1e-7,
        "zero layer weights preserve the legacy default response");
    expect_near(unchanged_eval.pdf, default_eval.pdf, 1e-7,
        "zero layer weights preserve the legacy default PDF");
}

void test_thin_film_reference_semantics() {
    constexpr std::size_t kIntegrationSamples = 131072;
    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.6614378f, 0.0f, 0.75f});

    rt::OpenPbrCoreMaterial reference;
    reference.specular_ior = 1.5f;
    reference.thin_film_weight = 1.0f;
    reference.thin_film_thickness = 0.3f;
    reference.thin_film_ior = 1.4f;
    expect_vec_near(rt::openpbr_dielectric_reflectance(reference, 0.75f, true),
        {0.04098798f, 0.02764139f, 0.02773710f}, 2e-6,
        "MaterialX two-order Airy reference at 300 nm");
    reference.thin_film_thickness = 0.6f;
    expect_vec_near(rt::openpbr_dielectric_reflectance(reference, 0.75f, true),
        {0.01976027f, 0.03432216f, 0.03386349f}, 2e-6,
        "MaterialX two-order Airy reference at 600 nm");

    rt::OpenPbrCoreMaterial film;
    film.base_color = {0.9f, 0.9f, 0.9f};
    film.base_diffuse_roughness = 0.35f;
    film.specular_roughness = 0.28f;
    film.specular_ior = 1.5f;
    film.transmission_weight = 0.35f;
    film.thin_film_weight = 0.8f;
    film.thin_film_thickness = 0.45f;
    film.thin_film_ior = 1.4f;
    expect_near(integrate_pdf(film, frame, wo, kIntegrationSamples), 1.0, 9e-3,
        "thin-film lobe mixture PDF normalization");
    const rt::OpenPbrVec3 film_energy = integrate_furnace(film, frame, wo, kIntegrationSamples);
    expect_finite_nonnegative(film_energy, "thin-film layered furnace energy");
    expect_true(film_energy.x <= 1.02f && film_energy.y <= 1.02f && film_energy.z <= 1.02f,
        "non-absorbing thin film only redistributes bounded energy");

    int matched_samples = 0;
    for (int index = 0; index < 4096; ++index) {
        const float u_lobe = std::fmod(0.618033989f * static_cast<float>(index + 1), 1.0f);
        const float u1 = std::fmod(0.754877666f * static_cast<float>(index + 1), 1.0f);
        const float u2 = std::fmod(0.569840296f * static_cast<float>(index + 1), 1.0f);
        const rt::OpenPbrSample sample =
            rt::sample_openpbr_core(film, frame, wo, u_lobe, u1, u2);
        if (sample.valid == 0 || sample.delta != 0) {
            continue;
        }
        const rt::OpenPbrEvaluation evaluation =
            rt::evaluate_openpbr_core(film, frame, wo, sample.wi);
        expect_relative(sample.pdf, evaluation.pdf, 7e-5,
            "thin-film sample/evaluate PDF agreement");
        expect_vec_relative(sample.value, evaluation.value, 7e-5,
            "thin-film sample/evaluate value agreement");
        ++matched_samples;
    }
    expect_true(matched_samples > 3000, "thin-film sampling produces a stable continuous population");

    rt::OpenPbrCoreMaterial coat = film;
    coat.coat_weight = 0.8f;
    coat.coat_roughness = 0.22f;
    coat.coat_ior = 1.6f;
    const rt::OpenPbrVec3 wi = normalized({-0.3f, 0.2f, 1.0f});
    rt::OpenPbrCoreMaterial coat_without_film = coat;
    coat_without_film.thin_film_weight = 0.0f;
    expect_vec_near(rt::openpbr_coat_reflection_value(coat, wo, wi),
        rt::openpbr_coat_reflection_value(coat_without_film, wo, wi), 1e-7,
        "thin film modifies the base interface but not the separate coat lobe");
    expect_true(rt::openpbr_length_squared(rt::openpbr_sub(
                    rt::openpbr_dielectric_reflectance(coat, 0.75f, true),
                    rt::openpbr_dielectric_reflectance(coat_without_film, 0.75f, true)))
                    > 1e-6f,
        "coat/base IOR context still produces a visible thin-film response");
    const rt::OpenPbrVec3 coat_energy = integrate_furnace(coat, frame, wo, kIntegrationSamples);
    expect_finite_nonnegative(coat_energy, "coat plus thin-film furnace energy");
    expect_true(coat_energy.x <= 1.02f && coat_energy.y <= 1.02f && coat_energy.z <= 1.02f,
        "coat passage bounds the thin-film substrate");

    rt::OpenPbrCoreMaterial thin_wall;
    thin_wall.base_weight = 0.0f;
    thin_wall.specular_roughness = 0.0f;
    thin_wall.specular_ior = 1.5f;
    thin_wall.transmission_weight = 1.0f;
    thin_wall.geometry_thin_walled = 1;
    thin_wall.thin_film_weight = 1.0f;
    thin_wall.thin_film_thickness = 0.3f;
    thin_wall.thin_film_ior = 1.4f;
    const rt::OpenPbrLobeProbabilities probabilities =
        rt::openpbr_lobe_probabilities(thin_wall, wo.z, true);
    const rt::OpenPbrSample reflected = rt::sample_openpbr_core(thin_wall, frame, wo,
        0.5f * probabilities.reflection, 0.2f, 0.8f);
    const rt::OpenPbrSample transmitted = rt::sample_openpbr_core(thin_wall, frame, wo,
        probabilities.reflection + 0.5f * probabilities.transmission, 0.2f, 0.8f);
    expect_true(reflected.valid != 0 && reflected.event == rt::OpenPbrScatterEvent::glossy_reflection,
        "thin-film thin wall samples reflection");
    expect_true(transmitted.valid != 0
                    && transmitted.event == rt::OpenPbrScatterEvent::thin_walled_transmission,
        "thin-film thin wall samples straight-through transmission");
    const rt::OpenPbrVec3 resolved_reflection =
        rt::openpbr_mul(reflected.weight, reflected.discrete_pdf);
    const rt::OpenPbrVec3 resolved_transmission =
        rt::openpbr_mul(transmitted.weight, transmitted.discrete_pdf);
    expect_vec_near(rt::openpbr_add(resolved_reflection, resolved_transmission),
        rt::openpbr_make_vec3(1.0f), 3e-4,
        "thin-film thin wall conserves reflection plus transmission");
}

void test_dispersion_reference_semantics() {
    constexpr std::size_t kIntegrationSamples = 131072;
    const rt::OpenPbrFrame frame = rt::make_openpbr_frame(kNormal, kTangent);
    const rt::OpenPbrVec3 wo = normalized({0.6f, 0.0f, 0.8f});
    const rt::OpenPbrTransportContext context {
        .rgb_wavelengths_nm = {rt::kOpenPbrFraunhoferCnm, rt::kOpenPbrFraunhoferDnm,
            rt::kOpenPbrFraunhoferFnm},
        .path_throughput = {1.0f, 1.0f, 1.0f},
    };

    const float ior_c = rt::openpbr_dispersion_adjusted_ior(
        1.5f, 1.0f, 20.0f, rt::kOpenPbrFraunhoferCnm);
    const float ior_d = rt::openpbr_dispersion_adjusted_ior(
        1.5f, 1.0f, 20.0f, rt::kOpenPbrFraunhoferDnm);
    const float ior_f = rt::openpbr_dispersion_adjusted_ior(
        1.5f, 1.0f, 20.0f, rt::kOpenPbrFraunhoferFnm);
    expect_near(ior_c, 1.49248045, 2e-6, "Cauchy IOR at Fraunhofer C");
    expect_near(ior_d, 1.5, 2e-6, "Cauchy IOR preserves authored Fraunhofer d IOR");
    expect_near(ior_f, 1.51748045, 2e-6, "Cauchy IOR at Fraunhofer F");
    expect_near((ior_d - 1.0f) / (ior_f - ior_c), 20.0, 2e-4,
        "Cauchy coefficients reconstruct the authored Abbe number");
    expect_true(rt::openpbr_dispersion_adjusted_ior(1.5f, 0.0f, 9.0f, 450.0f) == 1.5f,
        "zero dispersion scale returns the exact authored IOR");

    rt::OpenPbrCoreMaterial smooth;
    smooth.base_weight = 0.0f;
    smooth.specular_roughness = 0.0f;
    smooth.specular_ior = 1.5f;
    smooth.transmission_weight = 1.0f;
    smooth.transmission_dispersion_scale = 1.0f;
    smooth.transmission_dispersion_abbe_number = 20.0f;
    expect_vec_near(rt::openpbr_dispersion_ior(smooth, context), {ior_c, ior_d, ior_f}, 2e-6,
        "OpenPBR dispersion context resolves long, medium, and short IORs");

    const rt::OpenPbrLobeProbabilities smooth_probabilities =
        rt::openpbr_lobe_probabilities(smooth, wo.z, true, context);
    rt::OpenPbrSample channel_samples[3];
    for (int channel = 0; channel < 3; ++channel) {
        const float channel_u = (static_cast<float>(channel) + 0.5f) / 3.0f;
        channel_samples[channel] = rt::sample_openpbr_core(smooth, frame, wo,
            smooth_probabilities.reflection + smooth_probabilities.transmission * channel_u,
            0.2f, 0.8f, context);
        expect_true(channel_samples[channel].valid != 0
                        && channel_samples[channel].delta != 0
                        && channel_samples[channel].event
                               == rt::OpenPbrScatterEvent::glossy_transmission,
            "smooth dispersion samples a delta transmission channel");
        expect_near(channel_samples[channel].discrete_pdf,
            smooth_probabilities.transmission / 3.0f, 2e-6,
            "smooth dispersion includes channel selection in the discrete PDF");
        for (int component = 0; component < 3; ++component) {
            const float weight = rt::openpbr_component(channel_samples[channel].weight, component);
            expect_true(component == channel ? weight > 0.0f : weight == 0.0f,
                "smooth dispersion returns an unbiased one-channel delta weight");
        }
    }
    const auto tangent_length = [](const rt::OpenPbrVec3& direction) {
        return std::sqrt(direction.x * direction.x + direction.y * direction.y);
    };
    expect_true(tangent_length(channel_samples[0].wi) > tangent_length(channel_samples[1].wi)
                    && tangent_length(channel_samples[1].wi)
                           > tangent_length(channel_samples[2].wi),
        "increasing short-wave IOR bends RGB transmission toward the normal");

    rt::OpenPbrCoreMaterial rough = smooth;
    rough.specular_roughness = 0.28f;
    const rt::OpenPbrTransportContext weighted_context {
        .rgb_wavelengths_nm = context.rgb_wavelengths_nm,
        .path_throughput = {0.2f, 0.3f, 0.5f},
    };
    expect_vec_near(rt::openpbr_channel_probabilities(weighted_context),
        {0.2f, 0.3f, 0.5f}, 1e-7, "path throughput controls RGB channel selection");
    expect_near(integrate_pdf(rough, frame, wo, kIntegrationSamples, weighted_context),
        1.0, 1.2e-2, "dispersion MIS PDF normalization");
    const rt::OpenPbrVec3 energy =
        integrate_furnace(rough, frame, wo, kIntegrationSamples, weighted_context);
    expect_finite_nonnegative(energy, "dispersion furnace energy");
    expect_true(energy.x <= 1.02f && energy.y <= 1.02f && energy.z <= 1.02f,
        "dispersion only redistributes bounded dielectric energy");

    int matched_samples = 0;
    for (int index = 0; index < 4096; ++index) {
        const float u_lobe = std::fmod(0.618033989f * static_cast<float>(index + 1), 1.0f);
        const float u1 = std::fmod(0.754877666f * static_cast<float>(index + 1), 1.0f);
        const float u2 = std::fmod(0.569840296f * static_cast<float>(index + 1), 1.0f);
        const rt::OpenPbrSample sample =
            rt::sample_openpbr_core(rough, frame, wo, u_lobe, u1, u2, weighted_context);
        if (sample.valid == 0 || sample.delta != 0) {
            continue;
        }
        const rt::OpenPbrEvaluation evaluation =
            rt::evaluate_openpbr_core(rough, frame, wo, sample.wi, weighted_context);
        expect_relative(sample.pdf, evaluation.pdf, 8e-5,
            "dispersion sample/evaluate PDF agreement");
        expect_vec_relative(sample.value, evaluation.value, 8e-5,
            "dispersion sample/evaluate value agreement");
        ++matched_samples;
    }
    expect_true(matched_samples > 3000,
        "dispersion sampling produces a stable continuous population");

    rt::OpenPbrCoreMaterial zero_scale = rough;
    zero_scale.transmission_dispersion_scale = 0.0f;
    zero_scale.transmission_dispersion_abbe_number = 9.0f;
    rt::OpenPbrCoreMaterial legacy = zero_scale;
    legacy.transmission_dispersion_abbe_number = 20.0f;
    const rt::OpenPbrVec3 wi = normalized({-0.15f, 0.1f, -1.0f});
    const rt::OpenPbrEvaluation zero_eval =
        rt::evaluate_openpbr_core(zero_scale, frame, wo, wi, weighted_context);
    const rt::OpenPbrEvaluation legacy_eval =
        rt::evaluate_openpbr_core(legacy, frame, wo, wi, weighted_context);
    expect_vec_near(zero_eval.value, legacy_eval.value, 0.0,
        "zero dispersion scale preserves the exact legacy response");
    expect_near(zero_eval.pdf, legacy_eval.pdf, 0.0,
        "zero dispersion scale preserves the exact legacy PDF");
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
    test_coat_and_fuzz_reference_semantics();
    test_thin_film_reference_semantics();
    test_dispersion_reference_semantics();
    test_openpbr_reference_parameter_semantics();
    test_anisotropy_rotates_with_tangent();
    test_opacity_thin_walled_and_emission();
    return 0;
}
