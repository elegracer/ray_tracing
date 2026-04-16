#include "realtime/gpu/optix_renderer.h"

#include <optix_function_table_definition.h>
#include <optix_stubs.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace rt {

void launch_direction_debug_kernel(std::uint8_t* rgba, int width, int height, cudaStream_t stream);
void launch_radiance_kernel(const LaunchParams& params, cudaStream_t stream);

namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

void throw_cuda_driver_error(CUresult result, const char* expr) {
    if (result != CUDA_SUCCESS) {
        const char* name = nullptr;
        const char* message = nullptr;
        cuGetErrorName(result, &name);
        cuGetErrorString(result, &message);
        throw std::runtime_error(std::string("CUDA driver failure at ") + expr + ": "
            + (name != nullptr ? name : "unknown") + " / " + (message != nullptr ? message : "unknown"));
    }
}

void throw_optix_error(OptixResult result, const char* expr) {
    if (result != OPTIX_SUCCESS) {
        throw std::runtime_error(std::string("OptiX failure at ") + expr + ": " + std::to_string(result));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)
#define RT_CUDA_DRV_CHECK(expr) throw_cuda_driver_error((expr), #expr)
#define RT_OPTIX_CHECK(expr) throw_optix_error((expr), #expr)

void context_log_cb(unsigned int level, const char* tag, const char* message, void*) {
    (void)level;
    (void)tag;
    (void)message;
}

PackedSphere pack_sphere(const SpherePrimitive& sphere) {
    return PackedSphere {
        .center = sphere.center.cast<float>(),
        .radius = static_cast<float>(sphere.radius),
        .material_index = sphere.material_index,
    };
}

PackedQuad pack_quad(const QuadPrimitive& quad) {
    return PackedQuad {
        .origin = quad.origin.cast<float>(),
        .edge_u = quad.edge_u.cast<float>(),
        .edge_v = quad.edge_v.cast<float>(),
        .material_index = quad.material_index,
    };
}

MaterialSample pack_material(const MaterialDesc& material) {
    MaterialSample sample {};
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, LambertianMaterial>) {
                sample.albedo = value.albedo.template cast<float>();
                sample.type = 0;
            } else if constexpr (std::is_same_v<T, MetalMaterial>) {
                sample.albedo = value.albedo.template cast<float>();
                sample.fuzz = static_cast<float>(value.fuzz);
                sample.type = 1;
            } else if constexpr (std::is_same_v<T, DielectricMaterial>) {
                sample.ior = static_cast<float>(value.ior);
                sample.type = 2;
            } else if constexpr (std::is_same_v<T, DiffuseLightMaterial>) {
                sample.emission = value.emission.template cast<float>();
                sample.type = 3;
            }
        },
        material);
    return sample;
}

DeviceActiveCamera make_active_camera(const PackedCamera& camera) {
    DeviceActiveCamera active {};
    active.width = camera.width;
    active.height = camera.height;
    active.model = camera.model;

    active.origin[0] = camera.T_rc(0, 3);
    active.origin[1] = camera.T_rc(1, 3);
    active.origin[2] = camera.T_rc(2, 3);

    for (int row = 0; row < 3; ++row) {
        active.basis_x[row] = camera.T_rc(row, 0);
        active.basis_y[row] = camera.T_rc(row, 1);
        active.basis_z[row] = camera.T_rc(row, 2);
    }

    active.pinhole.fx = camera.pinhole.fx;
    active.pinhole.fy = camera.pinhole.fy;
    active.pinhole.cx = camera.pinhole.cx;
    active.pinhole.cy = camera.pinhole.cy;
    active.pinhole.k1 = camera.pinhole.k1;
    active.pinhole.k2 = camera.pinhole.k2;
    active.pinhole.k3 = camera.pinhole.k3;
    active.pinhole.p1 = camera.pinhole.p1;
    active.pinhole.p2 = camera.pinhole.p2;

    active.equi.width = camera.equi.width;
    active.equi.height = camera.equi.height;
    active.equi.fx = camera.equi.fx;
    active.equi.fy = camera.equi.fy;
    active.equi.cx = camera.equi.cx;
    active.equi.cy = camera.equi.cy;
    active.equi.tangential[0] = camera.equi.tangential.x();
    active.equi.tangential[1] = camera.equi.tangential.y();
    active.equi.lut_step = camera.equi.lut_step;
    for (int i = 0; i < 6; ++i) {
        active.equi.radial[i] = camera.equi.radial[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < 1024; ++i) {
        active.equi.lut[i] = camera.equi.lut[static_cast<std::size_t>(i)];
    }

    return active;
}

void free_device_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFree(ptr);
    }
}

