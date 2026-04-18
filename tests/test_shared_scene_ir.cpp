#include "scene/shared_scene_ir.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <Eigen/Geometry>

#include <numbers>
#include <variant>

int main() {
    const rt::scene::ConstantColorTextureDesc default_constant_texture {};
    expect_vec3_near(default_constant_texture.color, Eigen::Vector3d::Zero(), 1e-12, "constant color default");

    rt::scene::SceneIR scene;

    const int white = scene.add_texture(rt::scene::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.2, 0.4, 0.6}});
    const int checker = scene.add_texture(
        rt::scene::CheckerTextureDesc {.scale = 0.5, .even_texture = white, .odd_texture = white});
    const int image = scene.add_texture(rt::scene::ImageTextureDesc {.path = "data/earthmap.jpg"});
    const int noise = scene.add_texture(rt::scene::NoiseTextureDesc {.scale = 3.0});

    const int diffuse = scene.add_material(rt::scene::DiffuseMaterial {.albedo_texture = checker});
    const int metal = scene.add_material(rt::scene::MetalMaterial {.albedo_texture = white, .fuzz = 0.1});
    scene.add_material(rt::scene::DielectricMaterial {.ior = 1.5});
    scene.add_material(rt::scene::EmissiveMaterial {.emission_texture = image});
    const int isotropic = scene.add_material(rt::scene::IsotropicVolumeMaterial {.albedo_texture = noise});

    const int sphere = scene.add_shape(rt::scene::SphereShape {.center = Eigen::Vector3d {0.0, 1.0, -2.0}, .radius = 1.25});
    scene.add_shape(rt::scene::QuadShape {
        .origin = Eigen::Vector3d {-1.0, 0.0, -3.0},
        .edge_u = Eigen::Vector3d {2.0, 0.0, 0.0},
        .edge_v = Eigen::Vector3d {0.0, 2.0, 0.0},
    });
    const int box = scene.add_shape(rt::scene::BoxShape {
        .min_corner = Eigen::Vector3d {-0.5, -0.5, -0.5},
        .max_corner = Eigen::Vector3d {0.5, 0.5, 0.5},
    });

    rt::scene::Transform transformed = rt::scene::Transform::identity();
    transformed.translation = Eigen::Vector3d {0.0, 0.0, -4.0};
    transformed.rotation =
        Eigen::AngleAxisd(35.0 * std::numbers::pi / 180.0, Eigen::Vector3d::UnitY()).toRotationMatrix();
    scene.add_instance(rt::scene::SurfaceInstance {
        .shape_index = box,
        .material_index = diffuse,
        .transform = transformed,
    });
    scene.add_medium(rt::scene::MediumInstance {
        .shape_index = sphere,
        .material_index = isotropic,
        .density = 0.02,
        .transform = rt::scene::Transform::identity(),
    });

    expect_near(static_cast<double>(scene.textures().size()), 4.0, 1e-12, "texture count");
    expect_near(static_cast<double>(scene.materials().size()), 5.0, 1e-12, "material count");
    expect_near(static_cast<double>(scene.shapes().size()), 3.0, 1e-12, "shape count");
    expect_near(static_cast<double>(scene.surface_instances().size()), 1.0, 1e-12, "surface count");
    expect_near(static_cast<double>(scene.media().size()), 1.0, 1e-12, "medium count");

    expect_true(std::holds_alternative<rt::scene::CheckerTextureDesc>(scene.textures()[1]), "checker texture variant");
    expect_true(std::holds_alternative<rt::scene::IsotropicVolumeMaterial>(scene.materials()[4]), "isotropic variant");
    expect_true(std::holds_alternative<rt::scene::BoxShape>(scene.shapes()[2]), "box shape variant");

    const auto& checker_texture = std::get<rt::scene::CheckerTextureDesc>(scene.textures()[1]);
    expect_near(static_cast<double>(checker_texture.even_texture), 0.0, 1e-12, "checker even idx");
    expect_near(static_cast<double>(checker_texture.odd_texture), 0.0, 1e-12, "checker odd idx");

    const auto& surface = scene.surface_instances().front();
    expect_true(surface.shape_index >= 0 && surface.shape_index < static_cast<int>(scene.shapes().size()),
        "surface shape index range");
    expect_true(surface.material_index >= 0 && surface.material_index < static_cast<int>(scene.materials().size()),
        "surface material index range");
    expect_vec3_near(surface.transform.translation, Eigen::Vector3d {0.0, 0.0, -4.0}, 1e-12, "surface translation");
    expect_near(surface.transform.rotation.determinant(), 1.0, 1e-12, "surface rotation determinant");

    const auto& medium = scene.media().front();
    expect_true(medium.shape_index >= 0 && medium.shape_index < static_cast<int>(scene.shapes().size()),
        "medium shape index range");
    expect_true(medium.material_index >= 0 && medium.material_index < static_cast<int>(scene.materials().size()),
        "medium material index range");
    expect_near(medium.density, 0.02, 1e-12, "medium density");
    expect_vec3_near(medium.transform.translation, Eigen::Vector3d::Zero(), 1e-12, "medium identity translation");

    const auto& identity = rt::scene::Transform::identity();
    expect_vec3_near(identity.translation, Eigen::Vector3d::Zero(), 1e-12, "identity translation");
    expect_near(identity.rotation.trace(), 3.0, 1e-12, "identity rotation trace");

    rt::scene::MaterialDesc shared_material = rt::scene::MetalMaterial {.albedo_texture = white, .fuzz = 0.05};
    rt::MaterialDesc realtime_material = rt::MetalMaterial {.albedo = Eigen::Vector3d {0.7, 0.8, 0.9}, .fuzz = 0.1};
    expect_true(std::holds_alternative<rt::scene::MetalMaterial>(shared_material), "shared material variant");
    expect_true(std::holds_alternative<rt::MetalMaterial>(realtime_material), "realtime material variant");
    expect_near(static_cast<double>(metal), 1.0, 1e-12, "material index assignment");

    return 0;
}
