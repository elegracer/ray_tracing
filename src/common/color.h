#pragma once

#include "common/defs.h"
#include "common/ray.h"

inline Vec3i scale_normalized_color(const Vec3d& color_normalized, const int scale) {
    return (color_normalized * ((double)scale - 0.001))
        .cast<int>()
        .array()
        .max(0)
        .min(scale - 1)
        .matrix();
}

inline double hit_sphere(const Vec3d& center, const double radius, const Ray& ray) {
    const Vec3d oc = center - ray.origin();
    const double a = ray.direction().squaredNorm();
    const double h = ray.direction().dot(oc);
    const double c = oc.squaredNorm() - radius * radius;
    const double discriminant = h * h - a * c;

    if (discriminant < 0.0) {
        return -1.0;
    }

    return (h - std::sqrt(discriminant)) / a;
}

inline Vec3d ray_color(const Ray& ray) {
    const double t = hit_sphere(Vec3d {0.0, 0.0, -1.0}, 0.5, ray);
    if (t > 0.0) {
        const Vec3d normal = (ray.at(t) - Vec3d {0.0, 0.0, -1.0}).normalized();
        return 0.5 * (normal + Vec3d::Ones());
    }

    const Vec3d unit_direction = ray.direction().normalized();
    const double a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * Vec3d {1.0, 1.0, 1.0} + a * Vec3d {0.5, 0.7, 1.0};
}
