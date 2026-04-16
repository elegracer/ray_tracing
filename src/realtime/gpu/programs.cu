#include "realtime/gpu/device_math.h"
#include "realtime/gpu/launch_params.h"

#include <cuda_runtime.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace rt {

struct Ray {
    float3 origin;
    float3 direction;
};

struct HitInfo {
    bool hit = false;
    float t = 0.0f;
    float3 position = make_float3(0.0f, 0.0f, 0.0f);
    float3 geometric_normal = make_float3(0.0f, 0.0f, 1.0f);
    float3 shading_normal = make_float3(0.0f, 0.0f, 1.0f);
    float3 base_color = make_float3(0.0f, 0.0f, 0.0f);
    float3 emission = make_float3(0.0f, 0.0f, 0.0f);
    float fuzz = 0.0f;
    float ior = 1.0f;
    int material_type = 0;
    bool front_face = true;
};

struct PathState {
    float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
    float3 radiance = make_float3(0.0f, 0.0f, 0.0f);
    Ray ray {};
    bool alive = true;
};

__device__ PathState trace_primary_ray(const LaunchParams& params, int x, int y);
__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit, PathState& state);
__device__ void sample_bsdf(const LaunchParams& params, const HitInfo& hit, std::uint32_t& rng, PathState& state);

namespace {

constexpr float kRayEpsilon = 1e-3f;
constexpr float kRayFar = 1e30f;
constexpr float kShadowScale = 0.8f;

__device__ std::uint32_t hash_u32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

__device__ std::uint32_t rng_for(int pixel_index, int sample, int stream) {
    const std::uint32_t seed = static_cast<std::uint32_t>(pixel_index) * 1973u
        ^ static_cast<std::uint32_t>(sample + 1) * 9277u
        ^ static_cast<std::uint32_t>(stream + 1) * 26699u
        ^ 0x9e3779b9u;
    return hash_u32(seed);
}

__device__ float random_float01(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state & 0x00ffffffu) * (1.0f / 16777216.0f);
}

__device__ float3 random_in_unit_sphere(std::uint32_t& rng) {
    while (true) {
        const float3 p = make_float3(
            random_float01(rng) * 2.0f - 1.0f,
            random_float01(rng) * 2.0f - 1.0f,
            random_float01(rng) * 2.0f - 1.0f);
        if (length_sq3(p) < 1.0f) {
            return p;
        }
    }
}

__device__ float3 random_unit_vector(std::uint32_t& rng) {
    return normalize3(random_in_unit_sphere(rng));
}

__device__ float3 vector3f_to_float3(const Eigen::Vector3f& v) {
    const float* ptr = v.data();
    return make_float3(ptr[0], ptr[1], ptr[2]);
}

__device__ float reflectance(float cosine, float refraction_index) {
    float r0 = (1.0f - refraction_index) / (1.0f + refraction_index);
    r0 *= r0;
    return r0 + (1.0f - r0) * powf(1.0f - cosine, 5.0f);
}

__device__ bool near_zero3(const float3& v) {
    return fabsf(v.x) < 1e-7f && fabsf(v.y) < 1e-7f && fabsf(v.z) < 1e-7f;
}

__device__ void set_face_normal(const Ray& ray, const float3& outward_normal, HitInfo& hit) {
    hit.front_face = dot3(ray.direction, outward_normal) < 0.0f;
    hit.geometric_normal = outward_normal;
    hit.shading_normal = hit.front_face ? outward_normal : mul3(outward_normal, -1.0f);
}

__device__ void populate_material(const DeviceSceneView& scene, int material_index, HitInfo& hit) {
    if (material_index < 0 || material_index >= scene.material_count || scene.materials == nullptr) {
        hit.base_color = make_float3(0.0f, 0.0f, 0.0f);
        hit.emission = make_float3(0.0f, 0.0f, 0.0f);
        hit.material_type = 0;
        hit.ior = 1.0f;
        hit.fuzz = 0.0f;
        return;
    }

    const MaterialSample& material = scene.materials[material_index];
    hit.material_type = material.type;
    hit.fuzz = material.fuzz;
    hit.ior = material.ior;
    hit.base_color = vector3f_to_float3(material.albedo);
    hit.emission = vector3f_to_float3(material.emission);
    if (hit.material_type == 2) {
        hit.base_color = make_float3(0.92f, 0.95f, 1.0f);
    }
}

