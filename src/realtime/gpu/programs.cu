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
    int openpbr_index = -1;
    int material_type = 0;
    int primitive_type = -1;
    int primitive_index = -1;
    bool front_face = true;
};

struct AnalyticLightHit {
    bool hit = false;
    float t = 0.0f;
    float3 position = make_float3(0.0f, 0.0f, 0.0f);
    float3 normal = make_float3(0.0f, 0.0f, 1.0f);
    int light_index = -1;
};

struct PathState {
    float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
    float3 radiance = make_float3(0.0f, 0.0f, 0.0f);
    OpenPbrTransportContext openpbr_context {};
    OpenPbrSubsurfaceMedium subsurface_medium {};
    int subsurface_material_index = -1;
    float3 previous_scatter_position = make_float3(0.0f, 0.0f, 0.0f);
    float3 previous_scatter_normal = make_float3(0.0f, 0.0f, 1.0f);
    float previous_bsdf_pdf = 0.0f;
    int previous_material_type = -1;
    int previous_primitive_type = -1;
    int previous_primitive_index = -1;
    bool previous_scatter_valid = false;
    bool previous_scatter_delta = true;
    Ray ray {};
    bool alive = true;
};

struct DirectLightSample {
    float3 position = make_float3(0.0f, 0.0f, 0.0f);
    float3 normal = make_float3(0.0f, 0.0f, 1.0f);
    float3 direction = make_float3(0.0f, 0.0f, 1.0f);
    float3 emission = make_float3(0.0f, 0.0f, 0.0f);
    float distance = 0.0f;
    float pdf = 0.0f;
    int material_index = -1;
    int light_index = -1;
    bool infinite = false;
    bool delta = false;
    bool valid = false;
};

__device__ PathState trace_primary_ray(const LaunchParams& params, int x, int y);
__device__ float3 scene_background(const LaunchParams& params);
__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit,
    std::uint32_t& rng, bool bsdf_technique_available, PathState& state);
__device__ void accumulate_analytic_direct_light(const LaunchParams& params, const HitInfo& hit,
    std::uint32_t& rng, bool bsdf_technique_available, PathState& state);
__device__ void accumulate_restir_analytic_direct_light(const LaunchParams& params,
    const HitInfo& hit, int x, int y, std::uint32_t& rng, bool bsdf_technique_available,
    PathState& state);
__device__ void sample_bsdf(const LaunchParams& params, const HitInfo& hit, std::uint32_t& rng,
    PathState& state);

namespace {

constexpr float kRayEpsilon = 1e-3f;
constexpr float kRayFar = 1e30f;
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
                               ^ static_cast<std::uint32_t>(stream + 1) * 26699u ^ 0x9e3779b9u;
    return hash_u32(seed);
}

__device__ float random_float01(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state & 0x00ffffffu) * (1.0f / 16777216.0f);
}

