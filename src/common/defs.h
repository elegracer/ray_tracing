#pragma once

#include <cstdint>
#include <numbers>

#include <Eigen/Core>

// Constants

constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double pi = std::numbers::pi;

// Utility Functions

inline constexpr double deg2rad(const double deg) {
    return deg * pi / 180.0;
}

inline constexpr double rad2deg(const double rad) {
    return rad * 180.0 / pi;
}

// Type Aliases

using Vec3b = Eigen::Vector<uint8_t, 3>;
using Vec3d = Eigen::Vector3d;
using Vec3i = Eigen::Vector3i;