void free_frame_buffers(DeviceFrameBuffers& frame) {
    free_device_ptr(frame.beauty);
    free_device_ptr(frame.normal);
    free_device_ptr(frame.albedo);
    free_device_ptr(frame.depth);
    frame = DeviceFrameBuffers {};
}

void free_scene_buffers(PackedSphere*& spheres, PackedQuad*& quads, MaterialSample*& materials) {
    free_device_ptr(spheres);
    free_device_ptr(quads);
    free_device_ptr(materials);
    spheres = nullptr;
    quads = nullptr;
    materials = nullptr;
}

}  // namespace

OptixRenderer::OptixRenderer() {
    initialize_optix();
    create_direction_debug_pipeline();
}

OptixRenderer::~OptixRenderer() {
    free_device_resources();
    if (optix_context_ != nullptr) {
        optixDeviceContextDestroy(optix_context_);
    }
    if (stream_ != nullptr) {
        cudaStreamDestroy(stream_);
    }
}

void OptixRenderer::initialize_optix() {
    RT_CUDA_DRV_CHECK(cuInit(0));
    RT_CUDA_CHECK(cudaSetDevice(0));
    RT_CUDA_CHECK(cudaFree(nullptr));
    RT_CUDA_DRV_CHECK(cuCtxGetCurrent(&cu_context_));
    if (cu_context_ == nullptr) {
        throw std::runtime_error("CUDA context is null after initialization");
    }

    RT_OPTIX_CHECK(optixInit());

    OptixDeviceContextOptions options {};
    options.logCallbackFunction = &context_log_cb;
    options.logCallbackLevel = 4;
    RT_OPTIX_CHECK(optixDeviceContextCreate(cu_context_, &options, &optix_context_));

    RT_CUDA_CHECK(cudaStreamCreate(&stream_));
}

void OptixRenderer::create_direction_debug_pipeline() {
    if (optix_context_ == nullptr) {
        throw std::runtime_error("OptiX context is not initialized");
    }
}

void OptixRenderer::launch_direction_debug(const PackedCameraRig&, std::uint8_t* rgba, int width, int height) {
    launch_direction_debug_kernel(rgba, width, height, stream_);
}

void OptixRenderer::allocate_frame_buffers(int width, int height) {
    if (allocated_width_ == width && allocated_height_ == height) {
        return;
    }
    free_frame_buffers(device_frame_);
    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.beauty), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.normal), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.albedo), pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_frame_.depth), pixel_count * sizeof(float)));
    allocated_width_ = width;
    allocated_height_ = height;
}

DirectionDebugFrame OptixRenderer::render_direction_debug(const PackedCameraRig& rig) {
    DirectionDebugFrame frame {};
    frame.width = rig.cameras[0].width;
    frame.height = rig.cameras[0].height;
    frame.rgba.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4U, 0);

    std::uint8_t* device_rgba = nullptr;
    const std::size_t byte_count = frame.rgba.size() * sizeof(std::uint8_t);
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_rgba), byte_count));

    try {
        launch_direction_debug(rig, device_rgba, frame.width, frame.height);
        RT_CUDA_CHECK(cudaMemcpyAsync(frame.rgba.data(), device_rgba, byte_count, cudaMemcpyDeviceToHost, stream_));
        RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
        RT_CUDA_CHECK(cudaFree(device_rgba));
    } catch (...) {
        cudaFree(device_rgba);
        throw;
    }

    return frame;
}