__device__ float3 random_in_unit_sphere(std::uint32_t& rng) {
    while (true) {
        const float3 p = make_float3(random_float01(rng) * 2.0f - 1.0f,
            random_float01(rng) * 2.0f - 1.0f, random_float01(rng) * 2.0f - 1.0f);
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

__device__ float3 packed_light_vector_to_float3(const PackedLightVector3& v) {
    return make_float3(v.x, v.y, v.z);
}

__device__ OpenPbrVec3 to_openpbr_vec3(const float3& value) {
    return {value.x, value.y, value.z};
}

__device__ float3 from_openpbr_vec3(const OpenPbrVec3& value) {
    return make_float3(value.x, value.y, value.z);
}

__device__ OpenPbrTransportContext openpbr_context_for(const PathState& state) {
    OpenPbrTransportContext context = state.openpbr_context;
    context.path_throughput = to_openpbr_vec3(state.throughput);
    return context;
}

__device__ float3 packed_medium_world_to_local_point(const PackedMedium& medium,
    const float3& point) {
    const float3 shifted = sub3(point, vector3f_to_float3(medium.translation));
    return make_float3(dot3(vector3f_to_float3(medium.rotation_row0), shifted),
        dot3(vector3f_to_float3(medium.rotation_row1), shifted),
        dot3(vector3f_to_float3(medium.rotation_row2), shifted));
}

__device__ float3 packed_medium_world_to_local_dir(const PackedMedium& medium, const float3& dir) {
    return make_float3(dot3(vector3f_to_float3(medium.rotation_row0), dir),
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

__device__ float address_texture_coordinate(float coordinate, int mode, bool& valid) {
    if (mode == 0 && (coordinate < 0.0f || coordinate > 1.0f)) {
        valid = false;
        return 0.0f;
    }
    if (mode == 2) {
        return coordinate - floorf(coordinate);
    }
    if (mode == 3) {
        const float period = coordinate - 2.0f * floorf(0.5f * coordinate);
        return period <= 1.0f ? period : 2.0f - period;
    }
    return clamp01(coordinate);
}

__device__ float3 image_texel(const DeviceSceneView& scene, const PackedTexture& texture, int x,
    int y) {
    x = x < 0 ? 0 : (x >= texture.image_width ? texture.image_width - 1 : x);
    y = y < 0 ? 0 : (y >= texture.image_height ? texture.image_height - 1 : y);
    const int pixel_index = texture.image_offset + y * texture.image_width + x;
    return pixel_index >= 0 && pixel_index < scene.image_texel_count
               ? vector3f_to_float3(scene.image_texels[pixel_index])
               : make_float3(0.0f, 1.0f, 1.0f);
}

__device__ float3 sample_image_texture(const DeviceSceneView& scene, const PackedTexture& texture,
    float u, float v) {
    if (scene.image_texels == nullptr || texture.image_width <= 0 || texture.image_height <= 0) {
        return make_float3(0.0f, 1.0f, 1.0f);
    }

    bool valid = true;
    const float addressed_u = address_texture_coordinate(u, texture.u_address_mode, valid);
    const float addressed_v = 1.0f - address_texture_coordinate(v, texture.v_address_mode, valid);
    if (!valid) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    if (texture.filter_type == 0) {
        int x = static_cast<int>(addressed_u * static_cast<float>(texture.image_width));
        int y = static_cast<int>(addressed_v * static_cast<float>(texture.image_height));
        x = x >= texture.image_width ? texture.image_width - 1 : x;
        y = y >= texture.image_height ? texture.image_height - 1 : y;
        return image_texel(scene, texture, x, y);
    }
    const float x = addressed_u * static_cast<float>(texture.image_width - 1);
    const float y = addressed_v * static_cast<float>(texture.image_height - 1);
    const int x0 = static_cast<int>(floorf(x));
    const int y0 = static_cast<int>(floorf(y));
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float3 top = add3(mul3(image_texel(scene, texture, x0, y0), 1.0f - tx),
        mul3(image_texel(scene, texture, x0 + 1, y0), tx));
    const float3 bottom = add3(mul3(image_texel(scene, texture, x0, y0 + 1), 1.0f - tx),
        mul3(image_texel(scene, texture, x0 + 1, y0 + 1), tx));
    return add3(mul3(top, 1.0f - ty), mul3(bottom, ty));
}

__device__ float3 evaluate_texture(const DeviceSceneView& scene, int texture_index, float u,
    float v, const float3& p) {
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

__device__ Double2 distort_pinhole_normalized(const DevicePinhole32Params& params,
    const Double2& xy) {
    const double x = xy.x;
    const double y = xy.y;
    const double x2 = x * x;
    const double y2 = y * y;
    const double xy_term = x * y;
    const double r2 = x2 + y2;
    const double r4 = r2 * r2;
    const double r6 = r4 * r2;
    const double radial = 1.0 + params.k1 * r2 + params.k2 * r4 + params.k3 * r6;
    return make_double2_xy(x * radial + params.p2 * (r2 + 2.0 * x2) + 2.0 * params.p1 * xy_term,
        y * radial + params.p1 * (r2 + 2.0 * y2) + 2.0 * params.p2 * xy_term);
}

__device__ bool solve_2x2(double a00, double a01, double a10, double a11, const Double2& rhs,
    Double2& delta) {
    const double det = a00 * a11 - a01 * a10;
    if (fabs(det) < kCameraEpsilon) {
        return false;
    }
    const double inv_det = 1.0 / det;
    delta.x = (a11 * rhs.x - a01 * rhs.y) * inv_det;
    delta.y = (-a10 * rhs.x + a00 * rhs.y) * inv_det;
    return true;
}

__device__ float3 unproject_pinhole32(const DevicePinhole32Params& params, double pixel_x,
    double pixel_y) {
    const Double2 xy_distorted =
        make_double2_xy((pixel_x - params.cx) / params.fx, (pixel_y - params.cy) / params.fy);
    Double2 xy = xy_distorted;
    for (int iter = 0; iter < kPinholeUndistortMaxIterations; ++iter) {
        const Double2 distorted = distort_pinhole_normalized(params, xy);
        const Double2 error =
            make_double2_xy(xy_distorted.x - distorted.x, xy_distorted.y - distorted.y);
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
        const double d_radial_dx =
            2.0 * x * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);
        const double d_radial_dy =
            2.0 * y * (params.k1 + 2.0 * params.k2 * r2 + 3.0 * params.k3 * r4);

        Double2 delta {};
        if (!solve_2x2(radial + x * d_radial_dx + 6.0 * params.p2 * x + 2.0 * params.p1 * y,
                x * d_radial_dy + 2.0 * params.p1 * x + 2.0 * params.p2 * y,
                y * d_radial_dx + 2.0 * params.p1 * x + 2.0 * params.p2 * y,
                radial + y * d_radial_dy + 6.0 * params.p1 * y + 2.0 * params.p2 * x, error,
                delta)) {
            break;
        }
        xy.x += delta.x;
        xy.y += delta.y;
    }

    return normalize3(make_float3(static_cast<float>(xy.x), static_cast<float>(xy.y), 1.0f));
}

__device__ float3 project_pinhole32(const DevicePinhole32Params& params, const float3& dir_camera) {
    if (fabsf(dir_camera.z) < 1e-7f) {
        return make_float3(-1.0f, -1.0f, 0.0f);
    }
    const double inv_z = 1.0 / static_cast<double>(dir_camera.z);
    const Double2 xy_undistorted = make_double2_xy(static_cast<double>(dir_camera.x) * inv_z,
        static_cast<double>(dir_camera.y) * inv_z);
    const Double2 xy_distorted = distort_pinhole_normalized(params, xy_undistorted);
    return make_float3(static_cast<float>(xy_distorted.x * params.fx + params.cx),
        static_cast<float>(xy_distorted.y * params.fy + params.cy), 1.0f);
}

__device__ Double2 apply_equi_tangential(const Double2& xy, const double tangential[2]) {
    const double xr = xy.x;
    const double yr = xy.y;
    const double xr2 = xr * xr;
    const double yr2 = yr * yr;
    const double xryr = xr * yr;
    const double p1 = tangential[0];
    const double p2 = tangential[1];
    return make_double2_xy(xr + 2.0 * p1 * xryr + p2 * (xr2 + yr2 + 2.0 * xr2),
        yr + p1 * (xr2 + yr2 + 2.0 * yr2) + 2.0 * p2 * xryr);
}

__device__ Double2 remove_equi_tangential_single_step(const Double2& xy_distorted,
    const double tangential[2]) {
    const Double2 distorted = apply_equi_tangential(xy_distorted, tangential);
    Double2 delta {};
    if (!solve_2x2(1.0 + 2.0 * tangential[0] * xy_distorted.y
                       + 6.0 * tangential[1] * xy_distorted.x,
            2.0 * tangential[0] * xy_distorted.x + 2.0 * tangential[1] * xy_distorted.y,
            2.0 * tangential[0] * xy_distorted.x + 2.0 * tangential[1] * xy_distorted.y,
            1.0 + 6.0 * tangential[0] * xy_distorted.y + 2.0 * tangential[1] * xy_distorted.x,
            make_double2_xy(xy_distorted.x - distorted.x, xy_distorted.y - distorted.y), delta)) {
        return xy_distorted;
    }
    return make_double2_xy(xy_distorted.x + delta.x, xy_distorted.y + delta.y);
}

__device__ bool interpolate_lut_theta(const DeviceEqui62Lut1DParams& params, double rd,
    double& theta) {
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

__device__ double invert_lut_theta(const DeviceEqui62Lut1DParams& params, double theta) {
    if (theta <= 0.0 || params.lut_step <= 0.0) {
        return 0.0;
    }
    if (theta >= params.lut[1023]) {
        return 1023.0 * params.lut_step;
    }
    for (int i = 0; i < 1023; ++i) {
        if (params.lut[i] <= theta && theta <= params.lut[i + 1]) {
            const double denom = params.lut[i + 1] - params.lut[i];
            const double alpha = denom > 0.0 ? (theta - params.lut[i]) / denom : 0.0;
            return (static_cast<double>(i) + alpha) * params.lut_step;
        }
    }
    return 1023.0 * params.lut_step;
}

__device__ float3 project_equi62_lut1d(const DeviceEqui62Lut1DParams& params,
    const float3& dir_camera) {
    const double dx = static_cast<double>(dir_camera.x);
    const double dy = static_cast<double>(dir_camera.y);
    const double dz = static_cast<double>(dir_camera.z);
    const double r = sqrt(dx * dx + dy * dy);
    if (r < kCameraEpsilon) {
        return make_float3(static_cast<float>(params.cx), static_cast<float>(params.cy), 1.0f);
    }
    const double theta = atan2(r, dz);
    const double rd = invert_lut_theta(params, theta);
    const double azim_cos = dx / r;
    const double azim_sin = dy / r;
    const Double2 xy_radial = make_double2_xy(azim_cos * rd, azim_sin * rd);
    const Double2 xy_distorted = apply_equi_tangential(xy_radial, params.tangential);
    return make_float3(static_cast<float>(xy_distorted.x * params.fx + params.cx),
        static_cast<float>(xy_distorted.y * params.fy + params.cy), 1.0f);
}

__device__ float3 normalized_fallback_ray(const Double2& xy) {
    return normalize3(make_float3(static_cast<float>(xy.x), static_cast<float>(xy.y), 1.0f));
}

__device__ float3 unproject_equi62_lut1d(const DeviceEqui62Lut1DParams& params, double pixel_x,
    double pixel_y) {
    const Double2 xy =
        make_double2_xy((pixel_x - params.cx) / params.fx, (pixel_y - params.cy) / params.fy);
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
    return normalize3(make_float3(static_cast<float>(xy_radial.x * scale),
        static_cast<float>(xy_radial.y * scale), 1.0f));
}

__device__ float3 unproject_camera_ray(const DeviceActiveCamera& camera, double pixel_x,
    double pixel_y) {
    if (camera.model == CameraModelType::equi62_lut1d) {
        return unproject_equi62_lut1d(camera.equi, pixel_x, pixel_y);
    }
    return unproject_pinhole32(camera.pinhole, pixel_x, pixel_y);
}

__device__ float2 project_camera_pixel(const DeviceActiveCamera& camera, const float3& dir_camera) {
    float3 pixel;
    if (camera.model == CameraModelType::equi62_lut1d) {
        pixel = project_equi62_lut1d(camera.equi, dir_camera);
    } else {
        pixel = project_pinhole32(camera.pinhole, dir_camera);
    }
    return make_float2(pixel.x, pixel.y);
}

__device__ float3 decode_normal(const float4& encoded) {
    return make_float3(encoded.x * 2.0f - 1.0f, encoded.y * 2.0f - 1.0f, encoded.z * 2.0f - 1.0f);
}

__device__ float3 transform_direction(const DeviceActiveCamera& camera, const float3& dir_camera) {
    return normalize3(add3(
        add3(mul3(make_float3(static_cast<float>(camera.basis_x[0]),
                      static_cast<float>(camera.basis_x[1]), static_cast<float>(camera.basis_x[2])),
                 dir_camera.x),
            mul3(make_float3(static_cast<float>(camera.basis_y[0]),
                     static_cast<float>(camera.basis_y[1]), static_cast<float>(camera.basis_y[2])),
                dir_camera.y)),
        mul3(make_float3(static_cast<float>(camera.basis_z[0]),
                 static_cast<float>(camera.basis_z[1]), static_cast<float>(camera.basis_z[2])),
            dir_camera.z)));
}

__device__ float3 camera_origin(const DeviceActiveCamera& camera) {
    return make_float3(static_cast<float>(camera.origin[0]), static_cast<float>(camera.origin[1]),
        static_cast<float>(camera.origin[2]));
}

__device__ float3 sample_uniform_cone_direction(const float3& axis, float cos_theta_max, float u0,
    float u1) {
    const float cos_theta = 1.0f - u0 * (1.0f - cos_theta_max);
    const float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta * cos_theta));
    const float phi = 2.0f * kPi * u1;
    const float3 helper =
        fabsf(axis.x) > 0.9f ? make_float3(0.0f, 1.0f, 0.0f) : make_float3(1.0f, 0.0f, 0.0f);
    const float3 tangent = normalize3(cross3(helper, axis));
    const float3 bitangent = cross3(axis, tangent);
    return normalize3(add3(mul3(axis, cos_theta),
        add3(mul3(tangent, sin_theta * cosf(phi)), mul3(bitangent, sin_theta * sinf(phi)))));
}

__device__ float3 sample_uniform_sphere_direction(float u0, float u1) {
    const float z = 1.0f - 2.0f * u0;
    const float radial = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    const float phi = 2.0f * kPi * u1;
    return make_float3(radial * cosf(phi), radial * sinf(phi), z);
}

__device__ const PackedLight* find_packed_light(const DeviceSceneView& scene, PackedLightType type,
    int primitive_index) {
    for (int i = 0; i < scene.light_count; ++i) {
        const PackedLight& light = scene.lights[i];
        if (light.type == type && light.primitive_index == primitive_index) {
            return &light;
        }
    }
    return nullptr;
}

__device__ float effective_light_selection_pdf(const DeviceSceneView& scene,
    const PackedLight& light, int excluded_primitive_type, int excluded_primitive_index) {
    const PackedLight* excluded =
        excluded_primitive_type < 0
            ? nullptr
            : find_packed_light(scene, static_cast<PackedLightType>(excluded_primitive_type),
                  excluded_primitive_index);
    if (excluded == &light) {
        return 0.0f;
    }
    const float eligible_probability = excluded == nullptr ? 1.0f : 1.0f - excluded->selection_pdf;
    return eligible_probability > 1e-8f ? light.selection_pdf / eligible_probability : 0.0f;
}

__device__ float conditional_light_pdf_for_hit(const DeviceSceneView& scene,
    const PackedLight& light, const float3& origin, const float3& direction, const HitInfo* hit) {
    if (light.type == PackedLightType::environment) {
        return light_uniform_sphere_pdf();
    }
    if (hit == nullptr || !hit->hit || hit->primitive_type != static_cast<int>(light.type)
        || hit->primitive_index != light.primitive_index || !hit->front_face) {
        return 0.0f;
    }

    if (light.type == PackedLightType::sphere) {
        const PackedSphere& sphere = scene.spheres[light.primitive_index];
        const float distance_squared = length_sq3(sub3(vector3f_to_float3(sphere.center), origin));
        const float radius_squared = sphere.radius * sphere.radius;
        if (distance_squared <= radius_squared) {
            return 0.0f;
        }
        const float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius_squared / distance_squared));
        return light_uniform_cone_pdf(cos_theta_max);
    }

    const float distance_squared = length_sq3(sub3(hit->position, origin));
    const float abs_cosine = fmaxf(dot3(hit->geometric_normal, mul3(direction, -1.0f)), 0.0f);
    if (light.type == PackedLightType::quad) {
        const PackedQuad& quad = scene.quads[light.primitive_index];
        const float area =
            length3(cross3(vector3f_to_float3(quad.edge_u), vector3f_to_float3(quad.edge_v)));
        return light_area_to_solid_angle_pdf(area, distance_squared, abs_cosine);
    }
    const PackedTriangle& triangle = scene.triangles[light.primitive_index];
    const float area =
        0.5f
        * length3(cross3(sub3(vector3f_to_float3(triangle.p1), vector3f_to_float3(triangle.p0)),
            sub3(vector3f_to_float3(triangle.p2), vector3f_to_float3(triangle.p0))));
    return light_area_to_solid_angle_pdf(area, distance_squared, abs_cosine);
}

__device__ float emission_mis_weight(const DeviceSceneView& scene, const PathState& state,
    const HitInfo* hit, bool environment) {
    if (!state.previous_scatter_valid || state.previous_scatter_delta) {
        return 1.0f;
    }
    if (!environment && (hit == nullptr || hit->primitive_type < 0)) {
        return 1.0f;
    }
    const PackedLight* light =
        environment ? find_packed_light(scene, PackedLightType::environment, -1)
                    : find_packed_light(scene, static_cast<PackedLightType>(hit->primitive_type),
                          hit->primitive_index);
    if (light == nullptr) {
        return 1.0f;
    }
    const float3 direction = normalize3(state.ray.direction);
    const float conditional_pdf =
        environment ? (state.previous_material_type == 4
                              ? light_uniform_sphere_pdf()
                              : fmaxf(dot3(state.previous_scatter_normal, direction), 0.0f) / kPi)
                    : conditional_light_pdf_for_hit(scene, *light, state.previous_scatter_position,
                          direction, hit);
    const float selection_pdf = effective_light_selection_pdf(scene, *light,
        state.previous_primitive_type, state.previous_primitive_index);
    const float light_pdf = selection_pdf * conditional_pdf;
    return light_power_heuristic(state.previous_bsdf_pdf, light_pdf);
}

__device__ bool finite_analytic_light(PackedAnalyticLightType type) {
    return type == PackedAnalyticLightType::sphere || type == PackedAnalyticLightType::disk
           || type == PackedAnalyticLightType::rect || type == PackedAnalyticLightType::cylinder;
}

__device__ float3 analytic_light_normal(const PackedAnalyticLight& light) {
    return normalize3(mul3(cross3(packed_light_vector_to_float3(light.basis_x),
                               packed_light_vector_to_float3(light.basis_y)),
        -1.0f));
}

__device__ bool analytic_plane_coordinates(const PackedAnalyticLight& light, const float3& point,
    float& local_x, float& local_y) {
    const float3 basis_x = packed_light_vector_to_float3(light.basis_x);
    const float3 basis_y = packed_light_vector_to_float3(light.basis_y);
    const float3 local = sub3(point, packed_light_vector_to_float3(light.position));
    const float xx = dot3(basis_x, basis_x);
    const float xy = dot3(basis_x, basis_y);
    const float yy = dot3(basis_y, basis_y);
    const float determinant = xx * yy - xy * xy;
    if (determinant <= 1e-16f) {
        return false;
    }
    const float px = dot3(local, basis_x);
    const float py = dot3(local, basis_y);
    local_x = (px * yy - py * xy) / determinant;
    local_y = (py * xx - px * xy) / determinant;
    return true;
}

__device__ bool try_intersect_analytic_light(const PackedAnalyticLight& light, const Ray& ray,
    float t_min, float t_max, AnalyticLightHit& hit) {
    const float3 center = packed_light_vector_to_float3(light.position);
    if (light.type == PackedAnalyticLightType::sphere) {
        const float radius = light.radius * length3(packed_light_vector_to_float3(light.basis_x));
        const float3 oc = sub3(ray.origin, center);
        const float a = length_sq3(ray.direction);
        const float half_b = dot3(oc, ray.direction);
        const float c = length_sq3(oc) - radius * radius;
        const float discriminant = half_b * half_b - a * c;
        if (a <= 1e-16f || discriminant < 0.0f) {
            return false;
        }
        const float root_term = sqrtf(discriminant);
        float root = (-half_b - root_term) / a;
        if (root <= t_min || root >= t_max) {
            root = (-half_b + root_term) / a;
        }
        if (root <= t_min || root >= t_max) {
            return false;
        }
        const float3 position = add3(ray.origin, mul3(ray.direction, root));
        const float3 normal = normalize3(sub3(position, center));
        if (dot3(ray.direction, normal) >= 0.0f) {
            return false;
        }
        hit.hit = true;
        hit.t = root;
        hit.position = position;
        hit.normal = normal;
        return true;
    }

    if (light.type == PackedAnalyticLightType::disk
        || light.type == PackedAnalyticLightType::rect) {
        const float3 normal = analytic_light_normal(light);
        const float denominator = dot3(ray.direction, normal);
        if (denominator >= -1e-8f) {
            return false;
        }
        const float t = dot3(sub3(center, ray.origin), normal) / denominator;
        if (t <= t_min || t >= t_max) {
            return false;
        }
        const float3 position = add3(ray.origin, mul3(ray.direction, t));
        float local_x = 0.0f;
        float local_y = 0.0f;
        if (!analytic_plane_coordinates(light, position, local_x, local_y)) {
            return false;
        }
        const bool inside =
            light.type == PackedAnalyticLightType::disk
                ? local_x * local_x + local_y * local_y <= light.radius * light.radius
                : fabsf(local_x) <= 0.5f * light.width && fabsf(local_y) <= 0.5f * light.height;
        if (!inside) {
            return false;
        }
        hit.hit = true;
        hit.t = t;
        hit.position = position;
        hit.normal = normal;
        return true;
    }

    if (light.type != PackedAnalyticLightType::cylinder) {
        return false;
    }
    const float3 basis_x = packed_light_vector_to_float3(light.basis_x);
    const float3 basis_y = packed_light_vector_to_float3(light.basis_y);
    const float3 basis_z = packed_light_vector_to_float3(light.basis_z);
    const float scale = length3(basis_x);
    if (scale <= 1e-8f) {
        return false;
    }
    const float3 axis_x = div3(basis_x, scale);
    const float3 axis_y = div3(basis_y, scale);
    const float3 axis_z = normalize3(basis_z);
    const float3 local_origin = sub3(ray.origin, center);
    const float ox = dot3(local_origin, axis_x);
    const float oy = dot3(local_origin, axis_y);
    const float oz = dot3(local_origin, axis_z);
    const float dx = dot3(ray.direction, axis_x);
    const float dy = dot3(ray.direction, axis_y);
    const float dz = dot3(ray.direction, axis_z);
    const float radius = light.radius * scale;
    const float a = dx * dx + dy * dy;
    const float half_b = ox * dx + oy * dy;
    const float c = ox * ox + oy * oy - radius * radius;
    const float discriminant = half_b * half_b - a * c;
    if (a <= 1e-16f || discriminant < 0.0f) {
        return false;
    }
    const float root_term = sqrtf(discriminant);
    float root = (-half_b - root_term) / a;
    float z = oz + root * dz;
    const float half_length = 0.5f * light.length * length3(basis_z);
    if (root <= t_min || root >= t_max || fabsf(z) > half_length) {
        root = (-half_b + root_term) / a;
        z = oz + root * dz;
    }
    if (root <= t_min || root >= t_max || fabsf(z) > half_length) {
        return false;
    }
    const float x = ox + root * dx;
    const float y = oy + root * dy;
    const float3 normal = normalize3(add3(mul3(axis_x, x), mul3(axis_y, y)));
    if (dot3(ray.direction, normal) >= 0.0f) {
        return false;
    }
    hit.hit = true;
    hit.t = root;
    hit.position = add3(ray.origin, mul3(ray.direction, root));
    hit.normal = normal;
    return true;
}

__device__ AnalyticLightHit intersect_analytic_lights(const DeviceSceneView& scene, const Ray& ray,
    float t_min, float t_max) {
    AnalyticLightHit closest_hit {};
    float closest = t_max;
    for (int i = 0; i < scene.analytic_light_count; ++i) {
        const PackedAnalyticLight& light = scene.analytic_lights[i];
        if (!finite_analytic_light(light.type) || light.treat_as_point != 0) {
            continue;
        }
        AnalyticLightHit candidate {};
        if (!try_intersect_analytic_light(light, ray, t_min, closest, candidate)) {
            continue;
        }
        candidate.light_index = i;
        closest = candidate.t;
        closest_hit = candidate;
    }
    return closest_hit;
}

__device__ float analytic_light_pdf_for_hit(const PackedAnalyticLight& light, const float3& origin,
    const float3& direction, const AnalyticLightHit& hit) {
    if (light.delta != 0 || light.treat_as_point != 0 || light.treat_as_line != 0) {
        return 0.0f;
    }
    if (light.type == PackedAnalyticLightType::sphere) {
        const float3 center = packed_light_vector_to_float3(light.position);
        const float radius = light.radius * length3(packed_light_vector_to_float3(light.basis_x));
        const float distance_squared = length_sq3(sub3(center, origin));
        const float radius_squared = radius * radius;
        if (distance_squared <= radius_squared) {
            return 0.0f;
        }
        const float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius_squared / distance_squared));
        return light_uniform_cone_pdf(cos_theta_max);
    }
    const float distance_squared = length_sq3(sub3(hit.position, origin));
    const float abs_cosine = fmaxf(dot3(hit.normal, mul3(direction, -1.0f)), 0.0f);
    return light_area_to_solid_angle_pdf(light.world_area, distance_squared, abs_cosine);
}

