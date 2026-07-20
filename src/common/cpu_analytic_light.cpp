#include "common/cpu_analytic_light.h"

#include "common/common.h"
#include "common/interval.h"
#include "common/ray.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace rt {
namespace {

constexpr double kRayEpsilon = 1e-6;

bool finite_light(AnalyticLightType type) {
    return type == AnalyticLightType::sphere || type == AnalyticLightType::disk
           || type == AnalyticLightType::rect || type == AnalyticLightType::cylinder;
}

double power_heuristic(double pdf_a, double pdf_b) {
    if (pdf_a <= 0.0) {
        return 0.0;
    }
    const double a2 = pdf_a * pdf_a;
    const double b2 = pdf_b * pdf_b;
    return a2 / (a2 + b2);
}

double uniform_sphere_pdf() {
    return 1.0 / (4.0 * std::numbers::pi);
}

double uniform_cone_pdf(double cos_theta_max) {
    const double solid_angle = 2.0 * std::numbers::pi * (1.0 - cos_theta_max);
    return solid_angle > 1e-12 ? 1.0 / solid_angle : 0.0;
}

double area_to_solid_angle_pdf(double area, double distance_squared, double abs_cosine) {
    if (area <= 1e-12 || distance_squared <= 0.0 || abs_cosine <= 1e-8) {
        return 0.0;
    }
    return distance_squared / (area * abs_cosine);
}

Eigen::Vector3d uniform_sphere_direction(double u0, double u1) {
    const double z = 1.0 - 2.0 * std::clamp(u0, 0.0, 1.0);
    const double radial = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = 2.0 * std::numbers::pi * std::clamp(u1, 0.0, 1.0);
    return {radial * std::cos(phi), radial * std::sin(phi), z};
}

Eigen::Vector3d uniform_cone_direction(const Eigen::Vector3d& axis, double cos_theta_max, double u0,
    double u1) {
    const Eigen::Vector3d normalized_axis = axis.normalized();
    const double cos_theta = 1.0 - std::clamp(u0, 0.0, 1.0) * (1.0 - cos_theta_max);
    const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
    const double phi = 2.0 * std::numbers::pi * std::clamp(u1, 0.0, 1.0);
    const Eigen::Vector3d helper =
        std::abs(normalized_axis.x()) > 0.9 ? Eigen::Vector3d::UnitY() : Eigen::Vector3d::UnitX();
    const Eigen::Vector3d tangent = helper.cross(normalized_axis).normalized();
    const Eigen::Vector3d bitangent = normalized_axis.cross(tangent);
    return (normalized_axis * cos_theta + tangent * (sin_theta * std::cos(phi))
            + bitangent * (sin_theta * std::sin(phi)))
        .normalized();
}

Eigen::Vector3d plane_normal(const AnalyticLightDesc& light) {
    return -light.local_to_world_linear.col(0)
                .cross(light.local_to_world_linear.col(1))
                .normalized();
}

bool plane_coordinates(const AnalyticLightDesc& light, const Eigen::Vector3d& point,
    double& local_x, double& local_y) {
    const Eigen::Vector3d basis_x = light.local_to_world_linear.col(0);
    const Eigen::Vector3d basis_y = light.local_to_world_linear.col(1);
    const Eigen::Vector3d local = point - light.position;
    const double xx = basis_x.dot(basis_x);
    const double xy = basis_x.dot(basis_y);
    const double yy = basis_y.dot(basis_y);
    const double determinant = xx * yy - xy * xy;
    if (determinant <= 1e-16) {
        return false;
    }
    const double px = local.dot(basis_x);
    const double py = local.dot(basis_y);
    local_x = (px * yy - py * xy) / determinant;
    local_y = (py * xx - px * xy) / determinant;
    return true;
}

bool intersect_light(const AnalyticLightDesc& light, const Ray& ray, const Interval& ray_t,
    CpuAnalyticLightHit& hit) {
    if (light.type == AnalyticLightType::sphere) {
        const double radius = light.radius * light.local_to_world_linear.col(0).norm();
        const Eigen::Vector3d oc = ray.origin() - light.position;
        const double a = ray.direction().squaredNorm();
        const double half_b = oc.dot(ray.direction());
        const double c = oc.squaredNorm() - radius * radius;
        const double discriminant = half_b * half_b - a * c;
        if (a <= 1e-16 || discriminant < 0.0) {
            return false;
        }
        const double root_term = std::sqrt(discriminant);
        double root = (-half_b - root_term) / a;
        if (!ray_t.surrounds(root)) {
            root = (-half_b + root_term) / a;
        }
        if (!ray_t.surrounds(root)) {
            return false;
        }
        const Eigen::Vector3d position = ray.at(root);
        const Eigen::Vector3d normal = (position - light.position).normalized();
        if (ray.direction().dot(normal) >= 0.0) {
            return false;
        }
        hit.hit = true;
        hit.t = root;
        hit.position = position;
        hit.normal = normal;
        return true;
    }

    if (light.type == AnalyticLightType::disk || light.type == AnalyticLightType::rect) {
        const Eigen::Vector3d normal = plane_normal(light);
        const double denominator = ray.direction().dot(normal);
        if (denominator >= -1e-8) {
            return false;
        }
        const double t = (light.position - ray.origin()).dot(normal) / denominator;
        if (!ray_t.surrounds(t)) {
            return false;
        }
        const Eigen::Vector3d position = ray.at(t);
        double local_x = 0.0;
        double local_y = 0.0;
        if (!plane_coordinates(light, position, local_x, local_y)) {
            return false;
        }
        const bool inside =
            light.type == AnalyticLightType::disk
                ? local_x * local_x + local_y * local_y <= light.radius * light.radius
                : std::abs(local_x) <= 0.5 * light.width && std::abs(local_y) <= 0.5 * light.height;
        if (!inside) {
            return false;
        }
        hit.hit = true;
        hit.t = t;
        hit.position = position;
        hit.normal = normal;
        return true;
    }

    if (light.type != AnalyticLightType::cylinder) {
        return false;
    }
    const Eigen::Vector3d basis_x = light.local_to_world_linear.col(0);
    const Eigen::Vector3d basis_y = light.local_to_world_linear.col(1);
    const Eigen::Vector3d basis_z = light.local_to_world_linear.col(2);
    const double scale = basis_x.norm();
    if (scale <= 1e-8 || basis_z.norm() <= 1e-8) {
        return false;
    }
    const Eigen::Vector3d axis_x = basis_x / scale;
    const Eigen::Vector3d axis_y = basis_y / scale;
    const Eigen::Vector3d axis_z = basis_z.normalized();
    const Eigen::Vector3d local_origin = ray.origin() - light.position;
    const double ox = local_origin.dot(axis_x);
    const double oy = local_origin.dot(axis_y);
    const double oz = local_origin.dot(axis_z);
    const double dx = ray.direction().dot(axis_x);
    const double dy = ray.direction().dot(axis_y);
    const double dz = ray.direction().dot(axis_z);
    const double radius = light.radius * scale;
    const double a = dx * dx + dy * dy;
    const double half_b = ox * dx + oy * dy;
    const double c = ox * ox + oy * oy - radius * radius;
    const double discriminant = half_b * half_b - a * c;
    if (a <= 1e-16 || discriminant < 0.0) {
        return false;
    }
    const double root_term = std::sqrt(discriminant);
    double root = (-half_b - root_term) / a;
    double z = oz + root * dz;
    const double half_length = 0.5 * light.length * basis_z.norm();
    if (!ray_t.surrounds(root) || std::abs(z) > half_length) {
        root = (-half_b + root_term) / a;
        z = oz + root * dz;
    }
    if (!ray_t.surrounds(root) || std::abs(z) > half_length) {
        return false;
    }
    const double x = ox + root * dx;
    const double y = oy + root * dy;
    const Eigen::Vector3d normal = (axis_x * x + axis_y * y).normalized();
    if (ray.direction().dot(normal) >= 0.0) {
        return false;
    }
    hit.hit = true;
    hit.t = root;
    hit.position = ray.at(root);
    hit.normal = normal;
    return true;
}

double infinite_pdf(const AnalyticLightDesc& light) {
    if (light.delta) {
        return 0.0;
    }
    return light.type == AnalyticLightType::dome ? uniform_sphere_pdf()
                                                 : uniform_cone_pdf(light.cos_theta_max);
}

bool supports_infinite_direction(const AnalyticLightDesc& light, const Eigen::Vector3d& direction) {
    if (light.type == AnalyticLightType::dome) {
        return true;
    }
    if (light.type != AnalyticLightType::distant) {
        return false;
    }
    const Eigen::Vector3d axis = light.local_to_world_linear.col(2).normalized();
    const double threshold = light.delta ? 0.999999 : light.cos_theta_max;
    return axis.dot(direction.normalized()) >= threshold;
}

} // namespace

