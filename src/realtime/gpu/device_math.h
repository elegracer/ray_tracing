#pragma once

#include <cuda_runtime.h>

#include <cmath>

namespace rt {

struct DeviceVec3 {
    float x;
    float y;
    float z;
};

__host__ __device__ inline DeviceVec3 make_device_vec3(float x, float y, float z) {
    return DeviceVec3 {x, y, z};
}

__host__ __device__ inline float3 add3(const float3& a, const float3& b) {
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__host__ __device__ inline float3 sub3(const float3& a, const float3& b) {
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__host__ __device__ inline float3 mul3(const float3& a, const float3& b) {
    return make_float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

__host__ __device__ inline float3 mul3(const float3& v, float s) {
    return make_float3(v.x * s, v.y * s, v.z * s);
}

__host__ __device__ inline float3 div3(const float3& v, float s) {
    const float inv = 1.0f / s;
    return make_float3(v.x * inv, v.y * inv, v.z * inv);
}

__host__ __device__ inline float dot3(const float3& a, const float3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__host__ __device__ inline float3 cross3(const float3& a, const float3& b) {
    return make_float3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

__host__ __device__ inline float length_sq3(const float3& v) {
    return dot3(v, v);
}

__host__ __device__ inline float length3(const float3& v) {
    return std::sqrt(length_sq3(v));
}

__host__ __device__ inline float3 normalize3(const float3& v) {
    const float len = length3(v);
    if (len <= 1e-8f) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    return div3(v, len);
}

__host__ __device__ inline float3 reflect3(const float3& incident, const float3& normal) {
    return sub3(incident, mul3(normal, 2.0f * dot3(incident, normal)));
}

__host__ __device__ inline float3 refract3(const float3& incident, const float3& normal, float eta) {
    const float cos_theta = fminf(dot3(mul3(incident, -1.0f), normal), 1.0f);
    const float3 r_out_perp = mul3(add3(incident, mul3(normal, cos_theta)), eta);
    const float k = 1.0f - length_sq3(r_out_perp);
    const float3 r_out_parallel = mul3(normal, -std::sqrt(fmaxf(k, 0.0f)));
    return add3(r_out_perp, r_out_parallel);
}

__host__ __device__ inline float max_component3(const float3& v) {
    return fmaxf(v.x, fmaxf(v.y, v.z));
}

__host__ __device__ inline float3 clamp3(const float3& v, float lo, float hi) {
    return make_float3(
        fminf(fmaxf(v.x, lo), hi),
        fminf(fmaxf(v.y, lo), hi),
        fminf(fmaxf(v.z, lo), hi));
}

}  // namespace rt