__device__ float analytic_emission_mis_weight(const DeviceSceneView& scene, const PathState& state,
    const AnalyticLightHit& hit) {
    if (!state.previous_scatter_valid || state.previous_scatter_delta || hit.light_index < 0) {
        return 1.0f;
    }
    const PackedAnalyticLight& light = scene.analytic_lights[hit.light_index];
    const float3 direction = normalize3(state.ray.direction);
    const float light_pdf =
        light.selection_pdf
        * analytic_light_pdf_for_hit(light, state.previous_scatter_position, direction, hit);
    return light_power_heuristic(state.previous_bsdf_pdf, light_pdf);
}

__device__ bool analytic_infinite_direction(const PackedAnalyticLight& light,
    const float3& direction) {
    if (light.type == PackedAnalyticLightType::dome) {
        return true;
    }
    if (light.type != PackedAnalyticLightType::distant) {
        return false;
    }
    const float3 axis = normalize3(packed_light_vector_to_float3(light.basis_z));
    const float threshold = light.delta != 0 ? 0.999999f : light.cos_theta_max;
    return dot3(axis, direction) >= threshold;
}

__device__ float analytic_infinite_pdf(const PackedAnalyticLight& light) {
    if (light.delta != 0) {
        return 0.0f;
    }
    return light.type == PackedAnalyticLightType::dome
               ? light_uniform_sphere_pdf()
               : light_uniform_cone_pdf(light.cos_theta_max);
}