CpuAnalyticLightSampler::CpuAnalyticLightSampler(std::vector<AnalyticLightDesc> lights)
    : lights_(std::move(lights)) {}

CpuAnalyticLightSample CpuAnalyticLightSampler::sample(const Eigen::Vector3d& surface_point,
    double light_sample, double shape_sample_0, double shape_sample_1) const {
    CpuAnalyticLightSample sample;
    if (lights_.empty()) {
        return sample;
    }

    const double selected = std::clamp(light_sample, 0.0, std::nextafter(1.0, 0.0));
    auto light_it = std::find_if(lights_.begin(), lights_.end(),
        [selected](const auto& light) { return selected < light.cdf; });
    if (light_it == lights_.end()) {
        light_it = std::prev(lights_.end());
    }
    const AnalyticLightDesc& light = *light_it;
    if (light.selection_pdf <= 0.0) {
        return sample;
    }

    sample.radiance = light.radiance;
    if (light.type == AnalyticLightType::dome) {
        sample.direction = uniform_sphere_direction(shape_sample_0, shape_sample_1);
        sample.distance = infinity;
        sample.pdf = light.selection_pdf * uniform_sphere_pdf();
        sample.infinite = true;
    } else if (light.type == AnalyticLightType::distant) {
        const Eigen::Vector3d axis = light.local_to_world_linear.col(2).normalized();
        sample.direction = light.delta ? axis
                                       : uniform_cone_direction(axis, light.cos_theta_max,
                                             shape_sample_0, shape_sample_1);
        sample.distance = infinity;
        sample.pdf =
            light.selection_pdf * (light.delta ? 1.0 : uniform_cone_pdf(light.cos_theta_max));
        sample.infinite = true;
        sample.delta = light.delta;
    } else if (light.type == AnalyticLightType::sphere) {
        const Eigen::Vector3d to_center = light.position - surface_point;
        const double distance_squared = to_center.squaredNorm();
        const double radius = light.radius * light.local_to_world_linear.col(0).norm();
        const double radius_squared = radius * radius;
        if (distance_squared <= radius_squared * (1.0 + 1e-6)) {
            return {};
        }
        if (light.treat_as_point) {
            sample.distance = std::sqrt(distance_squared);
            sample.direction = to_center / sample.distance;
            const double intensity_scale = light.world_area > 1e-12 ? light.world_area : 1.0;
            sample.radiance *= intensity_scale / distance_squared;
            sample.pdf = light.selection_pdf;
            sample.delta = true;
        } else {
            const double cos_theta_max =
                std::sqrt(std::max(0.0, 1.0 - radius_squared / distance_squared));
            sample.direction =
                uniform_cone_direction(to_center, cos_theta_max, shape_sample_0, shape_sample_1);
            CpuAnalyticLightHit surface_hit;
            if (!intersect_light(light, Ray {surface_point, sample.direction},
                    Interval {kRayEpsilon, infinity}, surface_hit)) {
                return {};
            }
            sample.position = surface_hit.position;
            sample.normal = surface_hit.normal;
            sample.distance = surface_hit.t;
            sample.pdf = light.selection_pdf * uniform_cone_pdf(cos_theta_max);
        }
    } else {
        const double u0 = std::clamp(shape_sample_0, 0.0, 1.0);
        const double u1 = std::clamp(shape_sample_1, 0.0, 1.0);
        const Eigen::Vector3d basis_x = light.local_to_world_linear.col(0);
        const Eigen::Vector3d basis_y = light.local_to_world_linear.col(1);
        const Eigen::Vector3d basis_z = light.local_to_world_linear.col(2);
        if (light.type == AnalyticLightType::disk) {
            const double radial = light.radius * std::sqrt(u0);
            const double phi = 2.0 * std::numbers::pi * u1;
            sample.position = light.position + basis_x * (radial * std::cos(phi))
                              + basis_y * (radial * std::sin(phi));
            sample.normal = plane_normal(light);
        } else if (light.type == AnalyticLightType::rect) {
            sample.position = light.position + basis_x * ((u0 - 0.5) * light.width)
                              + basis_y * ((u1 - 0.5) * light.height);
            sample.normal = plane_normal(light);
        } else if (light.type == AnalyticLightType::cylinder) {
            const double phi = 2.0 * std::numbers::pi * u0;
            const double radial_x = light.radius * std::cos(phi);
            const double radial_y = light.radius * std::sin(phi);
            sample.position = light.position + basis_x * radial_x + basis_y * radial_y
                              + basis_z * ((u1 - 0.5) * light.length);
            sample.normal = (basis_x * radial_x + basis_y * radial_y).normalized();
        } else {
            return {};
        }

        const Eigen::Vector3d to_light = sample.position - surface_point;
        const double distance_squared = to_light.squaredNorm();
        if (distance_squared <= 1e-12) {
            return {};
        }
        sample.distance = std::sqrt(distance_squared);
        sample.direction = to_light / sample.distance;
        const double abs_cosine = sample.normal.dot(-sample.direction);
        const double conditional_pdf =
            area_to_solid_angle_pdf(light.world_area, distance_squared, abs_cosine);
        sample.pdf = light.selection_pdf * conditional_pdf;
    }

    sample.valid = sample.pdf > 0.0 && sample.direction.allFinite() && sample.radiance.allFinite()
                   && sample.radiance.maxCoeff() > 0.0;
    return sample;
}

