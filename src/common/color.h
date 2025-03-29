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

inline Eigen::Vector3i ray_color(const Ray& ray) {
    return {0, 0, 0};
}