__device__ float3 analytic_infinite_radiance(const DeviceSceneView& scene, const PathState& state) {
    float3 radiance = make_float3(0.0f, 0.0f, 0.0f);
    const float3 direction = normalize3(state.ray.direction);
    for (int i = 0; i < scene.analytic_light_count; ++i) {
        const PackedAnalyticLight& light = scene.analytic_lights[i];
        if (!analytic_infinite_direction(light, direction)) {
            continue;
        }
        float weight = 1.0f;
        if (state.previous_scatter_valid && !state.previous_scatter_delta) {
            const float conditional_pdf = analytic_infinite_pdf(light);
            if (conditional_pdf <= 0.0f) {
                continue;
            }
            weight = light_power_heuristic(state.previous_bsdf_pdf,
                light.selection_pdf * conditional_pdf);
        }
        radiance = add3(radiance, mul3(packed_light_vector_to_float3(light.radiance), weight));
    }
    return radiance;
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

__device__ const OpenPbrCompiledMaterial* resolve_openpbr_material(const DeviceSceneView& scene,
    int openpbr_index) {
    if (openpbr_index < 0 || openpbr_index >= scene.openpbr_material_count
        || scene.openpbr_materials == nullptr) {
        return nullptr;
    }
    return &scene.openpbr_materials[openpbr_index];
}

__device__ const OpenPbrCompiledMaterial* resolve_openpbr_material(const DeviceSceneView& scene,
    const MaterialSample& material) {
    return material.type == 5 ? resolve_openpbr_material(scene, material.openpbr_index) : nullptr;
}

__device__ void evaluate_openpbr_color_binding(const DeviceSceneView& scene,
    OpenPbrCoreMaterial& parameters, const OpenPbrColorTextureBinding& binding,
    OpenPbrColorInput input, float tex_u, float tex_v, const float3& position) {
    if (binding.texture_index < 0) {
        return;
    }
    const float3 sampled = evaluate_texture(scene, binding.texture_index, tex_u, tex_v, position);
    openpbr_apply_color_input(parameters, input, to_openpbr_vec3(sampled),
        binding.source_color_space);
}

__device__ OpenPbrCoreMaterial evaluate_openpbr_scattering_material(const DeviceSceneView& scene,
    const OpenPbrCompiledMaterial& material, const HitInfo& hit) {
    OpenPbrCoreMaterial parameters = material.parameters;
    if (material.color_textures.base_color.texture_index >= 0) {
        parameters.base_color = to_openpbr_vec3(hit.base_color);
    }
    evaluate_openpbr_color_binding(scene, parameters, material.color_textures.specular_color,
        OpenPbrColorInput::specular_color, hit.tex_u, hit.tex_v, hit.position);
    evaluate_openpbr_color_binding(scene, parameters, material.color_textures.transmission_color,
        OpenPbrColorInput::transmission_color, hit.tex_u, hit.tex_v, hit.position);
    return parameters;
}

__device__ void populate_material(const DeviceSceneView& scene, int material_index, HitInfo& hit) {
    if (material_index < 0 || material_index >= scene.material_count
        || scene.materials == nullptr) {
        hit.base_color = make_float3(0.0f, 0.0f, 0.0f);
        hit.emission = make_float3(0.0f, 0.0f, 0.0f);
        hit.material_type = 0;
        hit.ior = 1.0f;
        hit.fuzz = 0.0f;
        hit.openpbr_index = -1;
        return;
    }

    const MaterialSample& material = scene.materials[material_index];
    hit.openpbr_index = material.openpbr_index;
    hit.material_type = material.type;
    hit.fuzz = material.fuzz;
    hit.ior = material.ior;
    hit.base_color =
        evaluate_texture(scene, material.albedo_texture, hit.tex_u, hit.tex_v, hit.position);
    hit.emission =
        evaluate_texture(scene, material.emission_texture, hit.tex_u, hit.tex_v, hit.position);
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
    if (hit.material_type == 5) {
        const OpenPbrCompiledMaterial* compiled =
            resolve_openpbr_material(scene, material.openpbr_index);
        if (compiled == nullptr) {
            hit.material_type = -1;
            hit.base_color = make_float3(0.0f, 0.0f, 0.0f);
            hit.emission = make_float3(0.0f, 0.0f, 0.0f);
            return;
        }
        OpenPbrCoreMaterial parameters = compiled->parameters;
        evaluate_openpbr_color_binding(scene, parameters, compiled->color_textures.base_color,
            OpenPbrColorInput::base_color, hit.tex_u, hit.tex_v, hit.position);
        evaluate_openpbr_color_binding(scene, parameters, compiled->color_textures.emission_color,
            OpenPbrColorInput::emission_color, hit.tex_u, hit.tex_v, hit.position);
        hit.base_color = from_openpbr_vec3(parameters.base_color);
        hit.emission = from_openpbr_vec3(emission_openpbr_core(parameters));
    }
}

__device__ float3 evaluate_material_emission(const DeviceSceneView& scene,
    const MaterialSample& material, float tex_u, float tex_v, const float3& position) {
    if (material.type == 5) {
        const OpenPbrCompiledMaterial* compiled = resolve_openpbr_material(scene, material);
        if (compiled == nullptr) {
            return make_float3(0.0f, 0.0f, 0.0f);
        }
        OpenPbrCoreMaterial parameters = compiled->parameters;
        evaluate_openpbr_color_binding(scene, parameters, compiled->color_textures.emission_color,
            OpenPbrColorInput::emission_color, tex_u, tex_v, position);
        return from_openpbr_vec3(emission_openpbr_core(parameters));
    }
    return evaluate_texture(scene, material.emission_texture, tex_u, tex_v, position);
}

__device__ float3 direct_material_response(const DeviceSceneView& scene, const HitInfo& hit,
    const PathState& state, const float3& light_direction, float& bsdf_pdf) {
    bsdf_pdf = 0.0f;
    if (hit.material_type == 0) {
        const float cosine = fmaxf(dot3(hit.shading_normal, light_direction), 0.0f);
        bsdf_pdf = cosine / kPi;
        return mul3(hit.base_color, bsdf_pdf);
    }
    if (hit.material_type == 4) {
        bsdf_pdf = light_uniform_sphere_pdf();
        return mul3(hit.base_color, bsdf_pdf);
    }
    if (hit.material_type != 5) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    const OpenPbrCompiledMaterial* compiled = resolve_openpbr_material(scene, hit.openpbr_index);
    if (compiled == nullptr) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    const OpenPbrCoreMaterial parameters =
        evaluate_openpbr_scattering_material(scene, *compiled, hit);
    const OpenPbrFrame frame =
        make_openpbr_frame(to_openpbr_vec3(hit.geometric_normal), OpenPbrVec3 {});
    const OpenPbrEvaluation evaluation = evaluate_openpbr_core(parameters, frame,
        to_openpbr_vec3(mul3(normalize3(state.ray.direction), -1.0f)),
        to_openpbr_vec3(light_direction), openpbr_context_for(state));
    const float cosine = fabsf(dot3(hit.shading_normal, light_direction));
    bsdf_pdf = evaluation.pdf;
    return mul3(from_openpbr_vec3(evaluation.value), cosine);
}

__device__ DirectLightSample sample_direct_light(const DeviceSceneView& scene,
    const float3& environment, const HitInfo& receiver, std::uint32_t& rng) {
    DirectLightSample sample {};
    const float3 surface_point = receiver.position;
    const PackedLight* excluded =
        receiver.primitive_type < 0
            ? nullptr
            : find_packed_light(scene, static_cast<PackedLightType>(receiver.primitive_type),
                  receiver.primitive_index);
    const float eligible_probability = excluded == nullptr ? 1.0f : 1.0f - excluded->selection_pdf;
    if (eligible_probability <= 1e-8f) {
        return sample;
    }
    int light_index = -1;
    if (excluded == nullptr) {
        light_index = sample_packed_light(scene.lights, scene.light_count, random_float01(rng));
    } else {
        const float target = random_float01(rng) * eligible_probability;
        float accumulated = 0.0f;
        for (int i = 0; i < scene.light_count; ++i) {
            if (&scene.lights[i] == excluded) {
                continue;
            }
            accumulated += scene.lights[i].selection_pdf;
            if (target < accumulated) {
                light_index = i;
                break;
            }
        }
        if (light_index < 0) {
            for (int i = scene.light_count - 1; i >= 0; --i) {
                if (&scene.lights[i] != excluded) {
                    light_index = i;
                    break;
                }
            }
        }
    }
    if (light_index < 0) {
        return sample;
    }
    const PackedLight& light = scene.lights[light_index];
    const float selection_pdf = light.selection_pdf / eligible_probability;
    if (selection_pdf <= 0.0f) {
        return sample;
    }

    if (light.type == PackedLightType::environment) {
        if (receiver.material_type == 4) {
            sample.direction = random_unit_vector(rng);
            sample.pdf = selection_pdf * light_uniform_sphere_pdf();
        } else {
            sample.direction = add3(receiver.shading_normal, random_unit_vector(rng));
            if (near_zero3(sample.direction)) {
                sample.direction = receiver.shading_normal;
            }
            sample.direction = normalize3(sample.direction);
            const float cosine = fmaxf(dot3(receiver.shading_normal, sample.direction), 0.0f);
            sample.pdf = selection_pdf * cosine / kPi;
        }
        sample.emission = environment;
        sample.distance = kRayFar;
        sample.infinite = true;
        sample.valid = sample.pdf > 0.0f;
        return sample;
    }

    float tex_u = 0.0f;
    float tex_v = 0.0f;
    float conditional_pdf = 0.0f;
    if (light.type == PackedLightType::sphere) {
        if (light.primitive_index < 0 || light.primitive_index >= scene.sphere_count) {
            return sample;
        }
        const PackedSphere& sphere = scene.spheres[light.primitive_index];
        const float3 center = vector3f_to_float3(sphere.center);
        const float3 to_center = sub3(center, surface_point);
        const float distance_squared = length_sq3(to_center);
        const float radius_squared = sphere.radius * sphere.radius;
        if (distance_squared <= radius_squared * (1.0f + 1e-6f)) {
            return sample;
        }
        const float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius_squared / distance_squared));
        sample.direction = sample_uniform_cone_direction(normalize3(to_center), cos_theta_max,
            random_float01(rng), random_float01(rng));
        const float3 oc = sub3(surface_point, center);
        const float half_b = dot3(oc, sample.direction);
        const float discriminant = half_b * half_b - (length_sq3(oc) - radius_squared);
        if (discriminant <= 0.0f) {
            return sample;
        }
        sample.distance = -half_b - sqrtf(discriminant);
        if (sample.distance <= kRayEpsilon) {
            return sample;
        }
        sample.position = add3(surface_point, mul3(sample.direction, sample.distance));
        sample.normal = normalize3(sub3(sample.position, center));
        sphere_uv(sample.normal, tex_u, tex_v);
        sample.material_index = sphere.material_index;
        conditional_pdf = light_uniform_cone_pdf(cos_theta_max);
    } else if (light.type == PackedLightType::quad) {
        if (light.primitive_index < 0 || light.primitive_index >= scene.quad_count) {
            return sample;
        }
        const PackedQuad& quad = scene.quads[light.primitive_index];
        const float3 origin = vector3f_to_float3(quad.origin);
        const float3 edge_u = vector3f_to_float3(quad.edge_u);
        const float3 edge_v = vector3f_to_float3(quad.edge_v);
        tex_u = random_float01(rng);
        tex_v = random_float01(rng);
        sample.position = add3(origin, add3(mul3(edge_u, tex_u), mul3(edge_v, tex_v)));
        const float3 area_normal = cross3(edge_u, edge_v);
        const float area = length3(area_normal);
        if (area <= 1e-12f) {
            return sample;
        }
        sample.normal = div3(area_normal, area);
        sample.material_index = quad.material_index;
        const float3 to_light = sub3(sample.position, surface_point);
        const float distance_squared = length_sq3(to_light);
        if (distance_squared <= 1e-12f) {
            return DirectLightSample {};
        }
        sample.distance = sqrtf(distance_squared);
        sample.direction = div3(to_light, sample.distance);
        const float abs_cosine = dot3(sample.normal, mul3(sample.direction, -1.0f));
        conditional_pdf = light_area_to_solid_angle_pdf(area, distance_squared, abs_cosine);
    } else {
        if (light.primitive_index < 0 || light.primitive_index >= scene.triangle_count) {
            return sample;
        }
        const PackedTriangle& triangle = scene.triangles[light.primitive_index];
        const float3 p0 = vector3f_to_float3(triangle.p0);
        const float3 p1 = vector3f_to_float3(triangle.p1);
        const float3 p2 = vector3f_to_float3(triangle.p2);
        float w0 = 0.0f;
        float w1 = 0.0f;
        float w2 = 0.0f;
        sample_uniform_triangle(random_float01(rng), random_float01(rng), w0, w1, w2);
        sample.position = add3(mul3(p0, w0), add3(mul3(p1, w1), mul3(p2, w2)));
        if (triangle.has_texcoords != 0) {
            tex_u = triangle.uv0.x() * w0 + triangle.uv1.x() * w1 + triangle.uv2.x() * w2;
            tex_v = triangle.uv0.y() * w0 + triangle.uv1.y() * w1 + triangle.uv2.y() * w2;
        } else {
            tex_u = w1;
            tex_v = w2;
        }
        const float3 area_normal = cross3(sub3(p1, p0), sub3(p2, p0));
        const float double_area = length3(area_normal);
        if (double_area <= 1e-12f) {
            return sample;
        }
        sample.normal = div3(area_normal, double_area);
        sample.material_index = triangle.material_index;
        const float3 to_light = sub3(sample.position, surface_point);
        const float distance_squared = length_sq3(to_light);
        if (distance_squared <= 1e-12f) {
            return DirectLightSample {};
        }
        sample.distance = sqrtf(distance_squared);
        sample.direction = div3(to_light, sample.distance);
        const float abs_cosine = dot3(sample.normal, mul3(sample.direction, -1.0f));
        conditional_pdf =
            light_area_to_solid_angle_pdf(0.5f * double_area, distance_squared, abs_cosine);
    }

    if (conditional_pdf <= 0.0f || sample.material_index < 0
        || sample.material_index >= scene.material_count) {
        return DirectLightSample {};
    }
    sample.emission = evaluate_material_emission(scene, scene.materials[sample.material_index],
        tex_u, tex_v, sample.position);
    sample.pdf = selection_pdf * conditional_pdf;
    sample.valid = sample.pdf > 0.0f && max_component3(sample.emission) > 0.0f;
    return sample;
}

