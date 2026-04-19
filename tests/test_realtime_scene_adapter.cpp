#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"
#include "test_support.h"

#include <type_traits>

int main() {
    rt::scene::SceneIR triangle_scene;
    const int white = triangle_scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d::Ones()});
    const int matte = triangle_scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = white});
    const int mesh = triangle_scene.add_shape(rt::scene::TriangleMeshShape {
        .positions = {
            Eigen::Vector3d {0.0, 0.0, 0.0},
            Eigen::Vector3d {1.0, 0.0, 0.0},
            Eigen::Vector3d {0.0, 1.0, 0.0},
        },
        .normals = {},
        .uvs = {},
        .triangles = {Eigen::Vector3i {0, 1, 2}},
    });
    triangle_scene.add_instance(rt::scene::SurfaceInstance {.shape_index = mesh, .material_index = matte});

    const rt::SceneDescription adapted_triangle = rt::scene::adapt_to_realtime(triangle_scene);
    expect_true(adapted_triangle.triangles().size() == 1, "realtime adapter emits triangle primitive");

    const rt::scene::SceneIR cornell_smoke = rt::scene::build_scene("cornell_smoke");
    const rt::SceneDescription adapted = rt::scene::adapt_to_realtime(cornell_smoke);
    const rt::PackedScene packed = adapted.pack();

    expect_true(!packed.materials.empty(), "realtime adapter preserves materials");
    expect_true(!packed.quads.empty(), "realtime adapter lowers room geometry");
    expect_true(packed.media.size() == 2, "cornell smoke keeps two media");
    expect_true(packed.medium_count == 2, "packed medium count");
    expect_true(std::holds_alternative<rt::IsotropicVolumeMaterial>(packed.materials.back()),
        "isotropic material preserved for media");

    const rt::HomogeneousMediumPrimitive& first_medium = packed.media.front();
    expect_true(first_medium.boundary_type == 1, "box media lower to packed box boundaries");
    expect_true(first_medium.material_index >= 0, "medium material index");
    expect_true(first_medium.density > 0.0f, "medium density kept positive");
    return 0;
}
