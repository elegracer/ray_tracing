#include "common/shared_scene_ir.h"
#include "test_support.h"

#include <variant>

int main() {
    rt::SceneIR scene;

    scene.textures.push_back(rt::ConstantColorTextureDesc {Eigen::Vector3d {0.2, 0.4, 0.6}});
    scene.textures.push_back(rt::CheckerTextureDesc {.scale = 0.5, .even_texture_index = 0, .odd_texture_index = 0});
    scene.textures.push_back(rt::ImageTextureDesc {.path = "data/earthmap.jpg"});
    scene.textures.push_back(rt::NoiseTextureDesc {.scale = 3.0});

    scene.materials.push_back(rt::DiffuseMaterialDesc {.albedo_texture_index = 1});
    scene.materials.push_back(rt::MetalMaterialDesc {.albedo_texture_index = 0, .fuzz = 0.1});
    scene.materials.push_back(rt::DielectricMaterialDesc {.ior = 1.5});
    scene.materials.push_back(rt::EmissiveMaterialDesc {.emission_texture_index = 2});
    scene.materials.push_back(rt::IsotropicVolumeMaterialDesc {.albedo_texture_index = 3});

    scene.shapes.push_back(rt::SphereShapeDesc {.center = Eigen::Vector3d {0.0, 1.0, -2.0}, .radius = 1.25});
    scene.shapes.push_back(rt::QuadShapeDesc {
        .origin = Eigen::Vector3d {-1.0, 0.0, -3.0},
        .edge_u = Eigen::Vector3d {2.0, 0.0, 0.0},
        .edge_v = Eigen::Vector3d {0.0, 2.0, 0.0},
    });
    scene.shapes.push_back(rt::BoxShapeDesc {
        .min_corner = Eigen::Vector3d {-0.5, -0.5, -0.5},
        .max_corner = Eigen::Vector3d {0.5, 0.5, 0.5},
    });

    scene.surface_instances.push_back(rt::SurfaceInstanceDesc {
        .shape_index = 2,
        .material_index = 0,
        .transform = rt::TransformDesc {
            .translation = Eigen::Vector3d {0.0, 0.0, -4.0},
            .rotation_xyz_degrees = Eigen::Vector3d {0.0, 35.0, 0.0},
        },
    });
    scene.medium_instances.push_back(rt::MediumInstanceDesc {
        .shape_index = 0,
        .material_index = 4,
        .density = 0.02,
        .transform = rt::TransformDesc {
            .translation = Eigen::Vector3d {0.0, 0.0, 0.0},
            .rotation_xyz_degrees = Eigen::Vector3d {0.0, 0.0, 0.0},
        },
    });

    expect_near(static_cast<double>(scene.textures.size()), 4.0, 1e-12, "texture count");
    expect_near(static_cast<double>(scene.materials.size()), 5.0, 1e-12, "material count");
    expect_near(static_cast<double>(scene.shapes.size()), 3.0, 1e-12, "shape count");
    expect_near(static_cast<double>(scene.surface_instances.size()), 1.0, 1e-12, "surface count");
    expect_near(static_cast<double>(scene.medium_instances.size()), 1.0, 1e-12, "medium count");

    expect_true(std::holds_alternative<rt::CheckerTextureDesc>(scene.textures[1]), "checker texture variant");
    expect_true(std::holds_alternative<rt::IsotropicVolumeMaterialDesc>(scene.materials[4]), "isotropic variant");
    expect_true(std::holds_alternative<rt::BoxShapeDesc>(scene.shapes[2]), "box shape variant");

    const auto& checker = std::get<rt::CheckerTextureDesc>(scene.textures[1]);
    expect_near(static_cast<double>(checker.even_texture_index), 0.0, 1e-12, "checker even idx");
    expect_near(static_cast<double>(checker.odd_texture_index), 0.0, 1e-12, "checker odd idx");

    const auto& surface = scene.surface_instances.front();
    expect_true(surface.shape_index >= 0 && surface.shape_index < static_cast<int>(scene.shapes.size()),
        "surface shape index range");
    expect_true(surface.material_index >= 0 && surface.material_index < static_cast<int>(scene.materials.size()),
        "surface material index range");
    expect_vec3_near(surface.transform.rotation_xyz_degrees, Eigen::Vector3d {0.0, 35.0, 0.0}, 1e-12,
        "surface rotation");

    const auto& medium = scene.medium_instances.front();
    expect_true(medium.shape_index >= 0 && medium.shape_index < static_cast<int>(scene.shapes.size()),
        "medium shape index range");
    expect_true(medium.material_index >= 0 && medium.material_index < static_cast<int>(scene.materials.size()),
        "medium material index range");
    expect_near(medium.density, 0.02, 1e-12, "medium density");

    return 0;
}