__device__ DirectLightSample sample_analytic_direct_light_candidate(const DeviceSceneView& scene,
    const HitInfo& receiver, int light_index, float u0, float u1) {
    DirectLightSample sample {};
    if (light_index < 0 || light_index >= scene.analytic_light_count) {
        return sample;
    }
    const PackedAnalyticLight& light = scene.analytic_lights[light_index];
    if (light.selection_pdf <= 0.0f) {
        return sample;
    }
    sample.light_index = light_index;

    sample.emission = packed_light_vector_to_float3(light.radiance);
    if (light.type == PackedAnalyticLightType::dome) {
        sample.direction = sample_uniform_sphere_direction(u0, u1);
        sample.distance = kRayFar;
        sample.pdf = light.selection_pdf * light_uniform_sphere_pdf();
        sample.infinite = true;
        sample.valid = sample.pdf > 0.0f && max_component3(sample.emission) > 0.0f;
        return sample;
    }
    if (light.type == PackedAnalyticLightType::distant) {
        const float3 axis = normalize3(packed_light_vector_to_float3(light.basis_z));
        sample.direction = light.delta != 0
                               ? axis
                               : sample_uniform_cone_direction(axis, light.cos_theta_max, u0, u1);
        sample.distance = kRayFar;
        sample.pdf = light.selection_pdf
                     * (light.delta != 0 ? 1.0f : light_uniform_cone_pdf(light.cos_theta_max));
        sample.infinite = true;
        sample.delta = light.delta != 0;
        sample.valid = sample.pdf > 0.0f && max_component3(sample.emission) > 0.0f;
        return sample;
    }

    const float3 surface_point = receiver.position;
    const float3 center = packed_light_vector_to_float3(light.position);
    if (light.type == PackedAnalyticLightType::sphere) {
        const float3 to_center = sub3(center, surface_point);
        const float distance_squared = length_sq3(to_center);
        const float radius = light.radius * length3(packed_light_vector_to_float3(light.basis_x));
        const float radius_squared = radius * radius;
        if (distance_squared <= radius_squared * (1.0f + 1e-6f)) {
            return sample;
        }
        if (light.treat_as_point != 0) {
            sample.distance = sqrtf(distance_squared);
            sample.direction = div3(to_center, sample.distance);
            const float intensity_scale = light.world_area > 1e-12f ? light.world_area : 1.0f;
            sample.emission = mul3(sample.emission, intensity_scale / distance_squared);
            sample.pdf = light.selection_pdf;
            sample.delta = true;
            sample.valid = max_component3(sample.emission) > 0.0f;
            return sample;
        }
        const float cos_theta_max = sqrtf(fmaxf(0.0f, 1.0f - radius_squared / distance_squared));
        sample.direction =
            sample_uniform_cone_direction(normalize3(to_center), cos_theta_max, u0, u1);
        AnalyticLightHit surface_hit {};
        if (!try_intersect_analytic_light(light,
                Ray {.origin = surface_point, .direction = sample.direction}, kRayEpsilon, kRayFar,
                surface_hit)) {
            return DirectLightSample {};
        }
        sample.position = surface_hit.position;
        sample.normal = surface_hit.normal;
        sample.distance = surface_hit.t;
        sample.pdf = light.selection_pdf * light_uniform_cone_pdf(cos_theta_max);
        sample.valid = sample.pdf > 0.0f && max_component3(sample.emission) > 0.0f;
        return sample;
    }

    if (light.type == PackedAnalyticLightType::disk) {
        const float radial = light.radius * sqrtf(u0);
        const float phi = 2.0f * kPi * u1;
        sample.position = add3(center,
            add3(mul3(packed_light_vector_to_float3(light.basis_x), radial * cosf(phi)),
                mul3(packed_light_vector_to_float3(light.basis_y), radial * sinf(phi))));
        sample.normal = analytic_light_normal(light);
    } else if (light.type == PackedAnalyticLightType::rect) {
        sample.position = add3(center,
            add3(mul3(packed_light_vector_to_float3(light.basis_x), (u0 - 0.5f) * light.width),
                mul3(packed_light_vector_to_float3(light.basis_y), (u1 - 0.5f) * light.height)));
        sample.normal = analytic_light_normal(light);
    } else if (light.type == PackedAnalyticLightType::cylinder) {
        const float phi = 2.0f * kPi * u0;
        const float radial_x = light.radius * cosf(phi);
        const float radial_y = light.radius * sinf(phi);
        sample.position = add3(center,
            add3(add3(mul3(packed_light_vector_to_float3(light.basis_x), radial_x),
                     mul3(packed_light_vector_to_float3(light.basis_y), radial_y)),
                mul3(packed_light_vector_to_float3(light.basis_z), (u1 - 0.5f) * light.length)));
        sample.normal =
            normalize3(add3(mul3(packed_light_vector_to_float3(light.basis_x), radial_x),
                mul3(packed_light_vector_to_float3(light.basis_y), radial_y)));
    } else {
        return DirectLightSample {};
    }

    const float3 to_light = sub3(sample.position, surface_point);
    const float distance_squared = length_sq3(to_light);
    if (distance_squared <= 1e-12f) {
        return DirectLightSample {};
    }
    sample.distance = sqrtf(distance_squared);
    sample.direction = div3(to_light, sample.distance);
    const float abs_cosine = dot3(sample.normal, mul3(sample.direction, -1.0f));
    const float conditional_pdf =
        light_area_to_solid_angle_pdf(light.world_area, distance_squared, abs_cosine);
    sample.pdf = light.selection_pdf * conditional_pdf;
    sample.valid = sample.pdf > 0.0f && max_component3(sample.emission) > 0.0f;
    return sample;
}

__device__ DirectLightSample sample_analytic_direct_light(const DeviceSceneView& scene,
    const HitInfo& receiver, std::uint32_t& rng) {
    const int light_index = sample_packed_analytic_light(scene.analytic_lights,
        scene.analytic_light_count, random_float01(rng));
    return sample_analytic_direct_light_candidate(scene, receiver, light_index, random_float01(rng),
        random_float01(rng));
}

