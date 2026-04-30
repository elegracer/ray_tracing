#include "scene/scene_ir_validator.h"
#include "scene/shared_scene_ir.h"
#include "test_support.h"

#include <stdexcept>
#include <string>

namespace {

template <typename Exception, typename Fn>
void expect_throws_with_message(Fn&& fn, const std::string& message, const std::string& label) {
    try {
        fn();
    } catch (const Exception& ex) {
        expect_true(std::string {ex.what()} == message, label + " message");
        return;
    } catch (...) {
    }
    throw std::runtime_error("expected exception was not thrown: " + label);
}

rt::scene::SceneIR valid_scene() {
    rt::scene::SceneIR scene;
    const int white = scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte = scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int sphere = scene.add_shape(rt::scene::SphereShape {
        .center = Eigen::Vector3d::Zero(),
        .radius = 1.0,
    });
    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = sphere,
        .material_index = matte,
    });
    return scene;
}

}  // namespace

int main() {
    rt::scene::validate_scene_ir(valid_scene());

    rt::scene::SceneIR bad_surface = valid_scene();
    bad_surface.add_instance(rt::scene::SurfaceInstance {
        .shape_index = 99,
        .material_index = 0,
    });
    expect_throws_with_message<std::out_of_range>(
        [&]() { rt::scene::validate_scene_ir(bad_surface); },
        "surface shape index out of range",
        "surface shape index");

    rt::scene::SceneIR bad_triangle = valid_scene();
    const int mesh = bad_triangle.add_shape(rt::scene::TriangleMeshShape {
        .positions = {
            Eigen::Vector3d {0.0, 0.0, 0.0},
            Eigen::Vector3d {1.0, 0.0, 0.0},
            Eigen::Vector3d {0.0, 1.0, 0.0},
        },
        .triangles = {Eigen::Vector3i {0, 1, 3}},
    });
    bad_triangle.add_instance(rt::scene::SurfaceInstance {.shape_index = mesh, .material_index = 0});
    expect_throws_with_message<std::out_of_range>(
        [&]() { rt::scene::validate_scene_ir(bad_triangle); },
        "triangle mesh vertex index out of range",
        "triangle vertex index");

    rt::scene::SceneIR bad_medium = valid_scene();
    bad_medium.add_medium(rt::scene::MediumInstance {
        .shape_index = 0,
        .material_index = 0,
        .density = 0.0,
    });
    expect_throws_with_message<std::invalid_argument>(
        [&]() { rt::scene::validate_scene_ir(bad_medium); },
        "medium density must be positive",
        "medium density");

    rt::scene::SceneIR bad_medium_material = valid_scene();
    bad_medium_material.add_medium(rt::scene::MediumInstance {
        .shape_index = 0,
        .material_index = 0,
        .density = 0.5,
    });
    expect_throws_with_message<std::invalid_argument>(
        [&]() { rt::scene::validate_scene_ir(bad_medium_material); },
        "medium requires isotropic volume material",
        "medium material");

    return 0;
}