bool CpuAnalyticLightSampler::intersect(const Ray& ray, const Interval& ray_t,
    CpuAnalyticLightHit& hit) const {
    double closest = ray_t.max;
    bool found = false;
    for (std::size_t i = 0; i < lights_.size(); ++i) {
        const AnalyticLightDesc& light = lights_[i];
        if (!finite_light(light.type) || light.treat_as_point) {
            continue;
        }
        CpuAnalyticLightHit candidate;
        if (!intersect_light(light, ray, Interval {ray_t.min, closest}, candidate)) {
            continue;
        }
        candidate.light_index = static_cast<int>(i);
        candidate.radiance = light.radiance;
        closest = candidate.t;
        hit = candidate;
        found = true;
    }
    return found;
}

double CpuAnalyticLightSampler::pdf_for_hit(const CpuAnalyticLightHit& hit,
    const Eigen::Vector3d& origin, const Eigen::Vector3d& direction) const {
    if (hit.light_index < 0 || static_cast<std::size_t>(hit.light_index) >= lights_.size()) {
        return 0.0;
    }
    const AnalyticLightDesc& light = lights_[static_cast<std::size_t>(hit.light_index)];
    if (light.delta || light.treat_as_point || light.treat_as_line) {
        return 0.0;
    }
    double conditional_pdf = 0.0;
    if (light.type == AnalyticLightType::sphere) {
        const double radius = light.radius * light.local_to_world_linear.col(0).norm();
        const double distance_squared = (light.position - origin).squaredNorm();
        const double radius_squared = radius * radius;
        if (distance_squared <= radius_squared) {
            return 0.0;
        }
        const double cos_theta_max =
            std::sqrt(std::max(0.0, 1.0 - radius_squared / distance_squared));
        conditional_pdf = uniform_cone_pdf(cos_theta_max);
    } else {
        const double distance_squared = (hit.position - origin).squaredNorm();
        const double abs_cosine = std::max(0.0, hit.normal.dot(-direction.normalized()));
        conditional_pdf = area_to_solid_angle_pdf(light.world_area, distance_squared, abs_cosine);
    }
    return light.selection_pdf * conditional_pdf;
}