__device__ void try_hit_sphere(
    const PackedSphere& sphere, const Ray& ray, float t_min, float t_max, HitInfo& best_hit, bool& found_hit) {
    const float3 center = vector3f_to_float3(sphere.center);
    const float3 oc = sub3(ray.origin, center);
    const float radius = sphere.radius;
    const float a = length_sq3(ray.direction);
    const float half_b = dot3(oc, ray.direction);
    const float c = length_sq3(oc) - radius * radius;
    const float discriminant = half_b * half_b - a * c;
    if (discriminant <= 0.0f) {
        return;
    }
    const float sqrt_disc = sqrtf(discriminant);
    float root = (-half_b - sqrt_disc) / a;
    if (root <= t_min || root >= t_max) {
        root = (-half_b + sqrt_disc) / a;
        if (root <= t_min || root >= t_max) {
            return;
        }
    }

    found_hit = true;
    best_hit.hit = true;
    best_hit.t = root;
    best_hit.position = add3(ray.origin, mul3(ray.direction, root));
    const float3 outward = div3(sub3(best_hit.position, center), radius);
    set_face_normal(ray, outward, best_hit);
}

__device__ void try_hit_quad(
    const PackedQuad& quad, const Ray& ray, float t_min, float t_max, HitInfo& best_hit, bool& found_hit) {
    const float3 origin = vector3f_to_float3(quad.origin);
    const float3 edge_u = vector3f_to_float3(quad.edge_u);
    const float3 edge_v = vector3f_to_float3(quad.edge_v);
    const float3 normal = normalize3(cross3(edge_u, edge_v));
    const float denom = dot3(normal, ray.direction);
    if (fabsf(denom) < 1e-6f) {
        return;
    }

    const float t = dot3(sub3(origin, ray.origin), normal) / denom;
    if (t <= t_min || t >= t_max) {
        return;
    }

    const float3 p = add3(ray.origin, mul3(ray.direction, t));
    const float3 rel = sub3(p, origin);
    const float uu = dot3(edge_u, edge_u);
    const float vv = dot3(edge_v, edge_v);
    if (uu <= 0.0f || vv <= 0.0f) {
        return;
    }
    const float u = dot3(rel, edge_u) / uu;
    const float v = dot3(rel, edge_v) / vv;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
        return;
    }

    found_hit = true;
    best_hit.hit = true;
    best_hit.t = t;
    best_hit.position = p;
    set_face_normal(ray, normal, best_hit);
}

__device__ HitInfo intersect_scene(const DeviceSceneView& scene, const Ray& ray, float t_min, float t_max) {
    HitInfo hit {};
    float closest = t_max;
    bool found = false;

    for (int i = 0; i < scene.sphere_count; ++i) {
        const PackedSphere& sphere = scene.spheres[i];
        HitInfo candidate {};
        bool candidate_hit = false;
        try_hit_sphere(sphere, ray, t_min, closest, candidate, candidate_hit);
        if (!candidate_hit) {
            continue;
        }
        populate_material(scene, sphere.material_index, candidate);
        closest = candidate.t;
        hit = candidate;
        found = true;
    }

    for (int i = 0; i < scene.quad_count; ++i) {
        const PackedQuad& quad = scene.quads[i];
        HitInfo candidate {};
        bool candidate_hit = false;
        try_hit_quad(quad, ray, t_min, closest, candidate, candidate_hit);
        if (!candidate_hit) {
            continue;
        }
        populate_material(scene, quad.material_index, candidate);
        closest = candidate.t;
        hit = candidate;
        found = true;
    }

    hit.hit = found;
    return hit;
}

__device__ bool is_occluded(const DeviceSceneView& scene, const Ray& ray, float max_t) {
    const HitInfo shadow_hit = intersect_scene(scene, ray, kRayEpsilon, max_t);
    return shadow_hit.hit;
}

