#pragma once

#include "common/defs.h"
#include "common/ray.h"

inline Eigen::Vector3i scale_normalized_color(const Eigen::Vector3d& color_normalized,
    const int scale) {
    return (color_normalized * ((double)scale - 0.001))
        .cast<int>()
        .array()
        .max(0)
        .min(scale - 1)
        .matrix();
}

inline Eigen::Vector3d ray_color(const Ray& ray) {
    const Eigen::Vector3d unit_direction = ray.direction().normalized();
    const double a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * Eigen::Vector3d {1.0, 1.0, 1.0} + a * Eigen::Vector3d {0.5, 0.7, 1.0};
}