__device__ bool hit_medium_boundary(const PackedMedium& medium, const Ray& ray, float t_min,
    float t_max, float& entry_t, float& exit_t) {
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

__device__ void try_hit_sphere(const PackedSphere& sphere, const Ray& ray, float t_min, float t_max,
    HitInfo& best_hit, bool& found_hit) {
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

__device__ void try_hit_quad(const PackedQuad& quad, const Ray& ray, float t_min, float t_max,
    HitInfo& best_hit, bool& found_hit) {
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

__device__ void try_hit_triangle(const PackedTriangle& triangle, const Ray& ray, float t_min,
    float t_max, HitInfo& best_hit, bool& found_hit) {
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
    const float w = 1.0f - u - v;
    found_hit = true;
    best_hit.hit = true;
    best_hit.t = t;
    best_hit.position = add3(ray.origin, mul3(ray.direction, t));
    if (triangle.has_texcoords != 0) {
        best_hit.tex_u = triangle.uv0.x() * w + triangle.uv1.x() * u + triangle.uv2.x() * v;
        best_hit.tex_v = triangle.uv0.y() * w + triangle.uv1.y() * u + triangle.uv2.y() * v;
    } else {
        best_hit.tex_u = u;
        best_hit.tex_v = v;
    }
    set_face_normal(ray, normal, best_hit);
    if (triangle.has_vertex_normals != 0) {
        float3 shading = add3(mul3(vector3f_to_float3(triangle.n0), w),
            add3(mul3(vector3f_to_float3(triangle.n1), u),
                mul3(vector3f_to_float3(triangle.n2), v)));
        if (!near_zero3(shading)) {
            shading = normalize3(shading);
            if (dot3(shading, normal) < 0.0f) {
                shading = mul3(shading, -1.0f);
            }
            best_hit.shading_normal = best_hit.front_face ? shading : mul3(shading, -1.0f);
        }
    }
}

__device__ bool hit_acceleration_bounds(const PackedBvhNode& node, const Ray& ray, float t_min,
    float t_max) {
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = axis == 0 ? ray.origin.x : (axis == 1 ? ray.origin.y : ray.origin.z);
        const float direction =
            axis == 0 ? ray.direction.x : (axis == 1 ? ray.direction.y : ray.direction.z);
        const float bounds_min = node.bounds_min[axis];
        const float bounds_max = node.bounds_max[axis];
        if (fabsf(direction) <= 1e-12f) {
            if (origin < bounds_min || origin > bounds_max) {
                return false;
            }
            continue;
        }
        const float inverse = 1.0f / direction;
        float near_t = (bounds_min - origin) * inverse;
        float far_t = (bounds_max - origin) * inverse;
        if (near_t > far_t) {
            const float temporary = near_t;
            near_t = far_t;
            far_t = temporary;
        }
        t_min = fmaxf(t_min, near_t);
        t_max = fminf(t_max, far_t);
        if (t_max < t_min) {
            return false;
        }
    }
    return true;
}

__device__ void try_hit_acceleration_reference(const DeviceSceneView& scene,
    const PackedPrimitiveRef& reference, const Ray& ray, float t_min, float& closest, HitInfo& hit,
    bool& found) {
    HitInfo candidate {};
    bool candidate_hit = false;
    int material_index = -1;
    switch (static_cast<PackedPrimitiveType>(reference.primitive_type)) {
        case PackedPrimitiveType::sphere: {
            const PackedSphere& sphere = scene.spheres[reference.primitive_index];
            try_hit_sphere(sphere, ray, t_min, closest, candidate, candidate_hit);
            material_index = sphere.material_index;
            candidate.primitive_type = static_cast<int>(PackedLightType::sphere);
            break;
        }
        case PackedPrimitiveType::quad: {
            const PackedQuad& quad = scene.quads[reference.primitive_index];
            try_hit_quad(quad, ray, t_min, closest, candidate, candidate_hit);
            material_index = quad.material_index;
            candidate.primitive_type = static_cast<int>(PackedLightType::quad);
            break;
        }
        case PackedPrimitiveType::triangle: {
            const PackedTriangle& triangle = scene.triangles[reference.primitive_index];
            try_hit_triangle(triangle, ray, t_min, closest, candidate, candidate_hit);
            material_index = triangle.material_index;
            candidate.primitive_type = static_cast<int>(PackedLightType::triangle);
            break;
        }
    }
    if (!candidate_hit) {
        return;
    }
    candidate.primitive_index = reference.primitive_index;
    populate_material(scene, material_index, candidate);
    closest = candidate.t;
    hit = candidate;
    found = true;
}

__device__ void intersect_accelerated_surfaces(const DeviceSceneView& scene, const Ray& ray,
    float t_min, float& closest, HitInfo& hit, bool& found) {
    constexpr int kTraversalStackSize = 64;
    int stack[kTraversalStackSize];
    int stack_size = 1;
    stack[0] = 0;
    while (stack_size > 0) {
        const int node_index = stack[--stack_size];
        if (node_index < 0 || node_index >= scene.acceleration_node_count) {
            continue;
        }
        const PackedBvhNode& node = scene.acceleration_nodes[node_index];
        if (!hit_acceleration_bounds(node, ray, t_min, closest)) {
            continue;
        }
        if (node.reference_count > 0) {
            for (int i = 0; i < node.reference_count; ++i) {
                const int reference_index = node.first_reference + i;
                if (reference_index < 0 || reference_index >= scene.acceleration_reference_count) {
                    continue;
                }
                try_hit_acceleration_reference(scene,
                    scene.acceleration_references[reference_index], ray, t_min, closest, hit,
                    found);
            }
            continue;
        }
        if (stack_size + 2 > kTraversalStackSize) {
            continue;
        }
        stack[stack_size++] = node.right_child;
        stack[stack_size++] = node.left_child;
    }
}

__device__ HitInfo intersect_scene(const DeviceSceneView& scene, const Ray& ray, float t_min,
    float t_max, std::uint32_t* rng = nullptr) {
    HitInfo hit {};
    float closest = t_max;
    bool found = false;

    if (scene.acceleration_nodes != nullptr && scene.acceleration_references != nullptr
        && scene.acceleration_node_count > 0 && scene.acceleration_reference_count > 0) {
        intersect_accelerated_surfaces(scene, ray, t_min, closest, hit, found);
    } else {
        for (int i = 0; i < scene.sphere_count; ++i) {
            const PackedSphere& sphere = scene.spheres[i];
            HitInfo candidate {};
            bool candidate_hit = false;
            try_hit_sphere(sphere, ray, t_min, closest, candidate, candidate_hit);
            if (!candidate_hit) {
                continue;
            }
            candidate.primitive_type = static_cast<int>(PackedLightType::sphere);
            candidate.primitive_index = i;
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
            candidate.primitive_type = static_cast<int>(PackedLightType::quad);
            candidate.primitive_index = i;
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
            candidate.primitive_type = static_cast<int>(PackedLightType::triangle);
            candidate.primitive_index = i;
            populate_material(scene, triangle.material_index, candidate);
            closest = candidate.t;
            hit = candidate;
            found = true;
        }
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

__device__ void store_output(const LaunchParams& params, int pixel_index, const float3& beauty,
    const float3& normal, const float3& albedo, float depth) {
    if (params.frame.beauty != nullptr) {
        params.frame.beauty[pixel_index] = make_float4(beauty.x, beauty.y, beauty.z, 1.0f);
    }
    if (params.frame.normal != nullptr) {
        const float3 encoded =
            clamp3(add3(mul3(normal, 0.5f), make_float3(0.5f, 0.5f, 0.5f)), 0.0f, 1.0f);
        params.frame.normal[pixel_index] = make_float4(encoded.x, encoded.y, encoded.z, 1.0f);
    }
    if (params.frame.denoiser_normal != nullptr) {
        const float3 basis_x = make_float3(static_cast<float>(params.active_camera.basis_x[0]),
            static_cast<float>(params.active_camera.basis_x[1]),
            static_cast<float>(params.active_camera.basis_x[2]));
        const float3 basis_y = make_float3(static_cast<float>(params.active_camera.basis_y[0]),
            static_cast<float>(params.active_camera.basis_y[1]),
            static_cast<float>(params.active_camera.basis_y[2]));
        const float3 basis_z = make_float3(static_cast<float>(params.active_camera.basis_z[0]),
            static_cast<float>(params.active_camera.basis_z[1]),
            static_cast<float>(params.active_camera.basis_z[2]));
        const float3 camera_normal = normalize3(
            make_float3(dot3(normal, basis_x), dot3(normal, basis_y), dot3(normal, basis_z)));
        params.frame.denoiser_normal[pixel_index] =
            make_float4(camera_normal.x, camera_normal.y, camera_normal.z, 1.0f);
    }
    if (params.frame.albedo != nullptr) {
        const float3 clamped = clamp3(albedo, 0.0f, 1.0f);
        params.frame.albedo[pixel_index] = make_float4(clamped.x, clamped.y, clamped.z, 1.0f);
    }
    if (params.frame.depth != nullptr) {
        params.frame.depth[pixel_index] = depth;
    }
}

__global__ void direction_debug_kernel(const DeviceActiveCamera* camera_ptr, std::uint8_t* rgba,
    int width, int height) {
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
            const AnalyticLightHit analytic_hit = intersect_analytic_lights(params.scene, state.ray,
                kRayEpsilon, hit.hit ? hit.t : kRayFar);
            if (analytic_hit.hit) {
                const PackedAnalyticLight& light =
                    params.scene.analytic_lights[analytic_hit.light_index];
                const float weight =
                    analytic_emission_mis_weight(params.scene, state, analytic_hit);
                state.radiance = add3(state.radiance,
                    mul3(state.throughput,
                        mul3(packed_light_vector_to_float3(light.radiance), weight)));
                state.alive = false;
                break;
            }
            if (!hit.hit) {
                const float weight = emission_mis_weight(params.scene, state, nullptr, true);
                state.radiance = add3(state.radiance,
                    mul3(state.throughput, mul3(scene_background(params), weight)));
                state.radiance = add3(state.radiance,
                    mul3(state.throughput, analytic_infinite_radiance(params.scene, state)));
                state.alive = false;
                break;
            }
            if (state.subsurface_medium.active != 0) {
                if (hit.material_type != 5 || hit.openpbr_index != state.subsurface_material_index
                    || hit.front_face) {
                    state.alive = false;
                    break;
                }
                const float ray_length = sqrtf(length_sq3(state.ray.direction));
                const OpenPbrSubsurfaceSegment segment =
                    openpbr_sample_subsurface_segment(state.subsurface_medium, hit.t * ray_length,
                        random_float01(rng), openpbr_context_for(state));
                state.throughput = mul3(state.throughput, from_openpbr_vec3(segment.weight));
                if (segment.scattered != 0) {
                    const float3 incident = normalize3(state.ray.direction);
                    const OpenPbrVec3 direction = openpbr_sample_henyey_greenstein(
                        to_openpbr_vec3(incident), state.subsurface_medium.anisotropy,
                        random_float01(rng), random_float01(rng));
                    state.ray.origin = add3(state.ray.origin,
                        mul3(state.ray.direction, segment.distance / fmaxf(ray_length, 1e-8f)));
                    state.ray.direction = from_openpbr_vec3(direction);
                    state.ray.origin =
                        add3(state.ray.origin, mul3(state.ray.direction, kRayEpsilon));
                    state.previous_scatter_valid = false;
                    continue;
                }
            }
            if (!captured_aux && bounce == 0) {
                sample_normal = hit.shading_normal;
                sample_albedo = hit.base_color;
                sample_depth = hit.t;
                captured_aux = true;
            }

            const float emission_weight = emission_mis_weight(params.scene, state, &hit, false);
            state.radiance =
                add3(state.radiance, mul3(state.throughput, mul3(hit.emission, emission_weight)));
            const bool bsdf_technique_available = bounce + 1 < max_bounces;
            accumulate_direct_light(params, hit, rng, bsdf_technique_available, state);
            const int restir_min_lights =
                params.restir_min_analytic_lights > 0 ? params.restir_min_analytic_lights : 1;
            const bool use_restir = params.restir_di_enabled != 0 && bounce == 0
                                    && params.scene.analytic_light_count >= restir_min_lights;
            if (use_restir) {
                accumulate_restir_analytic_direct_light(params, hit, x, y, rng,
                    bsdf_technique_available, state);
            } else {
                accumulate_analytic_direct_light(params, hit, rng, bsdf_technique_available, state);
            }
            if (!bsdf_technique_available) {
                state.alive = false;
                break;
            }
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
    const float3 normal_avg =
        aux_count > 0 ? normalize3(mul3(normal_sum, inv_aux)) : make_float3(0.0f, 0.0f, 1.0f);
    const float3 albedo_avg =
        aux_count > 0 ? mul3(albedo_sum, inv_aux) : make_float3(0.0f, 0.0f, 0.0f);
    const float depth_avg = aux_count > 0 ? depth_sum * inv_aux : 0.0f;
    store_output(params, pixel_index, beauty_avg, normal_avg, albedo_avg, depth_avg);
}

__global__ void resolve_reprojection_kernel(const LaunchParams* params_ptr) {
    const LaunchParams& params = *params_ptr;
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= params.width || y >= params.height) {
        return;
    }

    const int pixel_index = y * params.width + x;

    if (params.frame.flow != nullptr) {
        params.frame.flow[pixel_index] = make_float2(0.0f, 0.0f);
    }
    if (params.frame.flow_trustworthiness != nullptr) {
        params.frame.flow_trustworthiness[pixel_index] = 0.0f;
    }

    if (params.history_length <= 0 || params.history.beauty == nullptr
        || params.history.normal == nullptr || params.history.depth == nullptr) {
        return;
    }

    const float4 current_beauty = params.frame.beauty[pixel_index];
    const float4 current_normal = params.frame.normal[pixel_index];
    const float current_depth = params.frame.depth[pixel_index];

    // Reconstruct world-space position from current depth and current camera
    const float pixel_x = static_cast<float>(x) + 0.5f;
    const float pixel_y = static_cast<float>(y) + 0.5f;
    const float3 dir_camera = unproject_camera_ray(params.active_camera, pixel_x, pixel_y);
    const float3 dir_world = transform_direction(params.active_camera, dir_camera);
    const float3 origin = camera_origin(params.active_camera);
    const float3 world_pos = add3(origin, mul3(dir_world, current_depth));

    // Transform world position to previous camera space
    const float3 world_offset = make_float3(static_cast<float>(world_pos.x - params.prev_origin[0]),
        static_cast<float>(world_pos.y - params.prev_origin[1]),
        static_cast<float>(world_pos.z - params.prev_origin[2]));

    const float3 bx = make_float3(static_cast<float>(params.prev_basis_x[0]),
        static_cast<float>(params.prev_basis_x[1]), static_cast<float>(params.prev_basis_x[2]));
    const float3 by = make_float3(static_cast<float>(params.prev_basis_y[0]),
        static_cast<float>(params.prev_basis_y[1]), static_cast<float>(params.prev_basis_y[2]));
    const float3 bz = make_float3(static_cast<float>(params.prev_basis_z[0]),
        static_cast<float>(params.prev_basis_z[1]), static_cast<float>(params.prev_basis_z[2]));

    // prev_cam_space = (dot(offset, bx), dot(offset, by), dot(offset, bz))
    const float3 prev_cam_space =
        make_float3(dot3(world_offset, bx), dot3(world_offset, by), dot3(world_offset, bz));

    const DeviceActiveCamera& previous_camera =
        params.previous_camera_valid != 0 ? params.previous_camera : params.active_camera;
    const float2 prev_pixel = project_camera_pixel(previous_camera, prev_cam_space);

    const bool previous_pixel_finite = isfinite(prev_pixel.x) && isfinite(prev_pixel.y);
    if (params.frame.flow != nullptr && previous_pixel_finite) {
        params.frame.flow[pixel_index] =
            make_float2(pixel_x - prev_pixel.x, pixel_y - prev_pixel.y);
    }

    const int prev_x = previous_pixel_finite ? static_cast<int>(floorf(prev_pixel.x)) : -1;
    const int prev_y = previous_pixel_finite ? static_cast<int>(floorf(prev_pixel.y)) : -1;

    bool valid = false;
    if (previous_pixel_finite && prev_x >= 0 && prev_x < params.width && prev_y >= 0
        && prev_y < params.height) {
        const int prev_idx = prev_y * params.width + prev_x;

        const float3 history_normal = decode_normal(params.history.normal[prev_idx]);
        const float3 curr_normal_decoded = decode_normal(current_normal);
        const float normal_dot = dot3(curr_normal_decoded, history_normal);

        const float history_depth = params.history.depth[prev_idx];
        const float expected_history_depth = length3(prev_cam_space);
        const float depth_tolerance =
            fmaxf(0.02f, 0.05f * fmaxf(expected_history_depth, history_depth));

        if (normal_dot > 0.95f && fabsf(expected_history_depth - history_depth) < depth_tolerance) {
            valid = true;
        }
    }

    if (valid) {
        if (params.frame.flow_trustworthiness != nullptr) {
            params.frame.flow_trustworthiness[pixel_index] = 1.0f;
        }
        const int prev_idx = prev_y * params.width + prev_x;
        const float4 h_beauty = params.history.beauty[prev_idx];
        const float4 h_normal = params.history.normal[prev_idx];
        const float h_depth = params.history.depth[prev_idx];
        const int next_len = params.history_length + 1;
        const float blend = 1.0f / static_cast<float>(next_len);

        const float4 clamped_current = make_float4(fminf(fmaxf(current_beauty.x, 0.0f), 10.0f),
            fminf(fmaxf(current_beauty.y, 0.0f), 10.0f),
            fminf(fmaxf(current_beauty.z, 0.0f), 10.0f), 1.0f);

        if (params.restir_di_enabled == 0) {
            params.frame.beauty[pixel_index] =
                make_float4(h_beauty.x + (clamped_current.x - h_beauty.x) * blend,
                    h_beauty.y + (clamped_current.y - h_beauty.y) * blend,
                    h_beauty.z + (clamped_current.z - h_beauty.z) * blend, 1.0f);
        }

        params.frame.normal[pixel_index] =
            make_float4(h_normal.x + (current_normal.x - h_normal.x) * blend,
                h_normal.y + (current_normal.y - h_normal.y) * blend,
                h_normal.z + (current_normal.z - h_normal.z) * blend,
                h_normal.w + (current_normal.w - h_normal.w) * blend);

        params.frame.depth[pixel_index] = h_depth + (current_depth - h_depth) * blend;
    }
}

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA kernel launch failure at ") + expr + ": "
                                 + cudaGetErrorString(error));
    }
}

