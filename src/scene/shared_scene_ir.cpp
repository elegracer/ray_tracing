#include "scene/shared_scene_ir.h"

namespace rt::scene {

Transform Transform::identity() {
    return Transform {};
}

int SceneIR::add_texture(const TextureDesc& texture) {
    textures_.push_back(texture);
    return static_cast<int>(textures_.size()) - 1;
}

int SceneIR::add_material(const MaterialDesc& material) {
    materials_.push_back(material);
    return static_cast<int>(materials_.size()) - 1;
}

int SceneIR::add_shape(const ShapeDesc& shape) {
    shapes_.push_back(shape);
    return static_cast<int>(shapes_.size()) - 1;
}

void SceneIR::add_instance(const SurfaceInstance& instance) {
    surface_instances_.push_back(instance);
}

void SceneIR::add_medium(const MediumInstance& medium) {
    media_.push_back(medium);
}

const std::vector<TextureDesc>& SceneIR::textures() const {
    return textures_;
}

const std::vector<MaterialDesc>& SceneIR::materials() const {
    return materials_;
}

const std::vector<ShapeDesc>& SceneIR::shapes() const {
    return shapes_;
}

const std::vector<SurfaceInstance>& SceneIR::surface_instances() const {
    return surface_instances_;
}

const std::vector<MediumInstance>& SceneIR::media() const {
    return media_;
}

}  // namespace rt::scene
