#include "realtime/gpu/device_scene_buffers.h"

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

namespace rt {
namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(
            std::string("CUDA runtime failure at ") + expr + ": " + cudaGetErrorString(error));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)

void free_device_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFree(ptr);
    }
}

template<typename T>
void upload_vector(T*& out, std::size_t& capacity, const std::vector<T>& values) {
    if (values.size() > capacity) {
        free_device_ptr(out);
        out = nullptr;
        capacity = 0;
        RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&out), values.size() * sizeof(T)));
        capacity = values.size();
    }
    if (values.empty()) {
        return;
    }
    RT_CUDA_CHECK(
        cudaMemcpy(out, values.data(), values.size() * sizeof(T), cudaMemcpyHostToDevice));
}

} // namespace

DeviceSceneBuffers::~DeviceSceneBuffers() {
    reset();
}

void DeviceSceneBuffers::upload(const GpuPreparedScene& scene) {
    if (scene.spheres.empty() && scene.quads.empty() && scene.triangles.empty()) {
        reset();
        return;
    }
    GpuSceneAcceleration acceleration;
    const AccelerationUpdateStats update = acceleration.update(scene);
    upload(scene, acceleration, update.kind);
}

void DeviceSceneBuffers::upload(const GpuPreparedScene& scene,
    const GpuSceneAcceleration& acceleration, AccelerationUpdateKind update_kind) {
    const bool upload_geometry = update_kind == AccelerationUpdateKind::rebuild
                                 || update_kind == AccelerationUpdateKind::refit;
    const bool upload_scene_data = update_kind != AccelerationUpdateKind::reuse;
    if (upload_geometry) {
        upload_vector(spheres_, sphere_capacity_, scene.spheres);
        upload_vector(quads_, quad_capacity_, scene.quads);
        upload_vector(triangles_, triangle_capacity_, scene.triangles);
        upload_vector(acceleration_nodes_, acceleration_node_capacity_, acceleration.nodes());
        if (update_kind == AccelerationUpdateKind::rebuild) {
            upload_vector(acceleration_references_, acceleration_reference_capacity_,
                acceleration.references());
        }
    }
    if (upload_scene_data) {
        upload_vector(media_, medium_capacity_, scene.media);
        upload_vector(textures_, texture_capacity_, scene.textures);
        upload_vector(image_texels_, image_texel_capacity_, scene.image_texels);
        upload_vector(materials_, material_capacity_, scene.materials);
        upload_vector(openpbr_materials_, openpbr_material_capacity_, scene.openpbr_materials);
        upload_vector(lights_, light_capacity_, scene.lights);
        upload_vector(analytic_lights_, analytic_light_capacity_, scene.analytic_lights);
    }

    sphere_count_ = static_cast<int>(scene.spheres.size());
    quad_count_ = static_cast<int>(scene.quads.size());
    triangle_count_ = static_cast<int>(scene.triangles.size());
    medium_count_ = static_cast<int>(scene.media.size());
    texture_count_ = static_cast<int>(scene.textures.size());
    image_texel_count_ = static_cast<int>(scene.image_texels.size());
    material_count_ = static_cast<int>(scene.materials.size());
    openpbr_material_count_ = static_cast<int>(scene.openpbr_materials.size());
    light_count_ = static_cast<int>(scene.lights.size());
    analytic_light_count_ = static_cast<int>(scene.analytic_lights.size());
    acceleration_node_count_ = static_cast<int>(acceleration.nodes().size());
    acceleration_reference_count_ = static_cast<int>(acceleration.references().size());
}

void DeviceSceneBuffers::reset() {
    free_device_ptr(spheres_);
    free_device_ptr(quads_);
    free_device_ptr(triangles_);
    free_device_ptr(media_);
    free_device_ptr(textures_);
    free_device_ptr(image_texels_);
    free_device_ptr(materials_);
    free_device_ptr(openpbr_materials_);
    free_device_ptr(lights_);
    free_device_ptr(analytic_lights_);
    free_device_ptr(acceleration_nodes_);
    free_device_ptr(acceleration_references_);
    spheres_ = nullptr;
    quads_ = nullptr;
    triangles_ = nullptr;
    media_ = nullptr;
    textures_ = nullptr;
    image_texels_ = nullptr;
    materials_ = nullptr;
    openpbr_materials_ = nullptr;
    lights_ = nullptr;
    analytic_lights_ = nullptr;
    acceleration_nodes_ = nullptr;
    acceleration_references_ = nullptr;
    sphere_capacity_ = 0;
    quad_capacity_ = 0;
    triangle_capacity_ = 0;
    medium_capacity_ = 0;
    texture_capacity_ = 0;
    image_texel_capacity_ = 0;
    material_capacity_ = 0;
    openpbr_material_capacity_ = 0;
    light_capacity_ = 0;
    analytic_light_capacity_ = 0;
    acceleration_node_capacity_ = 0;
    acceleration_reference_capacity_ = 0;
    sphere_count_ = 0;
    quad_count_ = 0;
    triangle_count_ = 0;
    medium_count_ = 0;
    texture_count_ = 0;
    image_texel_count_ = 0;
    material_count_ = 0;
    openpbr_material_count_ = 0;
    light_count_ = 0;
    analytic_light_count_ = 0;
    acceleration_node_count_ = 0;
    acceleration_reference_count_ = 0;
}

DeviceSceneView DeviceSceneBuffers::view() const {
    return DeviceSceneView {
        .spheres = spheres_,
        .quads = quads_,
        .triangles = triangles_,
        .media = media_,
        .textures = textures_,
        .image_texels = image_texels_,
        .materials = materials_,
        .openpbr_materials = openpbr_materials_,
        .lights = lights_,
        .analytic_lights = analytic_lights_,
        .acceleration_nodes = acceleration_nodes_,
        .acceleration_references = acceleration_references_,
        .sphere_count = sphere_count_,
        .quad_count = quad_count_,
        .triangle_count = triangle_count_,
        .medium_count = medium_count_,
        .texture_count = texture_count_,
        .image_texel_count = image_texel_count_,
        .material_count = material_count_,
        .openpbr_material_count = openpbr_material_count_,
        .light_count = light_count_,
        .analytic_light_count = analytic_light_count_,
        .acceleration_node_count = acceleration_node_count_,
        .acceleration_reference_count = acceleration_reference_count_,
    };
}

} // namespace rt