__device__ void apply_russian_roulette(PathState& state, std::uint32_t& rng) {
    const float p = fminf(fmaxf(max_component3(state.throughput), 0.05f), 0.95f);
    if (random_float01(rng) > p) {
        state.alive = false;
        return;
    }
    state.throughput = div3(state.throughput, p);
}

__device__ void store_output(const LaunchParams& params, int pixel_index, const float3& beauty, const float3& normal,
    const float3& albedo, float depth) {
    if (params.frame.beauty != nullptr) {
        params.frame.beauty[pixel_index] = make_float4(beauty.x, beauty.y, beauty.z, 1.0f);
    }
    if (params.frame.normal != nullptr) {
        const float3 encoded = clamp3(add3(mul3(normal, 0.5f), make_float3(0.5f, 0.5f, 0.5f)), 0.0f, 1.0f);
        params.frame.normal[pixel_index] = make_float4(encoded.x, encoded.y, encoded.z, 1.0f);
    }
    if (params.frame.albedo != nullptr) {
        const float3 clamped = clamp3(albedo, 0.0f, 1.0f);
        params.frame.albedo[pixel_index] = make_float4(clamped.x, clamped.y, clamped.z, 1.0f);
    }
    if (params.frame.depth != nullptr) {
        params.frame.depth[pixel_index] = depth;
    }
}

__global__ void direction_debug_kernel(std::uint8_t* rgba, int width, int height) {
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= width || y >= height) {
        return;
    }

    const int pixel_index = y * width + x;
    rgba[4 * pixel_index + 0] = static_cast<std::uint8_t>((255 * x) / max(width, 1));
    rgba[4 * pixel_index + 1] = static_cast<std::uint8_t>((255 * y) / max(height, 1));
    rgba[4 * pixel_index + 2] = 128;
    rgba[4 * pixel_index + 3] = 255;
}

__global__ void radiance_kernel(const LaunchParams* params_ptr) {
    const LaunchParams& params = *params_ptr;
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= params.width || y >= params.height) {
        return;
    }

    const int pixel_index = y * params.width + x;
    const int spp = params.samples_per_pixel > 0 ? params.samples_per_pixel : 1;

    float3 beauty = make_float3(0.0f, 0.0f, 0.0f);
    float3 normal_sum = make_float3(0.0f, 0.0f, 0.0f);
    float3 albedo_sum = make_float3(0.0f, 0.0f, 0.0f);
    float depth_sum = 0.0f;
    int aux_count = 0;

    for (int sample = 0; sample < spp; ++sample) {
        std::uint32_t rng = rng_for(pixel_index, sample, 0);
        PathState state = trace_primary_ray(params, x, y);
        float3 sample_normal = make_float3(0.0f, 0.0f, 1.0f);
        float3 sample_albedo = make_float3(0.0f, 0.0f, 0.0f);
        float sample_depth = 0.0f;
        bool captured_aux = false;

        const int max_bounces = params.max_bounces > 0 ? params.max_bounces : 1;
        for (int bounce = 0; bounce < max_bounces && state.alive; ++bounce) {
            HitInfo hit = intersect_scene(params.scene, state.ray, kRayEpsilon, kRayFar);
            if (!hit.hit) {
                state.alive = false;
                break;
            }
            if (!captured_aux && bounce == 0) {
                sample_normal = hit.shading_normal;
                sample_albedo = hit.base_color;
                sample_depth = hit.t;
                captured_aux = true;
            }

            state.radiance = add3(state.radiance, mul3(state.throughput, hit.emission));
            accumulate_direct_light(params, hit, state);
            sample_bsdf(params, hit, rng, state);
            if (state.alive && bounce >= params.rr_start_bounce) {
                apply_russian_roulette(state, rng);
            }
        }

        beauty = add3(beauty, state.radiance);
        if (captured_aux) {
            normal_sum = add3(normal_sum, sample_normal);
            albedo_sum = add3(albedo_sum, sample_albedo);
            depth_sum += sample_depth;
            ++aux_count;
        }
    }

    const float inv_spp = 1.0f / static_cast<float>(spp);
    const float3 beauty_avg = mul3(beauty, inv_spp);
    const float inv_aux = aux_count > 0 ? (1.0f / static_cast<float>(aux_count)) : 0.0f;
    const float3 normal_avg = aux_count > 0 ? normalize3(mul3(normal_sum, inv_aux)) : make_float3(0.0f, 0.0f, 1.0f);
    const float3 albedo_avg = aux_count > 0 ? mul3(albedo_sum, inv_aux) : make_float3(0.0f, 0.0f, 0.0f);
    const float depth_avg = aux_count > 0 ? depth_sum * inv_aux : 0.0f;
    store_output(params, pixel_index, beauty_avg, normal_avg, albedo_avg, depth_avg);
}

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA kernel launch failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