double CpuAnalyticLightSampler::emission_mis_weight(const CpuAnalyticLightHit& hit,
    const Eigen::Vector3d& origin, const Eigen::Vector3d& direction, double bsdf_pdf,
    bool previous_scatter_valid, bool previous_scatter_delta) const {
    if (!previous_scatter_valid || previous_scatter_delta) {
        return 1.0;
    }
    return power_heuristic(bsdf_pdf, pdf_for_hit(hit, origin, direction));
}

Eigen::Vector3d CpuAnalyticLightSampler::infinite_radiance(const Eigen::Vector3d& direction,
    double bsdf_pdf, bool previous_scatter_valid, bool previous_scatter_delta) const {
    Eigen::Vector3d radiance = Eigen::Vector3d::Zero();
    for (const AnalyticLightDesc& light : lights_) {
        if (!supports_infinite_direction(light, direction)) {
            continue;
        }
        double weight = 1.0;
        if (previous_scatter_valid && !previous_scatter_delta) {
            const double conditional_pdf = infinite_pdf(light);
            if (conditional_pdf <= 0.0) {
                continue;
            }
            weight = power_heuristic(bsdf_pdf, light.selection_pdf * conditional_pdf);
        }
        radiance += light.radiance * weight;
    }
    return radiance;
}

} // namespace rt
