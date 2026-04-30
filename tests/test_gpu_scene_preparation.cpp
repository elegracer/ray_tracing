#include "realtime/frame_convention.h"
#include "realtime/gpu/packed_scene_preparation.h"
#include "realtime/scene_description.h"
#include "test_support.h"

#include <Eigen/Core>

namespace {

void expect_vec3f_near(const Eigen::Vector3f& actual, const Eigen::Vector3f& expected, float tol,
    const std::string& label) {
    if ((actual - expected).cwiseAbs().maxCoeff() > tol) {
        throw std::runtime_error("expect_vec3f_near failed: " + label);
    }
}

}  // namespace

int main() {
    rt::SceneDescription scene;
    scene.background = Eigen::Vector3d {0.1, 0.2, 0.3};

    const int red = scene.add_texture(rt::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.8, 0.1, 0.2}});
    const int blue = scene.add_texture(rt::ConstantColorTextureDesc {.color = Eigen::Vector3d {0.1, 0.2, 0.8}});
    const int checker = scene.add_texture(rt::CheckerTextureDesc {.scale = 3.5, .even_texture = red, .odd_texture = blue});
    const int image = scene.add_texture(rt::ImageTextureDesc {.path = "missing-test-texture.png"});
    const int noise = scene.add_texture(rt::NoiseTextureDesc {.scale = 9.0});

    const int lambertian = scene.add_material(rt::LambertianMaterial {.albedo_texture = checker});
    const int metal = scene.add_material(rt::MetalMaterial {.fuzz = 0.25, .albedo_texture = red});
    const int dielectric = scene.add_material(rt::DielectricMaterial {.ior = 1.33});
    const int light = scene.add_material(rt::DiffuseLightMaterial {.emission_texture = blue});
    const int volume = scene.add_material(rt::IsotropicVolumeMaterial {.albedo_texture = noise});

    scene.add_sphere(rt::SpherePrimitive {
        lambertian,
        rt::legacy_renderer_to_world(Eigen::Vector3d {1.0, 2.0, -3.0}),
        0.75,
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {1.0, 2.0, 3.0},
        Eigen::Vector3d {4.0, 5.0, 6.0},
        Eigen::Vector3d {7.0, 8.0, 9.0},
        false,
    });
    scene.add_triangle(rt::TrianglePrimitive {
        .material_index = metal,
        .p0 = Eigen::Vector3d {1.0, 0.0, 0.0},
        .p1 = Eigen::Vector3d {0.0, 1.0, 0.0},
        .p2 = Eigen::Vector3d {0.0, 0.0, 1.0},
        .dynamic = false,
    });
    scene.add_medium(rt::HomogeneousMediumPrimitive {
        .material_index = volume,
        .density = 0.7,
        .boundary_type = 1,
        .local_center_or_min = Eigen::Vector3d {-1.0, -2.0, -3.0},
        .local_max = Eigen::Vector3d {1.0, 2.0, 3.0},
        .radius = 4.0,
        .world_to_local_rotation = Eigen::Matrix3d::Identity(),
        .translation = Eigen::Vector3d {0.5, 0.25, -0.5},
    });

    const rt::GpuPreparedScene prepared = rt::prepare_gpu_scene(scene.pack());

    expect_vec3f_near(prepared.background, Eigen::Vector3f {0.1f, 0.2f, 0.3f}, 1e-6f, "background");
    expect_true(prepared.spheres.size() == 1, "sphere count");
    expect_true(prepared.quads.size() == 1, "quad count");
    expect_true(prepared.triangles.size() == 1, "triangle count");
    expect_true(prepared.media.size() == 1, "medium count");
    expect_true(prepared.textures.size() == 5, "texture count");
    expect_true(prepared.materials.size() == 5, "material count");
    expect_true(prepared.image_texels.empty(), "missing image texture produces no texels");

    expect_vec3f_near(prepared.spheres[0].center,
        rt::legacy_renderer_to_world(Eigen::Vector3d {1.0, 2.0, -3.0}).cast<float>(), 1e-6f, "sphere center");
    expect_near(prepared.spheres[0].radius, 0.75, 1e-6, "sphere radius");
    expect_true(prepared.spheres[0].material_index == lambertian, "sphere material");

    expect_vec3f_near(prepared.quads[0].origin, Eigen::Vector3f {1.0f, 2.0f, 3.0f}, 1e-6f, "quad origin");
    expect_vec3f_near(prepared.quads[0].edge_u, Eigen::Vector3f {4.0f, 5.0f, 6.0f}, 1e-6f, "quad edge u");
    expect_vec3f_near(prepared.quads[0].edge_v, Eigen::Vector3f {7.0f, 8.0f, 9.0f}, 1e-6f, "quad edge v");
    expect_true(prepared.quads[0].material_index == light, "quad material");

    expect_vec3f_near(prepared.triangles[0].p0, Eigen::Vector3f {1.0f, 0.0f, 0.0f}, 1e-6f, "triangle p0");
    expect_vec3f_near(prepared.triangles[0].p1, Eigen::Vector3f {0.0f, 1.0f, 0.0f}, 1e-6f, "triangle p1");
    expect_vec3f_near(prepared.triangles[0].p2, Eigen::Vector3f {0.0f, 0.0f, 1.0f}, 1e-6f, "triangle p2");
    expect_true(prepared.triangles[0].material_index == metal, "triangle material");

    expect_vec3f_near(prepared.media[0].local_center_or_min, Eigen::Vector3f {-1.0f, -2.0f, -3.0f}, 1e-6f,
        "medium min");
    expect_vec3f_near(prepared.media[0].local_max, Eigen::Vector3f {1.0f, 2.0f, 3.0f}, 1e-6f, "medium max");
    expect_near(prepared.media[0].density, 0.7, 1e-6, "medium density");
    expect_true(prepared.media[0].boundary_type == 1, "medium boundary");
    expect_true(prepared.media[0].material_index == volume, "medium material");

    expect_true(prepared.materials[0].type == 0, "lambertian type");
    expect_true(prepared.materials[0].albedo_texture == checker, "lambertian texture");
    expect_true(prepared.materials[1].type == 1, "metal type");
    expect_true(prepared.materials[1].albedo_texture == red, "metal texture");
    expect_near(prepared.materials[1].fuzz, 0.25, 1e-6, "metal fuzz");
    expect_true(prepared.materials[2].type == 2, "dielectric type");
    expect_near(prepared.materials[2].ior, 1.33, 1e-6, "dielectric ior");
    expect_true(prepared.materials[3].type == 3, "light type");
    expect_true(prepared.materials[3].emission_texture == blue, "light texture");
    expect_true(prepared.materials[4].type == 4, "volume type");
    expect_true(prepared.materials[4].albedo_texture == noise, "volume texture");

    expect_true(prepared.textures[0].type == 0, "constant texture type");
    expect_vec3f_near(prepared.textures[0].color, Eigen::Vector3f {0.8f, 0.1f, 0.2f}, 1e-6f, "constant color");
    expect_true(prepared.textures[2].type == 1, "checker texture type");
    expect_near(prepared.textures[2].scale, 3.5, 1e-6, "checker scale");
    expect_true(prepared.textures[2].even_texture == red, "checker even");
    expect_true(prepared.textures[2].odd_texture == blue, "checker odd");
    expect_true(prepared.textures[3].type == 2, "image texture type");
    expect_true(prepared.textures[3].image_width == 0, "missing image width");
    expect_true(prepared.textures[3].image_height == 0, "missing image height");
    expect_true(prepared.textures[4].type == 3, "noise texture type");
    expect_near(prepared.textures[4].scale, 9.0, 1e-6, "noise scale");

    return 0;
}
