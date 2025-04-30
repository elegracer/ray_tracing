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
    static std::mt19937 generator;
    return std::uniform_real_distribution<double> {0.0, 1.0}(generator);
}

inline double random_double(const double min, const double max) {
    // Returns a random real in [min,max)
    return min + (max - min) * random_double();
}

inline int random_int(const int min, const int max) {
    // Returns a random int in [min, max]
    static std::mt19937 generator;
    return min + std::uniform_int_distribution<int> {}(generator) % (max - min + 1);
}

inline Vec3d random_vec3d() {
    return Vec3d {random_double(), random_double(), random_double()};
}

inline Vec3d random_vec3d(const double min, const double max) {
    return Vec3d {random_double(min, max), random_double(min, max), random_double(min, max)};
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

inline Vec3d random_in_unit_disk() {
    while (true) {
        if (const Vec3d p = {random_double(-1.0, 1.0), random_double(-1.0, 1.0), 0.0};
            p.squaredNorm() < 1.0) {
            return p;
        }
    }
}

inline Vec3d reflect(const Vec3d& v, const Vec3d& n) {
    return v - 2.0 * v.dot(n) * n;
}

inline Vec3d refract(const Vec3d& uv, const Vec3d& n, const double etai_over_etat) {
    const double cos_theta = std::min(-uv.dot(n), 1.0);
    const Vec3d r_out_perp = etai_over_etat * (uv + cos_theta * n);
    const Vec3d r_out_parallel = -std::sqrt(std::abs(1.0 - r_out_perp.squaredNorm())) * n;
    return r_out_perp + r_out_parallel;
}
