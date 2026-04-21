#pragma once

#include <Eigen/Core>
#include <sophus/se3.hpp>

namespace Eigen {

using Vector6d = Matrix<double, 6, 1>;

}  // namespace Eigen

namespace pico_common {

constexpr double D_EPS = 1e-9;

template <typename Scalar>
constexpr Scalar eps() {
    return static_cast<Scalar>(1e-9);
}

}  // namespace pico_common

constexpr int LUT_SIZE_1D = 512;
constexpr int MAX_UNDIST_ITER = 25;
constexpr double MAX_UNDIST_THRE = 1e-12;