void OptixRenderer::upload_scene(const PackedScene& scene) {
    free_scene_buffers(device_spheres_, device_quads_, device_materials_);

    if (!scene.spheres.empty()) {
        std::vector<PackedSphere> packed_spheres;
        packed_spheres.reserve(scene.spheres.size());
        for (const SpherePrimitive& sphere : scene.spheres) {
            packed_spheres.push_back(pack_sphere(sphere));
        }
        RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_spheres_),
            packed_spheres.size() * sizeof(PackedSphere)));
        RT_CUDA_CHECK(cudaMemcpy(device_spheres_, packed_spheres.data(),
            packed_spheres.size() * sizeof(PackedSphere), cudaMemcpyHostToDevice));
    }

    if (!scene.quads.empty()) {
        std::vector<PackedQuad> packed_quads;
        packed_quads.reserve(scene.quads.size());
        for (const QuadPrimitive& quad : scene.quads) {
            packed_quads.push_back(pack_quad(quad));
        }
        RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_quads_),
            packed_quads.size() * sizeof(PackedQuad)));
        RT_CUDA_CHECK(cudaMemcpy(device_quads_, packed_quads.data(),
            packed_quads.size() * sizeof(PackedQuad), cudaMemcpyHostToDevice));
    }

    if (!scene.materials.empty()) {
        std::vector<MaterialSample> packed_materials;
        packed_materials.reserve(scene.materials.size());
        for (const MaterialDesc& material : scene.materials) {
            packed_materials.push_back(pack_material(material));
        }
        RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&device_materials_),
            packed_materials.size() * sizeof(MaterialSample)));
        RT_CUDA_CHECK(cudaMemcpy(device_materials_, packed_materials.data(),
            packed_materials.size() * sizeof(MaterialSample), cudaMemcpyHostToDevice));
    }

    uploaded_scene_ = scene;
}

void OptixRenderer::free_device_resources() {
    free_frame_buffers(device_frame_);
    free_scene_buffers(device_spheres_, device_quads_, device_materials_);
    allocated_width_ = 0;
    allocated_height_ = 0;
}

void OptixRenderer::build_or_refit_accels(const PackedScene& scene) {
    if (scene.sphere_count == 0 && scene.quad_count == 0) {
        throw std::runtime_error("render_radiance requires at least one primitive");
    }
    build_geometry_accels(scene);
}

void OptixRenderer::build_geometry_accels(const PackedScene& scene) {
    sphere_gas_count_ = scene.sphere_count;
    quad_gas_count_ = scene.quad_count;
    tlas_instance_count_ = scene.sphere_count + scene.quad_count;
}

void OptixRenderer::launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    launch_radiance_pipeline(uploaded_scene_, rig, profile, camera_index);
}

