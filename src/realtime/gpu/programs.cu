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
    float tex_u = 0.0f;
    float tex_v = 0.0f;
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
__device__ float3 scene_background(const LaunchParams& params);
__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit, PathState& state);
__device__ void sample_bsdf(const LaunchParams& params, const HitInfo& hit, std::uint32_t& rng, PathState& state);

__device__ float3 project_pinhole32(const DevicePinhole32Params& params, const float3& dir_camera);
__device__ float3 project_equi62_lut1d(const DeviceEqui62Lut1DParams& params, const float3& dir_camera);
__device__ float2 project_camera_pixel(const DeviceActiveCamera& camera, const float3& dir_camera);
__device__ float3 decode_normal(const float4& encoded);

namespace {

constexpr float kRayEpsilon = 1e-3f;
constexpr float kRayFar = 1e30f;
constexpr float kShadowScale = 0.8f;
constexpr float kPi = 3.14159265358979323846f;
constexpr int kTextureResolveDepth = 8;
constexpr double kCameraEpsilon = 1e-12;
constexpr int kPinholeUndistortMaxIterations = 24;
constexpr double kPinholeUndistortResidualThresholdSq = 1e-20;

struct Double2 {
    double x = 0.0;
    double y = 0.0;
};

__device__ std::uint32_t hash_u32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

__device__ std::uint32_t rng_for(int pixel_index, int sample, std::uint32_t stream) {
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

__device__ float3 packed_medium_world_to_local_point(const PackedMedium& medium, const float3& point) {
    const float3 shifted = sub3(point, vector3f_to_float3(medium.translation));
    return make_float3(
        dot3(vector3f_to_float3(medium.rotation_row0), shifted),
        dot3(vector3f_to_float3(medium.rotation_row1), shifted),
        dot3(vector3f_to_float3(medium.rotation_row2), shifted));
}

__device__ float3 packed_medium_world_to_local_dir(const PackedMedium& medium, const float3& dir) {
    return make_float3(
        dot3(vector3f_to_float3(medium.rotation_row0), dir),
        dot3(vector3f_to_float3(medium.rotation_row1), dir),
        dot3(vector3f_to_float3(medium.rotation_row2), dir));
}

__device__ float clamp01(float v) {
    return fminf(fmaxf(v, 0.0f), 1.0f);
}

__device__ std::uint8_t encode_direction_channel(float value) {
    return static_cast<std::uint8_t>(255.0f * clamp01(0.5f * (value + 1.0f)));
}

__device__ float fractf(float v) {
    return v - floorf(v);
}

__device__ void sphere_uv(const float3& unit_p, float& u, float& v) {
    const float theta = acosf(fminf(fmaxf(-unit_p.y, -1.0f), 1.0f));
    const float phi = atan2f(-unit_p.z, unit_p.x) + kPi;
    u = phi / (2.0f * kPi);
    v = theta / kPi;
}

__device__ float hash_noise(const float3& p) {
    const float n = sinf(dot3(p, make_float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f;
    return fractf(n) * 2.0f - 1.0f;
}

__device__ float turbulence_noise(float3 p) {
    float accum = 0.0f;
    float weight = 1.0f;
    for (int i = 0; i < 7; ++i) {
        accum += weight * hash_noise(p);
        p = mul3(p, 2.0f);
        weight *= 0.5f;
    }
    return fabsf(accum);
}

__device__ float3 sample_image_texture(const DeviceSceneView& scene, const PackedTexture& texture, float u, float v) {
    if (scene.image_texels == nullptr || texture.image_width <= 0 || texture.image_height <= 0) {
        return make_float3(0.0f, 1.0f, 1.0f);
    }

    const float clamped_u = clamp01(u);
    const float clamped_v = clamp01(1.0f - v);
    int i = static_cast<int>(clamped_u * static_cast<float>(texture.image_width));
    int j = static_cast<int>(clamped_v * static_cast<float>(texture.image_height));
    if (i >= texture.image_width) {
        i = texture.image_width - 1;
    }
    if (j >= texture.image_height) {
        j = texture.image_height - 1;
    }
    const int pixel_index = texture.image_offset + j * texture.image_width + i;
    if (pixel_index < 0 || pixel_index >= scene.image_texel_count) {
        return make_float3(0.0f, 1.0f, 1.0f);
    }
    return vector3f_to_float3(scene.image_texels[pixel_index]);
}

__device__ float3 evaluate_texture(const DeviceSceneView& scene, int texture_index, float u, float v, const float3& p) {
    int index = texture_index;
    for (int depth = 0; depth < kTextureResolveDepth; ++depth) {
        if (index < 0 || index >= scene.texture_count || scene.textures == nullptr) {
            return make_float3(1.0f, 0.0f, 1.0f);
        }
        const PackedTexture& texture = scene.textures[index];
        if (texture.type == 0) {
            return vector3f_to_float3(texture.color);
        }
        if (texture.type == 1) {
            if (fabsf(texture.scale) < 1e-8f) {
                index = texture.even_texture;
                continue;
            }
            const int x = static_cast<int>(floorf(p.x / texture.scale));
            const int y = static_cast<int>(floorf(p.y / texture.scale));
            const int z = static_cast<int>(floorf(p.z / texture.scale));
            const bool is_even = ((x + y + z) % 2) == 0;
            index = is_even ? texture.even_texture : texture.odd_texture;
            continue;
        }
        if (texture.type == 2) {
            return sample_image_texture(scene, texture, u, v);
        }
        if (texture.type == 3) {
            const float turb = turbulence_noise(p);
            const float value = 0.5f * (1.0f + sinf(texture.scale * p.z + 10.0f * turb));
            return make_float3(value, value, value);
        }
        return make_float3(1.0f, 0.0f, 1.0f);
    }
    return make_float3(1.0f, 0.0f, 1.0f);
}

__device__ Double2 make_double2_xy(double x, double y) {
    return Double2 {x, y};
}

__device__ Double2 distort_pinhole_normalized(const DevicePinhole32Params& params, const Double2& xy) {
    const double x = xy.x;
    const double y = xy.y;
    const double x2 = x * x;
    const double y2 = y * y;
    const double xy_term = x * y;
    const double r2 = x2 + y2;
    const double r4 = r2 * r2;
    const double r6 = r4 * r2;
    const double radial = 1.0 + params.k1 * r2 + params.k2 * r4 + params.k3 * r6;
    return make_double2_xy(
        x * radial + params.p2 * (r2 + 2.0 * x2) + 2.0 * params.p1 * xy_term,
        y * radial + params.p1 * (r2 + 2.0 * y2) + 2.0 * params.p2 * xy_term);
}

__device__ bool solve_2x2(
    double a00, double a01, double a10, double a11, const Double2& rhs, Double2& delta) {
    const double det = a00 * a11 - a01 * a10;
    if (fabs(det) < kCameraEpsilon) {
        return false;
    }
    const double inv_det = 1.0 / det;
    delta.x = (a11 * rhs.x - a01 * rhs.y) * inv_det;
    delta.y = (-a10 * rhs.x + a00 * rhs.y) * inv_det;
    return true;
}

__device__ float3 unproject_pinhole32(const DevicePinhole32Params& params, double pixel_x, double pixel_y) {
    const Double2 xy_distorted = make_double2_xy(
        (pixel_x - params.cx) / params.fx,
        (pixel_y - params.cy) / params.fy);
    Double2 xy = xy_distorted;
    for (int iter = 0; iter < kPinholeUndistortMaxIterations; ++iter) {
        const Double2 distorted = distort_pinhole_normalized(params, xy);
        const Double2 error = make_double2_xy(xy_distorted.x - distorted.x, xy_distorted.y - distorted.y);
        const double error_sq = error.x * error.x + error.y * error.y;
        if (error_sq < kPinholeUndistortResidualThresholdSq) {
            break;
        }

        const double x = xy.x;
        const double y = xy.y;
        const double r2 = x * x + y * y;
        const double r4 = r2 * r2;
        const double r6 = r4 * r2;
        const double radial = 1.0 + params.k1 * r2 + params.k2 * r4 + params.k3 * r6;
        const double d_radial_dx = 2.0 * x * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);
        const double d_radial_dy = 2.0 * y * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);

        Double2 delta {};
        if (!solve_2x2(
                radial + x * d_radial_dx + 6.0 * params.p2 * x + 2.0 * params.p1 * y,
                x * d_radial_dy + 2.0 * params.p1 * x + 2.0 * params.p2 * y,
                y * d_radial_dx + 2.0 * params.p1 * x + 2.0 * params.p2 * y,
                radial + y * d_radial_dy + 6.0 * params.p1 * y + 2.0 * params.p2 * x,
                error, delta)) {
            break;
        }
        xy.x += delta.x;
        xy.y += delta.y;
    }

    return normalize3(make_float3(static_cast<float>(xy.x), static_cast<float>(xy.y), 1.0f));
}

__device__ Double2 apply_equi_tangential(const Double2& xy, const double tangential[2]) {
    const double xr = xy.x;
    const double yr = xy.y;
    const double xr2 = xr * xr;
    const double yr2 = yr * yr;
    const double xryr = xr * yr;
    const double p1 = tangential[0];
    const double p2 = tangential[1];
    return make_double2_xy(
        xr + 2.0 * p1 * xryr + p2 * (xr2 + yr2 + 2.0 * xr2),
        yr + p1 * (xr2 + yr2 + 2.0 * yr2) + 2.0 * p2 * xryr);
}

__device__ Double2 remove_equi_tangential_single_step(const Double2& xy_distorted, const double tangential[2]) {
    const Double2 distorted = apply_equi_tangential(xy_distorted, tangential);
    Double2 delta {};
    if (!solve_2x2(
            1.0 + 2.0 * tangential[0] * xy_distorted.y + 6.0 * tangential[1] * xy_distorted.x,
            2.0 * tangential[0] * xy_distorted.x + 2.0 * tangential[1] * xy_distorted.y,
            2.0 * tangential[0] * xy_distorted.x + 2.0 * tangential[1] * xy_distorted.y,
            1.0 + 6.0 * tangential[0] * xy_distorted.y + 2.0 * tangential[1] * xy_distorted.x,
            make_double2_xy(xy_distorted.x - distorted.x, xy_distorted.y - distorted.y), delta)) {
        return xy_distorted;
    }
    return make_double2_xy(xy_distorted.x + delta.x, xy_distorted.y + delta.y);
}

__device__ bool interpolate_lut_theta(const DeviceEqui62Lut1DParams& params, double rd, double& theta) {
    if (rd <= 0.0 || params.lut_step <= 0.0) {
        theta = 0.0;
        return true;
    }
    const double position = rd / params.lut_step;
    const double max_index = 1023.0;
    if (position > max_index) {
        return false;
    }
    if (position >= max_index) {
        theta = params.lut[1023];
        return true;
    }
    const int index = static_cast<int>(position);
    const double alpha = position - static_cast<double>(index);
    theta = (1.0 - alpha) * params.lut[index] + alpha * params.lut[index + 1];
    return true;
}

__device__ float3 normalized_fallback_ray(const Double2& xy) {
    return normalize3(make_float3(static_cast<float>(xy.x), static_cast<float>(xy.y), 1.0f));
}

__device__ float3 unproject_equi62_lut1d(const DeviceEqui62Lut1DParams& params, double pixel_x, double pixel_y) {
    const Double2 xy = make_double2_xy(
        (pixel_x - params.cx) / params.fx,
        (pixel_y - params.cy) / params.fy);
    if (xy.x * xy.x + xy.y * xy.y < kCameraEpsilon * kCameraEpsilon) {
        return make_float3(0.0f, 0.0f, 1.0f);
    }

    const Double2 xy_radial = remove_equi_tangential_single_step(xy, params.tangential);
    const double rd = sqrt(xy_radial.x * xy_radial.x + xy_radial.y * xy_radial.y);
    if (rd < kCameraEpsilon) {
        return make_float3(0.0f, 0.0f, 1.0f);
    }

    double theta = 0.0;
    if (!interpolate_lut_theta(params, rd, theta)) {
        return normalized_fallback_ray(xy);
    }

    const double scale = tan(theta) / rd;
    return normalize3(make_float3(
        static_cast<float>(xy_radial.x * scale),
        static_cast<float>(xy_radial.y * scale),
        1.0f));
}

__device__ float3 unproject_camera_ray(const DeviceActiveCamera& camera, double pixel_x, double pixel_y) {
    if (camera.model == CameraModelType::equi62_lut1d) {
        return unproject_equi62_lut1d(camera.equi, pixel_x, pixel_y);
    }
    return unproject_pinhole32(camera.pinhole, pixel_x, pixel_y);
}

__device__ float3 transform_direction(const DeviceActiveCamera& camera, const float3& dir_camera) {
    return normalize3(add3(
        add3(
            mul3(make_float3(static_cast<float>(camera.basis_x[0]), static_cast<float>(camera.basis_x[1]),
                     static_cast<float>(camera.basis_x[2])),
                dir_camera.x),
            mul3(make_float3(static_cast<float>(camera.basis_y[0]), static_cast<float>(camera.basis_y[1]),
                     static_cast<float>(camera.basis_y[2])),
                dir_camera.y)),
        mul3(make_float3(static_cast<float>(camera.basis_z[0]), static_cast<float>(camera.basis_z[1]),
                 static_cast<float>(camera.basis_z[2])),
            dir_camera.z)));
}

__device__ float3 camera_origin(const DeviceActiveCamera& camera) {
    return make_float3(
        static_cast<float>(camera.origin[0]),
        static_cast<float>(camera.origin[1]),
        static_cast<float>(camera.origin[2]));
}

__device__ float3 sample_sphere_light_point(const PackedSphere& sphere, const float3& surface_point) {
    const float3 center = vector3f_to_float3(sphere.center);
    float3 to_surface = sub3(surface_point, center);
    if (length_sq3(to_surface) <= 1e-8f) {
        to_surface = make_float3(0.0f, 1.0f, 0.0f);
    }
    return add3(center, mul3(normalize3(to_surface), sphere.radius));
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
    hit.base_color = evaluate_texture(scene, material.albedo_texture, hit.tex_u, hit.tex_v, hit.position);
    hit.emission = evaluate_texture(scene, material.emission_texture, hit.tex_u, hit.tex_v, hit.position);
    if (hit.material_type == 2) {
        hit.base_color = make_float3(0.92f, 0.95f, 1.0f);
        hit.emission = make_float3(0.0f, 0.0f, 0.0f);
    }
    if (hit.material_type == 3 && !hit.front_face) {
        hit.emission = make_float3(0.0f, 0.0f, 0.0f);
    }
    if (hit.material_type != 3) {
        hit.emission = make_float3(0.0f, 0.0f, 0.0f);
    }
}

__device__ bool hit_medium_boundary(
    const PackedMedium& medium, const Ray& ray, float t_min, float t_max, float& entry_t, float& exit_t) {
    const float3 local_origin = packed_medium_world_to_local_point(medium, ray.origin);
    const float3 local_direction = packed_medium_world_to_local_dir(medium, ray.direction);

    if (medium.boundary_type == 0) {
        const float3 center = vector3f_to_float3(medium.local_center_or_min);
        const float3 oc = sub3(local_origin, center);
        const float a = length_sq3(local_direction);
        const float half_b = dot3(oc, local_direction);
        const float c = length_sq3(oc) - medium.radius * medium.radius;
        const float discriminant = half_b * half_b - a * c;
        if (discriminant <= 0.0f) {
            return false;
        }
        const float sqrt_disc = sqrtf(discriminant);
        float t0 = (-half_b - sqrt_disc) / a;
        float t1 = (-half_b + sqrt_disc) / a;
        if (t0 > t1) {
            const float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        entry_t = fmaxf(t0, t_min);
        exit_t = fminf(t1, t_max);
        return entry_t < exit_t;
    }

    if (medium.boundary_type == 1) {
        const float3 box_min = vector3f_to_float3(medium.local_center_or_min);
        const float3 box_max = vector3f_to_float3(medium.local_max);
        float t0 = t_min;
        float t1 = t_max;
        const float origin[3] {local_origin.x, local_origin.y, local_origin.z};
        const float direction[3] {local_direction.x, local_direction.y, local_direction.z};
        const float min_v[3] {box_min.x, box_min.y, box_min.z};
        const float max_v[3] {box_max.x, box_max.y, box_max.z};
        for (int axis = 0; axis < 3; ++axis) {
            if (fabsf(direction[axis]) < 1e-8f) {
                if (origin[axis] < min_v[axis] || origin[axis] > max_v[axis]) {
                    return false;
                }
                continue;
            }
            float axis_t0 = (min_v[axis] - origin[axis]) / direction[axis];
            float axis_t1 = (max_v[axis] - origin[axis]) / direction[axis];
            if (axis_t0 > axis_t1) {
                const float tmp = axis_t0;
                axis_t0 = axis_t1;
                axis_t1 = tmp;
            }
            t0 = fmaxf(t0, axis_t0);
            t1 = fminf(t1, axis_t1);
            if (t0 >= t1) {
                return false;
            }
        }
        entry_t = t0;
        exit_t = t1;
        return true;
    }

    return false;
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
    sphere_uv(outward, best_hit.tex_u, best_hit.tex_v);
    set_face_normal(ray, outward, best_hit);
}

__device__ void try_hit_quad(
    const PackedQuad& quad, const Ray& ray, float t_min, float t_max, HitInfo& best_hit, bool& found_hit) {
    const float3 origin = vector3f_to_float3(quad.origin);
    const float3 edge_u = vector3f_to_float3(quad.edge_u);
    const float3 edge_v = vector3f_to_float3(quad.edge_v);
    const float3 n = cross3(edge_u, edge_v);
    const float n_len_sq = length_sq3(n);
    if (n_len_sq <= 1e-12f) {
        return;
    }
    const float3 normal = div3(n, sqrtf(n_len_sq));
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
    const float3 w = div3(n, n_len_sq);
    const float u = dot3(w, cross3(rel, edge_v));
    const float v = dot3(w, cross3(edge_u, rel));
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
        return;
    }

    found_hit = true;
    best_hit.hit = true;
    best_hit.t = t;
    best_hit.position = p;
    best_hit.tex_u = u;
    best_hit.tex_v = v;
    set_face_normal(ray, normal, best_hit);
}

__device__ void try_hit_triangle(
    const PackedTriangle& triangle, const Ray& ray, float t_min, float t_max, HitInfo& best_hit, bool& found_hit) {
    const float3 p0 = vector3f_to_float3(triangle.p0);
    const float3 p1 = vector3f_to_float3(triangle.p1);
    const float3 p2 = vector3f_to_float3(triangle.p2);
    const float3 edge1 = sub3(p1, p0);
    const float3 edge2 = sub3(p2, p0);
    const float3 pvec = cross3(ray.direction, edge2);
    const float det = dot3(edge1, pvec);
    if (fabsf(det) <= 1e-8f) {
        return;
    }

    const float inv_det = 1.0f / det;
    const float3 tvec = sub3(ray.origin, p0);
    const float u = dot3(tvec, pvec) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return;
    }

    const float3 qvec = cross3(tvec, edge1);
    const float v = dot3(ray.direction, qvec) * inv_det;
    if (v < 0.0f || (u + v) > 1.0f) {
        return;
    }

    const float t = dot3(edge2, qvec) * inv_det;
    if (t <= t_min || t >= t_max) {
        return;
    }

    const float3 normal = normalize3(cross3(edge1, edge2));
    found_hit = true;
    best_hit.hit = true;
    best_hit.t = t;
    best_hit.position = add3(ray.origin, mul3(ray.direction, t));
    best_hit.tex_u = u;
    best_hit.tex_v = v;
    set_face_normal(ray, normal, best_hit);
}

__device__ HitInfo intersect_scene(
    const DeviceSceneView& scene, const Ray& ray, float t_min, float t_max, std::uint32_t* rng = nullptr) {
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

    for (int i = 0; i < scene.triangle_count; ++i) {
        const PackedTriangle& triangle = scene.triangles[i];
        HitInfo candidate {};
        bool candidate_hit = false;
        try_hit_triangle(triangle, ray, t_min, closest, candidate, candidate_hit);
        if (!candidate_hit) {
            continue;
        }
        populate_material(scene, triangle.material_index, candidate);
        closest = candidate.t;
        hit = candidate;
        found = true;
    }

    if (rng != nullptr) {
        const float ray_length = length3(ray.direction);
        if (ray_length > 1e-8f) {
            for (int i = 0; i < scene.medium_count; ++i) {
                const PackedMedium& medium = scene.media[i];
                if (medium.density <= 0.0f) {
                    continue;
                }
                float entry_t = 0.0f;
                float exit_t = 0.0f;
                if (!hit_medium_boundary(medium, ray, t_min, closest, entry_t, exit_t)) {
                    continue;
                }
                entry_t = fmaxf(entry_t, t_min);
                exit_t = fminf(exit_t, closest);
                if (entry_t >= exit_t) {
                    continue;
                }
                const float distance_inside = (exit_t - entry_t) * ray_length;
                const float sample = fmaxf(random_float01(*rng), 1e-6f);
                const float hit_distance = -logf(sample) / medium.density;
                if (hit_distance >= distance_inside) {
                    continue;
                }

                HitInfo candidate {};
                candidate.hit = true;
                candidate.t = entry_t + hit_distance / ray_length;
                candidate.position = add3(ray.origin, mul3(ray.direction, candidate.t));
                candidate.front_face = true;
                candidate.geometric_normal = mul3(normalize3(ray.direction), -1.0f);
                candidate.shading_normal = candidate.geometric_normal;
                candidate.tex_u = 0.0f;
                candidate.tex_v = 0.0f;
                populate_material(scene, medium.material_index, candidate);
                closest = candidate.t;
                hit = candidate;
                found = true;
            }
        }
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

__global__ void direction_debug_kernel(const DeviceActiveCamera* camera_ptr, std::uint8_t* rgba, int width, int height) {
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (camera_ptr == nullptr || x >= width || y >= height) {
        return;
    }

    const int pixel_index = y * width + x;
    const DeviceActiveCamera& camera = *camera_ptr;
    const float pixel_x = static_cast<float>(x) + 0.5f;
    const float pixel_y = static_cast<float>(y) + 0.5f;
    const float3 dir_camera = unproject_camera_ray(camera, pixel_x, pixel_y);
    const float3 dir_world = transform_direction(camera, dir_camera);

    rgba[4 * pixel_index + 0] = encode_direction_channel(dir_world.x);
    rgba[4 * pixel_index + 1] = encode_direction_channel(dir_world.y);
    rgba[4 * pixel_index + 2] = encode_direction_channel(dir_world.z);
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
        std::uint32_t rng = rng_for(pixel_index, sample, params.sample_stream);
        PathState state = trace_primary_ray(params, x, y);
        float3 sample_normal = make_float3(0.0f, 0.0f, 1.0f);
        float3 sample_albedo = make_float3(0.0f, 0.0f, 0.0f);
        float sample_depth = 0.0f;
        bool captured_aux = false;

        const int max_bounces = params.max_bounces > 0 ? params.max_bounces : 1;
        for (int bounce = 0; bounce < max_bounces && state.alive; ++bounce) {
            HitInfo hit = intersect_scene(params.scene, state.ray, kRayEpsilon, kRayFar, &rng);
            if (!hit.hit) {
                state.radiance = add3(state.radiance, mul3(state.throughput, scene_background(params)));
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

    const float pixel_x = static_cast<float>(x) + 0.5f;
    const float pixel_y = static_cast<float>(y) + 0.5f;
    const DeviceActiveCamera& camera = params.active_camera;
    const float3 dir_camera = unproject_camera_ray(camera, pixel_x, pixel_y);
    state.ray.origin = camera_origin(camera);
    state.ray.direction = transform_direction(camera, dir_camera);
    return state;
}

__device__ float3 scene_background(const LaunchParams& params) {
    return make_float3(params.background[0], params.background[1], params.background[2]);
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
        const float3 light_pos = sample_sphere_light_point(sphere, surface_point);
        const float3 center = vector3f_to_float3(sphere.center);
        float light_u = 0.0f;
        float light_v = 0.0f;
        sphere_uv(normalize3(sub3(light_pos, center)), light_u, light_v);
        const float3 emission =
            evaluate_texture(params.scene, material.emission_texture, light_u, light_v, light_pos);
        const float3 to_light = sub3(light_pos, surface_point);
        const float dist_sq = fmaxf(length_sq3(to_light), 1e-6f);
        const float dist = sqrtf(dist_sq);
        const float3 light_dir = div3(to_light, dist);
        const bool isotropic = hit.material_type == 4;
        const float n_dot_l = isotropic ? (1.0f / (4.0f * kPi)) : fmaxf(dot3(hit.shading_normal, light_dir), 0.0f);
        if (!isotropic && n_dot_l <= 0.0f) {
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
        const float3 origin = vector3f_to_float3(quad.origin);
        const float3 edge_u = vector3f_to_float3(quad.edge_u);
        const float3 edge_v = vector3f_to_float3(quad.edge_v);
        const float3 light_pos = add3(origin, mul3(add3(edge_u, edge_v), 0.5f));
        const float3 emission =
            evaluate_texture(params.scene, material.emission_texture, 0.5f, 0.5f, light_pos);
        const float3 to_light = sub3(light_pos, surface_point);
        const float dist_sq = fmaxf(length_sq3(to_light), 1e-6f);
        const float dist = sqrtf(dist_sq);
        const float3 light_dir = div3(to_light, dist);
        const float3 light_normal = normalize3(cross3(edge_u, edge_v));
        if (dot3(light_normal, light_dir) >= 0.0f) {
            continue;
        }
        const bool isotropic = hit.material_type == 4;
        const float n_dot_l = isotropic ? (1.0f / (4.0f * kPi)) : fmaxf(dot3(hit.shading_normal, light_dir), 0.0f);
        if (!isotropic && n_dot_l <= 0.0f) {
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

    for (int i = 0; i < params.scene.triangle_count; ++i) {
        const PackedTriangle& triangle = params.scene.triangles[i];
        if (triangle.material_index < 0 || triangle.material_index >= params.scene.material_count) {
            continue;
        }
        const MaterialSample& material = params.scene.materials[triangle.material_index];
        if (material.type != 3) {
            continue;
        }
        const float3 p0 = vector3f_to_float3(triangle.p0);
        const float3 p1 = vector3f_to_float3(triangle.p1);
        const float3 p2 = vector3f_to_float3(triangle.p2);
        const float3 edge1 = sub3(p1, p0);
        const float3 edge2 = sub3(p2, p0);
        const float3 light_pos = div3(add3(add3(p0, p1), p2), 3.0f);
        const float3 emission =
            evaluate_texture(params.scene, material.emission_texture, 1.0f / 3.0f, 1.0f / 3.0f, light_pos);
        const float3 to_light = sub3(light_pos, surface_point);
        const float dist_sq = fmaxf(length_sq3(to_light), 1e-6f);
        const float dist = sqrtf(dist_sq);
        const float3 light_dir = div3(to_light, dist);
        const float3 light_normal = normalize3(cross3(edge1, edge2));
        if (dot3(light_normal, light_dir) >= 0.0f) {
            continue;
        }
        const bool isotropic = hit.material_type == 4;
        const float n_dot_l = isotropic ? (1.0f / (4.0f * kPi)) : fmaxf(dot3(hit.shading_normal, light_dir), 0.0f);
        if (!isotropic && n_dot_l <= 0.0f) {
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

    if (hit.material_type == 4) {
        state.ray.origin = add3(hit.position, mul3(random_unit_vector(rng), kRayEpsilon));
        state.ray.direction = random_unit_vector(rng);
        state.throughput = mul3(state.throughput, hit.base_color);
        return;
    }

    state.alive = false;
}

void launch_direction_debug_kernel(
    const DeviceActiveCamera& camera, std::uint8_t* rgba, int width, int height, cudaStream_t stream) {
    DeviceActiveCamera* device_camera = nullptr;
    throw_cuda_error(cudaMalloc(reinterpret_cast<void**>(&device_camera), sizeof(DeviceActiveCamera)), "cudaMalloc()");
    const dim3 block_size(16, 16, 1);
    try {
        throw_cuda_error(cudaMemcpyAsync(
            device_camera, &camera, sizeof(DeviceActiveCamera), cudaMemcpyHostToDevice, stream), "cudaMemcpyAsync()");
        direction_debug_kernel<<<make_grid(width, height, block_size), block_size, 0, stream>>>(
            device_camera, rgba, width, height);
        throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
        throw_cuda_error(cudaFree(device_camera), "cudaFree()");
    } catch (...) {
        cudaFree(device_camera);
        throw;
    }
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
