#include "realtime/gpu/device_scene_buffers.h"

#include <cuda_runtime.h>

#include <stdexcept>
#include <string>

namespace rt {
namespace {

void throw_cuda_error(cudaError_t error, const char* expr) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA runtime failure at ") + expr + ": "
            + cudaGetErrorString(error));
    }
}

#define RT_CUDA_CHECK(expr) throw_cuda_error((expr), #expr)

void free_device_ptr(void* ptr) {
    if (ptr != nullptr) {
        cudaFree(ptr);
    }
}

template <typename T>
void upload_vector(T*& out, const std::vector<T>& values) {
    if (values.empty()) {
        return;
    }
    RT_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&out), values.size() * sizeof(T)));
    RT_CUDA_CHECK(cudaMemcpy(out, values.data(), values.size() * sizeof(T), cudaMemcpyHostToDevice));
}

}  // namespace

DeviceSceneBuffers::~DeviceSceneBuffers() {
    reset();
}

void DeviceSceneBuffers::upload(const GpuPreparedScene& scene) {
    reset();

    upload_vector(spheres_, scene.spheres);
    upload_vector(quads_, scene.quads);
    upload_vector(triangles_, scene.triangles);
    upload_vector(media_, scene.media);
    upload_vector(textures_, scene.textures);
    upload_vector(image_texels_, scene.image_texels);
    upload_vector(materials_, scene.materials);

    sphere_count_ = static_cast<int>(scene.spheres.size());
    quad_count_ = static_cast<int>(scene.quads.size());
    triangle_count_ = static_cast<int>(scene.triangles.size());
    medium_count_ = static_cast<int>(scene.media.size());
    texture_count_ = static_cast<int>(scene.textures.size());
    image_texel_count_ = static_cast<int>(scene.image_texels.size());
    material_count_ = static_cast<int>(scene.materials.size());
}

void DeviceSceneBuffers::reset() {
    free_device_ptr(spheres_);
    free_device_ptr(quads_);
    free_device_ptr(triangles_);
    free_device_ptr(media_);
    free_device_ptr(textures_);
    free_device_ptr(image_texels_);
    free_device_ptr(materials_);
    spheres_ = nullptr;
    quads_ = nullptr;
    triangles_ = nullptr;
    media_ = nullptr;
    textures_ = nullptr;
    image_texels_ = nullptr;
    materials_ = nullptr;
    sphere_count_ = 0;
    quad_count_ = 0;
    triangle_count_ = 0;
    medium_count_ = 0;
    texture_count_ = 0;
    image_texel_count_ = 0;
    material_count_ = 0;
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
        .sphere_count = sphere_count_,
        .quad_count = quad_count_,
        .triangle_count = triangle_count_,
        .medium_count = medium_count_,
        .texture_count = texture_count_,
        .image_texel_count = image_texel_count_,
        .material_count = material_count_,
    };
}

}  // namespace rt