dim3 make_grid(int width, int height, const dim3& block_size) {
    return dim3(
        static_cast<unsigned int>((width + static_cast<int>(block_size.x) - 1) / static_cast<int>(block_size.x)),
        static_cast<unsigned int>((height + static_cast<int>(block_size.y) - 1) / static_cast<int>(block_size.y)),
        1);
}

}  // namespace

__device__ PathState trace_primary_ray(const LaunchParams& params, int x, int y) {
    PathState state {};
    state.throughput = make_float3(1.0f, 1.0f, 1.0f);
    state.radiance = make_float3(0.0f, 0.0f, 0.0f);
    state.alive = true;

    const float width = params.width > 0 ? static_cast<float>(params.width) : 1.0f;
    const float height = params.height > 0 ? static_cast<float>(params.height) : 1.0f;
    const float aspect = width / height;
    const float ndc_x = ((static_cast<float>(x) + 0.5f) / width) * 2.0f - 1.0f;
    const float ndc_y = ((static_cast<float>(y) + 0.5f) / height) * 2.0f - 1.0f;
    state.ray.origin = make_float3(0.0f, 0.0f, 0.0f);
    state.ray.direction = normalize3(make_float3(ndc_x * aspect, -ndc_y, -1.0f));
    return state;
}

__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit, PathState& state) {
    if (hit.material_type == 3) {
        return;
    }

    const float3 surface_point = add3(hit.position, mul3(hit.shading_normal, kRayEpsilon * 2.0f));
    float3 direct = make_float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < params.scene.sphere_count; ++i) {
        const PackedSphere& sphere = params.scene.spheres[i];
        if (sphere.material_index < 0 || sphere.material_index >= params.scene.material_count) {
            continue;
        }
        const MaterialSample& material = params.scene.materials[sphere.material_index];
        if (material.type != 3) {
            continue;
        }
        const float3 emission = vector3f_to_float3(material.emission);
        const float3 light_pos = vector3f_to_float3(sphere.center);
        const float3 to_light = sub3(light_pos, surface_point);
        const float dist_sq = fmaxf(length_sq3(to_light), 1e-6f);
        const float dist = sqrtf(dist_sq);
        const float3 light_dir = div3(to_light, dist);
        const float n_dot_l = fmaxf(dot3(hit.shading_normal, light_dir), 0.0f);
        if (n_dot_l <= 0.0f) {
            continue;
        }
        Ray shadow_ray {};
        shadow_ray.origin = surface_point;
        shadow_ray.direction = light_dir;
        if (is_occluded(params.scene, shadow_ray, dist - kRayEpsilon)) {
            continue;
        }
        const float attenuation = kShadowScale * n_dot_l / dist_sq;
        direct = add3(direct, mul3(emission, attenuation));
    }

    for (int i = 0; i < params.scene.quad_count; ++i) {
        const PackedQuad& quad = params.scene.quads[i];
        if (quad.material_index < 0 || quad.material_index >= params.scene.material_count) {
            continue;
        }
        const MaterialSample& material = params.scene.materials[quad.material_index];
        if (material.type != 3) {
            continue;
        }
        const float3 emission = vector3f_to_float3(material.emission);
        const float3 origin = vector3f_to_float3(quad.origin);
        const float3 edge_u = vector3f_to_float3(quad.edge_u);
        const float3 edge_v = vector3f_to_float3(quad.edge_v);
        const float3 light_pos = add3(origin, mul3(add3(edge_u, edge_v), 0.5f));
        const float3 to_light = sub3(light_pos, surface_point);
        const float dist_sq = fmaxf(length_sq3(to_light), 1e-6f);
        const float dist = sqrtf(dist_sq);
        const float3 light_dir = div3(to_light, dist);
        const float n_dot_l = fmaxf(dot3(hit.shading_normal, light_dir), 0.0f);
        if (n_dot_l <= 0.0f) {
            continue;
        }
        Ray shadow_ray {};
        shadow_ray.origin = surface_point;
        shadow_ray.direction = light_dir;
        if (is_occluded(params.scene, shadow_ray, dist - kRayEpsilon)) {
            continue;
        }
        const float attenuation = kShadowScale * n_dot_l / dist_sq;
        direct = add3(direct, mul3(emission, attenuation));
    }

    state.radiance = add3(state.radiance, mul3(state.throughput, mul3(hit.base_color, direct)));
}

