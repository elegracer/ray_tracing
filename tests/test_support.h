#pragma once

#include <Eigen/Core>

#include <cmath>
#include <stdexcept>
#include <string>

inline void expect_true(bool value, const std::string& label) {
    if (!value) {
        throw std::runtime_error("expect_true failed: " + label);
    }
}

inline void expect_near(double actual, double expected, double tol, const std::string& label) {
    if (std::abs(actual - expected) > tol) {
        throw std::runtime_error("expect_near failed: " + label);
    }
}

inline void expect_vec3_near(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
    double tol, const std::string& label) {
    if ((actual - expected).cwiseAbs().maxCoeff() > tol) {
        throw std::runtime_error("expect_vec3_near failed: " + label);
    }
}
