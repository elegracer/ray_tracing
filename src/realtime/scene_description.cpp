#include "realtime/scene_description.h"

namespace rt {

int SceneDescription::add_material(const MaterialDesc& material) {
    materials_.push_back(material);
    return static_cast<int>(materials_.size()) - 1;
}

void SceneDescription::add_sphere(const SpherePrimitive& sphere) {
    spheres_.push_back(sphere);
}

void SceneDescription::add_quad(const QuadPrimitive& quad) {
    quads_.push_back(quad);
}

PackedScene SceneDescription::pack() const {
    return PackedScene {
        .material_count = static_cast<int>(materials_.size()),
        .sphere_count = static_cast<int>(spheres_.size()),
        .quad_count = static_cast<int>(quads_.size()),
        .materials = materials_,
        .spheres = spheres_,
        .quads = quads_,
    };
}

}  // namespace rt
