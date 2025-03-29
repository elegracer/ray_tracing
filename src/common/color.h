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

inline bool hit_sphere(const Vec3d& center, const double radius, const Ray& ray) {
    const Vec3d oc = center - ray.origin();
    const double a = ray.direction().squaredNorm();
    const double b = -2.0 * ray.direction().dot(oc);
    const double c = oc.squaredNorm() - radius * radius;
    const double discriminant = b * b - 4.0 * a * c;
    return discriminant >= 0.0;
}

inline Vec3d ray_color(const Ray& ray) {
    if (hit_sphere(Vec3d {0.0, 0.0, -1.0}, 0.5, ray)) {
        return Vec3d {1.0, 0.0, 0.0};
    }

    const Vec3d unit_direction = ray.direction().normalized();
    const double a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * Vec3d {1.0, 1.0, 1.0} + a * Vec3d {0.5, 0.7, 1.0};
}
