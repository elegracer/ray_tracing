#pragma once

#include "common/light_sampling.h"
#include "common/openpbr_core.h"
#include "common/restir_di.h"
#include "realtime/camera_rig.h"
#include "realtime/gpu/frame_types.h"

#include <Eigen/Core>

#include <cstdint>
#include <vector_types.h>

namespace rt {

struct PackedSphere;
struct PackedQuad;
struct PackedTriangle;
struct PackedMedium;
struct PackedTexture;
struct MaterialSample;
struct PackedBvhNode;
struct PackedPrimitiveRef;

struct DeviceFrameBuffers {
    float4* beauty = nullptr;
    float4* normal = nullptr;
    float4* denoiser_normal = nullptr;
    float4* albedo = nullptr;
    float* depth = nullptr;
    float2* flow = nullptr;
    float* flow_trustworthiness = nullptr;
    RestirReservoir* restir_reservoirs = nullptr;
};

struct DeviceSceneView {
    PackedSphere* spheres = nullptr;
    PackedQuad* quads = nullptr;
    PackedTriangle* triangles = nullptr;
    PackedMedium* media = nullptr;
    PackedTexture* textures = nullptr;
    Eigen::Vector3f* image_texels = nullptr;
    MaterialSample* materials = nullptr;
    OpenPbrCompiledMaterial* openpbr_materials = nullptr;
    PackedLight* lights = nullptr;
    PackedAnalyticLight* analytic_lights = nullptr;
    PackedBvhNode* acceleration_nodes = nullptr;
    PackedPrimitiveRef* acceleration_references = nullptr;
    int sphere_count = 0;
    int quad_count = 0;
    int triangle_count = 0;
    int medium_count = 0;
    int texture_count = 0;
    int image_texel_count = 0;
    int material_count = 0;
    int openpbr_material_count = 0;
    int light_count = 0;
    int analytic_light_count = 0;
    int acceleration_node_count = 0;
    int acceleration_reference_count = 0;
};

struct DevicePinhole32Params {
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double k1 = 0.0;
    double k2 = 0.0;
    double k3 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
};

struct DeviceEqui62Lut1DParams {
    int width = 0;
    int height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double radial[6] {};
    double tangential[2] {};
    double lut[1024] {};
    double lut_step = 0.0;
};

struct DeviceActiveCamera {
    int width = 0;
    int height = 0;
    CameraModelType model = CameraModelType::pinhole32;
    double origin[3] {};
    double basis_x[3] {};
    double basis_y[3] {};
    double basis_z[3] {};
    DevicePinhole32Params pinhole {};
    DeviceEqui62Lut1DParams equi {};
};

struct LaunchParams {
    DeviceFrameBuffers frame {};
    DeviceSceneView scene {};
    DeviceActiveCamera active_camera {};
    float background[3] {};
    int width = 0;
    int height = 0;
    std::uint32_t sample_stream = 0;
    int samples_per_pixel = 1;
    int max_bounces = 4;
    int rr_start_bounce = 3;
    int mode = 0;

    // --- ReSTIR DI ---
    int restir_di_enabled = 0;
    int restir_initial_candidates = 1;
    int restir_temporal_reuse = 0;
    int restir_max_history_age = 0;
    int restir_max_temporal_candidates = 0;
    int restir_min_analytic_lights = 0;

    // --- temporal reprojection ---
    DeviceFrameBuffers history {};
    DeviceActiveCamera previous_camera {};
    int previous_camera_valid = 0;
    double prev_origin[3] {};
    double prev_basis_x[3] {};
    double prev_basis_y[3] {};
    double prev_basis_z[3] {};
    int history_length = 0;
};

struct PackedSphere {
    Eigen::Vector3f center;
    float radius;
    int material_index;
    int acceleration_prototype_id = -1;
    int acceleration_instance_id = -1;
};

struct PackedQuad {
    Eigen::Vector3f origin;
    float pad0 = 0.0f;
    Eigen::Vector3f edge_u;
    float pad1 = 0.0f;
    Eigen::Vector3f edge_v;
    int material_index;
    int acceleration_prototype_id = -1;
    int acceleration_instance_id = -1;
};

struct PackedTriangle {
    Eigen::Vector3f p0;
    float pad0 = 0.0f;
    Eigen::Vector3f p1;
    float pad1 = 0.0f;
    Eigen::Vector3f p2;
    int material_index = -1;
    Eigen::Vector3f n0;
    int has_vertex_normals = 0;
    Eigen::Vector3f n1;
    int has_texcoords = 0;
    Eigen::Vector3f n2;
    float pad2 = 0.0f;
    Eigen::Vector2f uv0;
    Eigen::Vector2f uv1;
    Eigen::Vector2f uv2;
    Eigen::Vector2f pad3 = Eigen::Vector2f::Zero();
    int acceleration_prototype_id = -1;
    int acceleration_instance_id = -1;
    int acceleration_pad0 = 0;
    int acceleration_pad1 = 0;
};

enum class PackedPrimitiveType : int {
    sphere = 0,
    quad = 1,
    triangle = 2,
};

struct PackedPrimitiveRef {
    int primitive_type = 0;
    int primitive_index = -1;
    int prototype_id = -1;
    int instance_id = -1;
};

struct PackedBvhNode {
    Eigen::Vector3f bounds_min = Eigen::Vector3f::Zero();
    int left_child = -1;
    Eigen::Vector3f bounds_max = Eigen::Vector3f::Zero();
    int right_child = -1;
    int first_reference = 0;
    int reference_count = 0;
    int pad0 = 0;
    int pad1 = 0;
};

struct PackedMedium {
    Eigen::Vector3f local_center_or_min;
    float radius = 0.0f;
    Eigen::Vector3f local_max;
    float density = 0.0f;
    Eigen::Vector3f rotation_row0;
    int material_index = -1;
    Eigen::Vector3f rotation_row1;
    int boundary_type = 0;
    Eigen::Vector3f rotation_row2;
    float pad0 = 0.0f;
    Eigen::Vector3f translation;
    float pad1 = 0.0f;
};

struct MaterialSample {
    int albedo_texture = -1;
    int emission_texture = -1;
    float ior = 1.0f;
    float fuzz = 0.0f;
    int type = 0;
    int openpbr_index = -1;
};

struct PackedTexture {
    int type = 0;
    int even_texture = -1;
    int odd_texture = -1;
    int image_offset = 0;
    int image_width = 0;
    int image_height = 0;
    int u_address_mode = 1;
    int v_address_mode = 1;
    int filter_type = 1;
    float scale = 1.0f;
    Eigen::Vector3f color = Eigen::Vector3f::Zero();
};

} // namespace rt
