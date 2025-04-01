#pragma once

#include <cstdint>
#include <numbers>
#include <random>

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


// Random Number Generation

inline double random_double() {
    // Returns a random real in [0,1)
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

inline double random_double(const double min, const double max) {
    // Returns a random real in [min,max)
    return min + (max - min) * random_double();
}

// Random Unit Vector Generation

inline Vec3d random_unit_vector() {
    while (true) {
        const Vec3d p = Vec3d::Random();
        const double norm_sq = p.squaredNorm();
        if (norm_sq > 1e-100 && norm_sq <= 1.0) {
            return p.normalized();
        }
    }
}

inline Vec3d random_on_hemisphere(const Vec3d& normal) {
    const Vec3d on_unit_sphere = random_unit_vector();
    if (on_unit_sphere.dot(normal) > 0.0) {
        return on_unit_sphere;
    } else {
        return -on_unit_sphere;
    }
}