void OptixRenderer::launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    LaunchParams params {};
    params.width = rig.cameras[camera_index].width;
    params.height = rig.cameras[camera_index].height;
    params.active_camera = make_active_camera(rig.cameras[camera_index]);
    allocate_frame_buffers(params.width, params.height);
    params.frame = device_frame_;
    params.scene.spheres = device_spheres_;
    params.scene.quads = device_quads_;
    params.scene.materials = device_materials_;
    params.scene.sphere_count = scene.sphere_count;
    params.scene.quad_count = scene.quad_count;
    params.scene.material_count = scene.material_count;
    params.samples_per_pixel = profile.samples_per_pixel;
    params.max_bounces = profile.max_bounces;
    params.rr_start_bounce = profile.rr_start_bounce;
    params.mode = 1;
    const std::size_t pixel_count = static_cast<std::size_t>(params.width) * static_cast<std::size_t>(params.height);
    RT_CUDA_CHECK(cudaMemset(params.frame.beauty, 0, pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMemset(params.frame.normal, 0, pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMemset(params.frame.albedo, 0, pixel_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMemset(params.frame.depth, 0, pixel_count * sizeof(float)));
    launch_radiance_kernel(params, stream_);
    uploaded_scene_ = scene;
    last_width_ = params.width;
    last_height_ = params.height;
    last_camera_index_ = camera_index;
    last_profile_ = profile;
}

RadianceFrame OptixRenderer::download_radiance_frame(int camera_index) const {
    return download_camera_frame(camera_index);
}

RadianceFrame OptixRenderer::download_camera_frame(int camera_index) const {
    RadianceFrame frame {};
    frame.width = last_launch_width(camera_index);
    frame.height = last_launch_height(camera_index);
    RT_CUDA_CHECK(cudaStreamSynchronize(stream_));
    frame.beauty_rgba = download_beauty();
    frame.normal_rgba = download_normal();
    frame.albedo_rgba = download_albedo();
    frame.depth = download_depth();
    frame.average_luminance = compute_average_luminance(frame.beauty_rgba);
    return frame;
}

int OptixRenderer::last_launch_width(int camera_index) const {
    (void)camera_index;
    return last_width_;
}

int OptixRenderer::last_launch_height(int camera_index) const {
    (void)camera_index;
    return last_height_;
}

std::vector<float> OptixRenderer::download_beauty() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    if (pixel_count == 0 || device_frame_.beauty == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame_.beauty, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    std::vector<float> rgba(pixel_count * 4U, 0.0f);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        rgba[i * 4U + 0] = host_pixels[i].x;
        rgba[i * 4U + 1] = host_pixels[i].y;
        rgba[i * 4U + 2] = host_pixels[i].z;
        rgba[i * 4U + 3] = host_pixels[i].w;
    }
    return rgba;
}

std::vector<float> OptixRenderer::download_normal() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    if (pixel_count == 0 || device_frame_.normal == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame_.normal, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    std::vector<float> rgba(pixel_count * 4U, 0.0f);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        rgba[i * 4U + 0] = host_pixels[i].x;
        rgba[i * 4U + 1] = host_pixels[i].y;
        rgba[i * 4U + 2] = host_pixels[i].z;
        rgba[i * 4U + 3] = host_pixels[i].w;
    }
    return rgba;
}

std::vector<float> OptixRenderer::download_albedo() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    if (pixel_count == 0 || device_frame_.albedo == nullptr) {
        return {};
    }
    std::vector<float4> host_pixels(pixel_count);
    RT_CUDA_CHECK(cudaMemcpy(
        host_pixels.data(), device_frame_.albedo, pixel_count * sizeof(float4), cudaMemcpyDeviceToHost));
    std::vector<float> rgba(pixel_count * 4U, 0.0f);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        rgba[i * 4U + 0] = host_pixels[i].x;
        rgba[i * 4U + 1] = host_pixels[i].y;
        rgba[i * 4U + 2] = host_pixels[i].z;
        rgba[i * 4U + 3] = host_pixels[i].w;
    }
    return rgba;
}

std::vector<float> OptixRenderer::download_depth() const {
    const std::size_t pixel_count = static_cast<std::size_t>(last_width_) * static_cast<std::size_t>(last_height_);
    if (pixel_count == 0 || device_frame_.depth == nullptr) {
        return {};
    }
    std::vector<float> depth(pixel_count, 0.0f);
    RT_CUDA_CHECK(
        cudaMemcpy(depth.data(), device_frame_.depth, pixel_count * sizeof(float), cudaMemcpyDeviceToHost));
    return depth;
}

double OptixRenderer::compute_average_luminance(const std::vector<float>& rgba) const {
    double sum = 0.0;
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        const double r = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 0]))), 0.0, 0.999);
        const double g = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 1]))), 0.0, 0.999);
        const double b = std::clamp(std::sqrt(static_cast<double>(std::max(0.0f, rgba[i + 2]))), 0.0, 0.999);
        sum += (r + g + b) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

RadianceFrame OptixRenderer::render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    upload_scene(scene);
    build_or_refit_accels(scene);
    launch_radiance(rig, profile, camera_index);
    return download_radiance_frame(camera_index);
}

}  // namespace rt
