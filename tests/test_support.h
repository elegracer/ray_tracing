#pragma once

#include "common/common.h"

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

inline void expect_true(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void expect_near(const double actual, const double expected, const double epsilon,
    const std::string& message) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error(message);
    }
}

inline void expect_vec3_near(const Vec3d& actual, const Vec3d& expected, const double epsilon = 1e-9) {
    expect_near(actual.x(), expected.x(), epsilon, "x mismatch");
    expect_near(actual.y(), expected.y(), epsilon, "y mismatch");
    expect_near(actual.z(), expected.z(), epsilon, "z mismatch");
}
