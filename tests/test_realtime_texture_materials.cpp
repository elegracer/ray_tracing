#include "realtime/frame_convention.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;

    const int white = scene.add_texture(rt::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.9, 0.9, 0.9}});
    const int dark = scene.add_texture(rt::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.2, 0.3, 0.2}});
    const int checker = scene.add_texture(rt::CheckerTextureDesc {.scale = 0.5, .even_texture = white, .odd_texture = dark});

    const int checker_mat = scene.add_material(rt::LambertianMaterial {.albedo_texture = checker});
    scene.add_sphere(rt::SpherePrimitive {
        checker_mat,
        rt::legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -3.0}),
        1.0,
        false,
    });

    const rt::PackedScene packed = scene.pack();
    expect_near(static_cast<double>(packed.texture_count), 3.0, 1e-12, "texture count");
    expect_near(static_cast<double>(packed.material_count), 1.0, 1e-12, "material count");
    expect_near(static_cast<double>(packed.sphere_count), 1.0, 1e-12, "sphere count");
    expect_true(
        packed.texture_count == static_cast<int>(packed.textures.size())
            && packed.material_count == static_cast<int>(packed.materials.size())
            && packed.sphere_count == static_cast<int>(packed.spheres.size()),
        "packed scene counts match vector sizes");

    const auto& checker_material = std::get<rt::LambertianMaterial>(packed.materials[0]);
    expect_true(checker_material.albedo_texture == checker, "lambertian uses checker texture");
    return 0;
}