dim3 make_grid(int width, int height, const dim3& block_size) {
    return dim3(static_cast<unsigned int>(
                    (width + static_cast<int>(block_size.x) - 1) / static_cast<int>(block_size.x)),
        static_cast<unsigned int>(
            (height + static_cast<int>(block_size.y) - 1) / static_cast<int>(block_size.y)),
        1);
}

} // namespace

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

__device__ PackedLightVector3 restir_vector(const float3& value) {
    return PackedLightVector3 {.x = value.x, .y = value.y, .z = value.z};
}

__device__ RestirSurface restir_surface(const HitInfo& hit) {
    return RestirSurface {
        .position = restir_vector(hit.position),
        .normal = restir_vector(hit.shading_normal),
        .material_type = hit.material_type,
        .primitive_type = hit.primitive_type,
        .primitive_index = hit.primitive_index,
    };
}

__device__ float restir_luminance(const float3& value) {
    return fmaxf(0.0f, 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z);
}

__device__ float3 restir_unshadowed_numerator(const LaunchParams& params, const HitInfo& hit,
    const PathState& state, const DirectLightSample& light, bool bsdf_technique_available,
    float& target_density) {
    target_density = 0.0f;
    if (!light.valid || light.pdf <= 0.0f) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    float bsdf_pdf = 0.0f;
    const float3 response =
        direct_material_response(params.scene, hit, state, light.direction, bsdf_pdf);
    if (max_component3(response) <= 0.0f) {
        return make_float3(0.0f, 0.0f, 0.0f);
    }
    const float mis_weight = light.delta || !bsdf_technique_available
                                 ? 1.0f
                                 : light_power_heuristic(light.pdf, bsdf_pdf);
    const float3 numerator = mul3(mul3(light.emission, response), mis_weight);
    target_density = restir_luminance(numerator);
    return numerator;
}

__device__ bool restir_previous_pixel(const LaunchParams& params, const HitInfo& hit, int& index) {
    index = -1;
    if (params.history_length <= 0 || params.previous_camera_valid == 0
        || params.history.restir_reservoirs == nullptr) {
        return false;
    }
    const DeviceActiveCamera& camera = params.previous_camera;
    const float3 offset = sub3(hit.position, camera_origin(camera));
    const float3 basis_x = make_float3(static_cast<float>(camera.basis_x[0]),
        static_cast<float>(camera.basis_x[1]), static_cast<float>(camera.basis_x[2]));
    const float3 basis_y = make_float3(static_cast<float>(camera.basis_y[0]),
        static_cast<float>(camera.basis_y[1]), static_cast<float>(camera.basis_y[2]));
    const float3 basis_z = make_float3(static_cast<float>(camera.basis_z[0]),
        static_cast<float>(camera.basis_z[1]), static_cast<float>(camera.basis_z[2]));
    const float3 camera_point =
        make_float3(dot3(offset, basis_x), dot3(offset, basis_y), dot3(offset, basis_z));
    const float2 pixel = project_camera_pixel(camera, camera_point);
    if (!isfinite(pixel.x) || !isfinite(pixel.y)) {
        return false;
    }
    const int x = static_cast<int>(floorf(pixel.x));
    const int y = static_cast<int>(floorf(pixel.y));
    if (x < 0 || x >= params.width || y < 0 || y >= params.height) {
        return false;
    }
    index = y * params.width + x;
    return true;
}

__device__ bool restir_temporal_candidate(const LaunchParams& params, const HitInfo& hit,
    RestirReservoir& previous) {
    int previous_index = -1;
    if (!restir_previous_pixel(params, hit, previous_index)) {
        return false;
    }
    previous = params.history.restir_reservoirs[previous_index];
    const float position_tolerance = fmaxf(0.005f, 0.01f * hit.t);
    return restir_temporal_surface_valid(restir_surface(hit), previous, position_tolerance, 0.95f,
        params.restir_max_history_age);
}

