#pragma once

#include <cstdint>

#include <Eigen/Core>

namespace Eigen {
using Vector3b = Eigen::Vector<uint8_t, 3>;
}

#define HD_ATTR __host__ __device__
#define H_ATTR __host__
#define D_ATTR __device__
