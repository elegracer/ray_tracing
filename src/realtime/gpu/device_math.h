#pragma once

#include <cuda_runtime.h>

namespace rt {

struct DeviceVec3 {
    float x;
    float y;
    float z;
};

__host__ __device__ inline DeviceVec3 make_device_vec3(float x, float y, float z) {
    return DeviceVec3 {x, y, z};
}

}  // namespace rt