__device__ void accumulate_direct_light(const LaunchParams& params, const HitInfo& hit,
    std::uint32_t& rng, bool bsdf_technique_available, PathState& state) {
    if (hit.material_type == 3 || params.scene.light_count <= 0) {
        return;
    }

    const DirectLightSample light =
        sample_direct_light(params.scene, scene_background(params), hit, rng);
    if (!light.valid) {
        return;
    }

    float bsdf_pdf = 0.0f;
    const float3 response =
        direct_material_response(params.scene, hit, state, light.direction, bsdf_pdf);
    if (max_component3(response) <= 0.0f) {
        return;
    }

    Ray shadow_ray {};
    shadow_ray.origin = add3(hit.position, mul3(light.direction, kRayEpsilon * 2.0f));
    shadow_ray.direction = light.direction;
    const float max_t = light.infinite ? kRayFar : light.distance - kRayEpsilon * 3.0f;
    if (max_t <= kRayEpsilon || is_occluded(params.scene, shadow_ray, max_t)) {
        return;
    }

    const float mis_weight =
        bsdf_technique_available ? light_power_heuristic(light.pdf, bsdf_pdf) : 1.0f;
    const float3 direct = mul3(mul3(light.emission, response), mis_weight / light.pdf);
    state.radiance = add3(state.radiance, mul3(state.throughput, direct));
}

__device__ void accumulate_analytic_direct_light(const LaunchParams& params, const HitInfo& hit,
    std::uint32_t& rng, bool bsdf_technique_available, PathState& state) {
    if (hit.material_type == 3 || params.scene.analytic_light_count <= 0) {
        return;
    }

    const DirectLightSample light = sample_analytic_direct_light(params.scene, hit, rng);
    if (!light.valid) {
        return;
    }

    float bsdf_pdf = 0.0f;
    const float3 response =
        direct_material_response(params.scene, hit, state, light.direction, bsdf_pdf);
    if (max_component3(response) <= 0.0f) {
        return;
    }

    Ray shadow_ray {};
    shadow_ray.origin = add3(hit.position, mul3(light.direction, kRayEpsilon * 2.0f));
    shadow_ray.direction = light.direction;
    const float max_t = light.infinite ? kRayFar : light.distance - kRayEpsilon * 3.0f;
    if (max_t <= kRayEpsilon || is_occluded(params.scene, shadow_ray, max_t)) {
        return;
    }

    const float mis_weight = light.delta || !bsdf_technique_available
                                 ? 1.0f
                                 : light_power_heuristic(light.pdf, bsdf_pdf);
    const float3 direct = mul3(mul3(light.emission, response), mis_weight / light.pdf);
    state.radiance = add3(state.radiance, mul3(state.throughput, direct));
}

__device__ void accumulate_restir_analytic_direct_light(const LaunchParams& params,
    const HitInfo& hit, int x, int y, std::uint32_t& rng, bool bsdf_technique_available,
    PathState& state) {
    if (hit.material_type == 3 || params.scene.analytic_light_count <= 0
        || params.frame.restir_reservoirs == nullptr) {
        return;
    }

    RestirReservoir reservoir {};
    const int requested_candidates =
        params.restir_initial_candidates > 0 ? params.restir_initial_candidates : 1;
    const int candidate_count = requested_candidates < 32 ? requested_candidates : 32;
    for (int candidate_index = 0; candidate_index < candidate_count; ++candidate_index) {
        const int light_index = sample_packed_analytic_light(params.scene.analytic_lights,
            params.scene.analytic_light_count, random_float01(rng));
        const RestirCandidate candidate {
            .light_index = light_index,
            .sample_u0 = random_float01(rng),
            .sample_u1 = random_float01(rng),
        };
        const DirectLightSample light = sample_analytic_direct_light_candidate(params.scene, hit,
            candidate.light_index, candidate.sample_u0, candidate.sample_u1);
        float target_density = 0.0f;
        restir_unshadowed_numerator(params, hit, state, light, bsdf_technique_available,
            target_density);
        const float candidate_weight = light.pdf > 0.0f ? target_density / light.pdf : 0.0f;
        restir_update(reservoir, candidate, target_density, candidate_weight, 1,
            random_float01(rng));
    }

    if (params.restir_temporal_reuse != 0) {
        RestirReservoir previous {};
        if (restir_temporal_candidate(params, hit, previous)) {
            const DirectLightSample light = sample_analytic_direct_light_candidate(params.scene,
                hit, previous.selected.light_index, previous.selected.sample_u0,
                previous.selected.sample_u1);
            float target_density = 0.0f;
            restir_unshadowed_numerator(params, hit, state, light, bsdf_technique_available,
                target_density);
            restir_merge_temporal(reservoir, previous, target_density,
                params.restir_max_temporal_candidates, random_float01(rng));
        }
    }

    reservoir.surface = restir_surface(hit);
    restir_finalize(reservoir);
    params.frame.restir_reservoirs[y * params.width + x] = reservoir;
    if (reservoir.valid == 0) {
        return;
    }

    const DirectLightSample light = sample_analytic_direct_light_candidate(params.scene, hit,
        reservoir.selected.light_index, reservoir.selected.sample_u0, reservoir.selected.sample_u1);
    float target_density = 0.0f;
    const float3 numerator = restir_unshadowed_numerator(params, hit, state, light,
        bsdf_technique_available, target_density);
    if (!light.valid || target_density <= 0.0f) {
        return;
    }

    Ray shadow_ray {};
    shadow_ray.origin = add3(hit.position, mul3(light.direction, kRayEpsilon * 2.0f));
    shadow_ray.direction = light.direction;
    const float max_t = light.infinite ? kRayFar : light.distance - kRayEpsilon * 3.0f;
    if (max_t <= kRayEpsilon || is_occluded(params.scene, shadow_ray, max_t)) {
        return;
    }
    const float3 direct = mul3(numerator, reservoir.estimator_weight);
    state.radiance = add3(state.radiance, mul3(state.throughput, direct));
}

__device__ void record_scatter_for_mis(PathState& state, const HitInfo& hit, float bsdf_pdf,
    bool delta) {
    state.previous_scatter_position = hit.position;
    state.previous_scatter_normal = hit.shading_normal;
    state.previous_bsdf_pdf = bsdf_pdf;
    state.previous_material_type = hit.material_type;
    state.previous_primitive_type = hit.primitive_type;
    state.previous_primitive_index = hit.primitive_index;
    state.previous_scatter_valid = true;
    state.previous_scatter_delta = delta;
}

__device__ void sample_bsdf(const LaunchParams& params, const HitInfo& hit, std::uint32_t& rng,
    PathState& state) {
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
        const float cosine = fmaxf(dot3(hit.shading_normal, state.ray.direction), 0.0f);
        record_scatter_for_mis(state, hit, cosine / kPi, false);
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
        record_scatter_for_mis(state, hit, 0.0f, true);
        return;
    }

    if (hit.material_type == 2) {
        const float refraction_ratio =
            hit.front_face ? (1.0f / fmaxf(hit.ior, 1e-4f)) : fmaxf(hit.ior, 1e-4f);
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
        record_scatter_for_mis(state, hit, 0.0f, true);
        return;
    }

    if (hit.material_type == 5) {
        const OpenPbrCompiledMaterial* compiled =
            resolve_openpbr_material(params.scene, hit.openpbr_index);
        if (compiled == nullptr) {
            state.alive = false;
            return;
        }
        const OpenPbrCoreMaterial parameters =
            evaluate_openpbr_scattering_material(params.scene, *compiled, hit);
        const OpenPbrFrame frame =
            make_openpbr_frame(to_openpbr_vec3(hit.geometric_normal), OpenPbrVec3 {});
        const OpenPbrSample sample = sample_openpbr_core(parameters, frame,
            to_openpbr_vec3(mul3(normalize3(state.ray.direction), -1.0f)), random_float01(rng),
            random_float01(rng), random_float01(rng), openpbr_context_for(state));
        if (sample.valid == 0) {
            state.alive = false;
            return;
        }
        const float3 direction = from_openpbr_vec3(sample.wi);
        state.ray.origin = add3(hit.position, mul3(direction, kRayEpsilon));
        state.ray.direction = normalize3(direction);
        state.throughput = mul3(state.throughput, from_openpbr_vec3(sample.weight));
        record_scatter_for_mis(state, hit, sample.pdf, sample.delta != 0);
        if (sample.event == OpenPbrScatterEvent::subsurface_entry) {
            state.subsurface_medium = openpbr_subsurface_medium(parameters);
            state.subsurface_material_index = hit.openpbr_index;
        } else if (sample.event == OpenPbrScatterEvent::subsurface_exit) {
            state.subsurface_medium = {};
            state.subsurface_material_index = -1;
        }
        return;
    }

    if (hit.material_type == 4) {
        const float3 direction = random_unit_vector(rng);
        state.ray.origin = add3(hit.position, mul3(direction, kRayEpsilon));
        state.ray.direction = direction;
        state.throughput = mul3(state.throughput, hit.base_color);
        record_scatter_for_mis(state, hit, light_uniform_sphere_pdf(), false);
        return;
    }

    state.alive = false;
}

void launch_direction_debug_kernel(const DeviceActiveCamera& camera, std::uint8_t* rgba, int width,
    int height, cudaStream_t stream) {
    DeviceActiveCamera* device_camera = nullptr;
    throw_cuda_error(
        cudaMalloc(reinterpret_cast<void**>(&device_camera), sizeof(DeviceActiveCamera)),
        "cudaMalloc()");
    const dim3 block_size(16, 16, 1);
    try {
        throw_cuda_error(cudaMemcpyAsync(device_camera, &camera, sizeof(DeviceActiveCamera),
                             cudaMemcpyHostToDevice, stream),
            "cudaMemcpyAsync()");
        direction_debug_kernel<<<make_grid(width, height, block_size), block_size, 0, stream>>>(
            device_camera, rgba, width, height);
        throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
        throw_cuda_error(cudaFree(device_camera), "cudaFree()");
    } catch (...) {
        cudaFree(device_camera);
        throw;
    }
}

void upload_launch_params(LaunchParams* device_params, const LaunchParams& params,
    cudaStream_t stream) {
    if (device_params == nullptr) {
        throw std::invalid_argument("upload_launch_params requires persistent device storage");
    }
    throw_cuda_error(cudaMemcpyAsync(device_params, &params, sizeof(LaunchParams),
                         cudaMemcpyHostToDevice, stream),
        "cudaMemcpyAsync()");
}

void launch_radiance_kernel(const LaunchParams& params, const LaunchParams* device_params,
    cudaStream_t stream) {
    const dim3 block_size(8, 8, 1);
    radiance_kernel<<<make_grid(params.width, params.height, block_size), block_size, 0, stream>>>(
        device_params);
    throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
}

void launch_resolve_kernel(const LaunchParams& params, const LaunchParams* device_params,
    cudaStream_t stream) {
    const dim3 block_size(8, 8, 1);
    resolve_reprojection_kernel<<<make_grid(params.width, params.height, block_size), block_size, 0,
        stream>>>(device_params);
    throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
}

} // namespace rt