__device__ void sample_bsdf(const LaunchParams&, const HitInfo& hit, std::uint32_t& rng, PathState& state) {
    if (!state.alive) {
        return;
    }

    if (hit.material_type == 3) {
        state.alive = false;
        return;
    }

    if (hit.material_type == 0) {
        float3 scatter_dir = add3(hit.shading_normal, random_unit_vector(rng));
        if (near_zero3(scatter_dir)) {
            scatter_dir = hit.shading_normal;
        }
        state.ray.origin = add3(hit.position, mul3(hit.shading_normal, kRayEpsilon));
        state.ray.direction = normalize3(scatter_dir);
        state.throughput = mul3(state.throughput, hit.base_color);
        return;
    }

    if (hit.material_type == 1) {
        const float3 reflected = reflect3(normalize3(state.ray.direction), hit.shading_normal);
        const float3 fuzz = mul3(random_in_unit_sphere(rng), fminf(fmaxf(hit.fuzz, 0.0f), 1.0f));
        const float3 scattered = normalize3(add3(reflected, fuzz));
        if (dot3(scattered, hit.shading_normal) <= 0.0f) {
            state.alive = false;
            return;
        }
        state.ray.origin = add3(hit.position, mul3(hit.shading_normal, kRayEpsilon));
        state.ray.direction = scattered;
        state.throughput = mul3(state.throughput, hit.base_color);
        return;
    }

    if (hit.material_type == 2) {
        const float refraction_ratio = hit.front_face ? (1.0f / fmaxf(hit.ior, 1e-4f)) : fmaxf(hit.ior, 1e-4f);
        const float3 unit_dir = normalize3(state.ray.direction);
        const float cos_theta = fminf(dot3(mul3(unit_dir, -1.0f), hit.shading_normal), 1.0f);
        const float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
        const bool cannot_refract = refraction_ratio * sin_theta > 1.0f;
        float3 direction;
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float01(rng)) {
            direction = reflect3(unit_dir, hit.shading_normal);
        } else {
            direction = refract3(unit_dir, hit.shading_normal, refraction_ratio);
        }
        state.ray.origin = add3(hit.position, mul3(direction, kRayEpsilon));
        state.ray.direction = normalize3(direction);
        state.throughput = mul3(state.throughput, hit.base_color);
        return;
    }

    state.alive = false;
}

void launch_direction_debug_kernel(std::uint8_t* rgba, int width, int height, cudaStream_t stream) {
    const dim3 block_size(16, 16, 1);
    direction_debug_kernel<<<make_grid(width, height, block_size), block_size, 0, stream>>>(rgba, width, height);
    throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
}

void launch_radiance_kernel(const LaunchParams& params, cudaStream_t stream) {
    LaunchParams* device_params = nullptr;
    throw_cuda_error(cudaMalloc(reinterpret_cast<void**>(&device_params), sizeof(LaunchParams)), "cudaMalloc()");
    try {
        throw_cuda_error(cudaMemcpyAsync(
            device_params, &params, sizeof(LaunchParams), cudaMemcpyHostToDevice, stream), "cudaMemcpyAsync()");
        const dim3 block_size(8, 8, 1);
        radiance_kernel<<<make_grid(params.width, params.height, block_size), block_size, 0, stream>>>(device_params);
        throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
        throw_cuda_error(cudaFree(device_params), "cudaFree()");
    } catch (...) {
        cudaFree(device_params);
        throw;
    }
}

}  // namespace rt
