#include "scene/analytic_light_compiler.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <string_view>

namespace rt {

void finalize_analytic_light_distribution(std::vector<AnalyticLightDesc>& lights) {
    double total_weight = 0.0;
    for (AnalyticLightDesc& light : lights) {
        light.selection_weight =
            std::isfinite(light.selection_weight) ? std::max(0.0, light.selection_weight) : 0.0;
        total_weight += light.selection_weight;
    }

    double cdf = 0.0;
    for (AnalyticLightDesc& light : lights) {
        light.selection_pdf = total_weight > 0.0 ? light.selection_weight / total_weight : 0.0;
        cdf += light.selection_pdf;
        light.cdf = cdf;
    }
    if (total_weight > 0.0) {
        const auto last_positive = std::find_if(lights.rbegin(), lights.rend(),
            [](const AnalyticLightDesc& light) { return light.selection_pdf > 0.0; });
        const std::size_t close_index =
            lights.size() - 1
            - static_cast<std::size_t>(std::distance(lights.rbegin(), last_positive));
        for (std::size_t i = close_index; i < lights.size(); ++i) {
            lights[i].cdf = 1.0;
        }
    }
}

} // namespace rt

namespace rt::scene {
namespace {

constexpr double kTransformTolerance = 1e-6;

Eigen::Vector3d blackbody_temperature_rgb(double temperature_kelvin) {
    // Values and interpolation follow OpenUSD pxr/usd/usdLux/blackbody.cpp.
    // Copyright 2016 Pixar; used under the OpenUSD LICENSE.txt terms.
    // OpenUSD's UsdLuxBlackbodyTemperatureAsRgb lookup table, interpolated with
    // the same Catmull-Rom basis and normalized to Rec.709 luminance.
    static constexpr std::array<std::array<double, 3>, 22> kRgb {{
        {1.000000, 0.027490, 0.000000},
        {1.000000, 0.027490, 0.000000},
        {1.000000, 0.149664, 0.000000},
        {1.000000, 0.256644, 0.008095},
        {1.000000, 0.372033, 0.067450},
        {1.000000, 0.476725, 0.153601},
        {1.000000, 0.570376, 0.259196},
        {1.000000, 0.653480, 0.377155},
        {1.000000, 0.726878, 0.501606},
        {1.000000, 0.791543, 0.628050},
        {1.000000, 0.848462, 0.753228},
        {1.000000, 0.898581, 0.874905},
        {1.000000, 0.942771, 0.991642},
        {0.906947, 0.890456, 1.000000},
        {0.828247, 0.841838, 1.000000},
        {0.765791, 0.801896, 1.000000},
        {0.715255, 0.768579, 1.000000},
        {0.673683, 0.740423, 1.000000},
        {0.638992, 0.716359, 1.000000},
        {0.609681, 0.695588, 1.000000},
        {0.609681, 0.695588, 1.000000},
        {0.609681, 0.695588, 1.000000},
    }};
    static constexpr std::array<std::array<double, 4>, 4> kBasis {{
        {-0.5, 1.5, -1.5, 0.5},
        {1.0, -2.5, 2.0, -0.5},
        {-0.5, 0.0, 0.5, 0.0},
        {0.0, 1.0, 0.0, 0.0},
    }};

    const double u_spline = std::clamp((temperature_kelvin - 1000.0) / 9000.0, 0.0, 1.0);
    constexpr int kSegmentCount = static_cast<int>(kRgb.size()) - 4;
    const double x = u_spline * kSegmentCount;
    const int segment = static_cast<int>(std::floor(x));
    const double u = x - segment;
    const auto knot = [&](int offset) {
        const auto& value = kRgb[static_cast<std::size_t>(segment + offset)];
        return Eigen::Vector3d {value[0], value[1], value[2]};
    };
    const Eigen::Vector3d k0 = knot(0);
    const Eigen::Vector3d k1 = knot(1);
    const Eigen::Vector3d k2 = knot(2);
    const Eigen::Vector3d k3 = knot(3);
    const auto coefficient = [&](int row) {
        return kBasis[static_cast<std::size_t>(row)][0] * k0
               + kBasis[static_cast<std::size_t>(row)][1] * k1
               + kBasis[static_cast<std::size_t>(row)][2] * k2
               + kBasis[static_cast<std::size_t>(row)][3] * k3;
    };
    Eigen::Vector3d rgb =
        ((coefficient(0) * u + coefficient(1)) * u + coefficient(2)) * u + coefficient(3);
    const double luminance = rgb.dot(Eigen::Vector3d {0.2126, 0.7152, 0.0722});
    rgb /= luminance;
    return rgb.cwiseMax(0.0);
}

double rec709_luminance(const Eigen::Vector3d& value) {
    return value.dot(Eigen::Vector3d {0.2126, 0.7152, 0.0722});
}

double require_similarity_scale(const Eigen::Matrix3d& linear, std::string_view path,
    std::string_view light_type) {
    const Eigen::Matrix3d gram = linear.transpose() * linear;
    const double scale_squared = gram.trace() / 3.0;
    if (!std::isfinite(scale_squared) || scale_squared <= 0.0) {
        throw std::invalid_argument(
            std::string {light_type} + " light has a singular transform: " + std::string {path});
    }
    const double tolerance = kTransformTolerance * std::max(1.0, scale_squared);
    if ((gram - Eigen::Matrix3d::Identity() * scale_squared).cwiseAbs().maxCoeff() > tolerance) {
        throw std::invalid_argument(
            std::string {light_type}
            + " light requires a similarity transform in this backend: " + std::string {path});
    }
    return std::sqrt(scale_squared);
}

AnalyticLightType runtime_light_type(SceneLightType type) {
    switch (type) {
        case SceneLightType::sphere: return AnalyticLightType::sphere;
        case SceneLightType::disk: return AnalyticLightType::disk;
        case SceneLightType::rect: return AnalyticLightType::rect;
        case SceneLightType::cylinder: return AnalyticLightType::cylinder;
        case SceneLightType::distant: return AnalyticLightType::distant;
        case SceneLightType::dome: return AnalyticLightType::dome;
        case SceneLightType::geometry:
            throw std::invalid_argument("geometry light is not an intrinsic analytic light");
    }
    throw std::logic_error("unreachable SceneLightType value");
}

double distant_size_factor(double theta_max) {
    if (theta_max <= 0.0) {
        return 1.0;
    }
    const double sine_squared = std::sin(theta_max) * std::sin(theta_max);
    return theta_max <= std::numbers::pi / 2.0 ? std::numbers::pi * sine_squared
                                               : std::numbers::pi * (2.0 - sine_squared);
}

AnalyticLightDesc compile_light(const ScenePrim& prim, const SceneLight& light,
    const Eigen::Matrix4d& world) {
    if (!prim.asset_references.empty()) {
        throw std::invalid_argument(
            "runtime analytic light textures are not supported yet: " + prim.path);
    }
    if (std::abs(light.diffuse - 1.0) > 1e-12 || std::abs(light.specular - 1.0) > 1e-12) {
        throw std::invalid_argument(
            "runtime analytic light diffuse/specular overrides are not supported yet: "
            + prim.path);
    }

    AnalyticLightDesc out;
    out.type = runtime_light_type(light.type);
    out.source_path = prim.path;
    out.position = world.block<3, 1>(0, 3);
    out.local_to_world_linear = world.block<3, 3>(0, 0);
    out.radius = light.radius;
    out.width = light.width;
    out.height = light.height;
    out.length = light.length;
    out.diffuse = light.diffuse;
    out.specular = light.specular;
    out.treat_as_point = light.treat_as_point;
    out.treat_as_line = light.treat_as_line;

    double size_factor = 1.0;
    switch (light.type) {
        case SceneLightType::sphere: {
            const double scale =
                require_similarity_scale(out.local_to_world_linear, prim.path, "sphere");
            out.world_area = 4.0 * std::numbers::pi * std::pow(light.radius * scale, 2.0);
            size_factor = out.world_area;
            break;
        }
        case SceneLightType::disk:
            out.world_area =
                std::numbers::pi * light.radius * light.radius
                * out.local_to_world_linear.col(0).cross(out.local_to_world_linear.col(1)).norm();
            size_factor = out.world_area;
            break;
        case SceneLightType::rect:
            out.world_area =
                light.width * light.height
                * out.local_to_world_linear.col(0).cross(out.local_to_world_linear.col(1)).norm();
            size_factor = out.world_area;
            break;
        case SceneLightType::cylinder: {
            const double scale =
                require_similarity_scale(out.local_to_world_linear, prim.path, "cylinder");
            out.world_area =
                2.0 * std::numbers::pi * (light.radius * scale) * (light.length * scale);
            size_factor = out.world_area;
            break;
        }
        case SceneLightType::distant: {
            const double angle_degrees = std::clamp(light.angle_degrees, 0.0, 360.0);
            const double theta_max = angle_degrees * std::numbers::pi / 360.0;
            out.cos_theta_max = std::cos(theta_max);
            out.delta = theta_max <= 1e-12;
            size_factor = distant_size_factor(theta_max);
            break;
        }
        case SceneLightType::dome: break;
        case SceneLightType::geometry: throw std::logic_error("geometry light reached compiler");
    }

    Eigen::Vector3d color = light.color;
    if (light.enable_color_temperature) {
        color = color.cwiseProduct(blackbody_temperature_rgb(light.color_temperature_kelvin));
    }
    const double normalization = light.normalize ? size_factor : 1.0;
    out.radiance = color * (scene_light_exposed_intensity(light) / normalization);

    const double luminance = std::max(0.0, rec709_luminance(out.radiance));
    if (light.type == SceneLightType::dome) {
        out.selection_weight = 4.0 * std::numbers::pi * luminance;
    } else if (light.type == SceneLightType::distant) {
        const double solid_angle =
            out.delta ? 1.0 : 2.0 * std::numbers::pi * (1.0 - out.cos_theta_max);
        out.selection_weight = solid_angle * luminance;
    } else {
        out.selection_weight = out.world_area * luminance;
    }
    return out;
}

} // namespace

std::vector<AnalyticLightDesc> compile_analytic_lights(const SceneIRv2& scene) {
    require_valid_scene_ir_v2(scene);
    std::vector<AnalyticLightDesc> out;
    const double time_code = scene.stage_metadata().start_time_code.value_or(0.0);
    for (const ScenePrim& prim : scene.prims()) {
        if (!prim.light || prim.light->type == SceneLightType::geometry
            || !compute_scene_visibility(scene, prim.path)) {
            continue;
        }
        const ScenePurpose purpose = compute_scene_purpose(scene, prim.path);
        if (purpose == ScenePurpose::proxy || purpose == ScenePurpose::guide) {
            continue;
        }
        out.push_back(compile_light(prim, *prim.light,
            compute_scene_world_transform(scene, prim.path, time_code)));
    }
    finalize_analytic_light_distribution(out);
    return out;
}

} // namespace rt::scene
