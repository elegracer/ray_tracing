#pragma once

#include "common.h"
#include "ray.h"


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

inline double linear_to_gamma(const double linear_component) {
    if (linear_component > 0.0) {
        return std::sqrt(linear_component);
    }

    return 0.0;
}
