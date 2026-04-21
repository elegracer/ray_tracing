#include "scene/shared_scene_builders.h"

#include "realtime/camera_models.h"
#include "scene/scene_definition.h"
#include "scene/scene_file_catalog.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace rt::scene {
namespace {

using BuilderFn = SceneIR (*)();

struct SceneRegistryEntry {
    SceneMetadata metadata;
    BuilderFn builder = nullptr;
};

struct CpuPresetRegistryEntry {
    CpuRenderPreset preset;
    bool is_default = false;
};

struct RealtimePresetRegistryEntry {
    std::string_view scene_id;
    RealtimeViewPreset preset;
};

constexpr int kBuiltinRealtimeWidth = 640;
constexpr int kBuiltinRealtimeHeight = 480;

Eigen::Vector3d legacy_renderer_to_world(const Eigen::Vector3d& v) {
    return Eigen::Vector3d {v.x(), -v.z(), v.y()};
}

int add_constant_texture(SceneIR& scene, const Eigen::Vector3d& color) {
    return scene.add_texture(ConstantColorTextureDesc {.color = color});
}

int add_diffuse_color(SceneIR& scene, const Eigen::Vector3d& color) {
    const int texture = add_constant_texture(scene, color);
    return scene.add_material(DiffuseMaterial {.albedo_texture = texture});
}

int add_emissive_color(SceneIR& scene, const Eigen::Vector3d& color) {
    const int texture = add_constant_texture(scene, color);
    return scene.add_material(EmissiveMaterial {.emission_texture = texture});
}

void add_sphere_instance(SceneIR& scene, int material_index, const Eigen::Vector3d& center, double radius) {
    const int shape = scene.add_shape(SphereShape {.center = center, .radius = radius});
    scene.add_instance(SurfaceInstance {
        .shape_index = shape,
        .material_index = material_index,
    });
}

void add_quad_instance(SceneIR& scene,
    int material_index,
    const Eigen::Vector3d& origin,
    const Eigen::Vector3d& edge_u,
    const Eigen::Vector3d& edge_v) {
    const int shape = scene.add_shape(QuadShape {.origin = origin, .edge_u = edge_u, .edge_v = edge_v});
    scene.add_instance(SurfaceInstance {
        .shape_index = shape,
        .material_index = material_index,
    });
}

int add_box_shape(SceneIR& scene, const Eigen::Vector3d& min_corner, const Eigen::Vector3d& max_corner) {
    return scene.add_shape(BoxShape {
        .min_corner = min_corner,
        .max_corner = max_corner,
    });
}

void add_box_instance(SceneIR& scene, int material_index, int shape_index, const Eigen::Vector3d& translation, double yaw_deg) {
    Transform transform = Transform::identity();
    transform.translation = translation;
    transform.rotation =
        Eigen::AngleAxisd(yaw_deg * std::numbers::pi / 180.0, Eigen::Vector3d::UnitY()).toRotationMatrix();
    scene.add_instance(SurfaceInstance {
        .shape_index = shape_index,
        .material_index = material_index,
        .transform = transform,
    });
}

struct CornellMaterials {
    int red = -1;
    int white = -1;
    int green = -1;
    int light = -1;
};

CornellMaterials add_cornell_room(SceneIR& scene) {
    CornellMaterials materials;
    materials.red = add_diffuse_color(scene, Eigen::Vector3d {0.65, 0.05, 0.05});
    materials.white = add_diffuse_color(scene, Eigen::Vector3d {0.73, 0.73, 0.73});
    materials.green = add_diffuse_color(scene, Eigen::Vector3d {0.12, 0.45, 0.15});
    materials.light = add_emissive_color(scene, Eigen::Vector3d {15.0, 15.0, 15.0});

    add_quad_instance(scene,
        materials.green,
        Eigen::Vector3d {555.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 555.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 555.0});
    add_quad_instance(scene,
        materials.red,
        Eigen::Vector3d {0.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 555.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 555.0});
    add_quad_instance(scene,
        materials.light,
        Eigen::Vector3d {343.0, 554.0, 332.0},
        Eigen::Vector3d {-130.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, -105.0});
    add_quad_instance(scene,
        materials.white,
        Eigen::Vector3d {0.0, 0.0, 0.0},
        Eigen::Vector3d {555.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 555.0});
    add_quad_instance(scene,
        materials.white,
        Eigen::Vector3d {555.0, 555.0, 555.0},
        Eigen::Vector3d {-555.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, -555.0});
    add_quad_instance(scene,
        materials.white,
        Eigen::Vector3d {0.0, 0.0, 555.0},
        Eigen::Vector3d {555.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 555.0, 0.0});
    return materials;
}

CpuCameraPreset make_cpu_camera(double vfov, const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat,
    double defocus_angle = 0.0) {
    (void)vfov;
    CpuCameraPreset preset;
    const int image_height = static_cast<int>(std::lround(static_cast<double>(preset.image_width) / preset.aspect_ratio));
    const DefaultCameraIntrinsics intrinsics = derive_default_camera_intrinsics(
        CameraModelType::equi62_lut1d, preset.image_width, image_height, default_hfov_deg(CameraModelType::equi62_lut1d));
    preset.camera = CameraSpec {
        .model = CameraModelType::equi62_lut1d,
        .width = preset.image_width,
        .height = image_height,
        .fx = intrinsics.fx,
        .fy = intrinsics.fy,
        .cx = intrinsics.cx,
        .cy = intrinsics.cy,
    };
    preset.lookfrom = lookfrom;
    preset.lookat = lookat;
    preset.defocus_angle = defocus_angle;
    return preset;
}

CpuCameraPreset make_explicit_pinhole_cpu_camera(double hfov_deg, const Eigen::Vector3d& lookfrom,
    const Eigen::Vector3d& lookat, double defocus_angle = 0.0) {
    CpuCameraPreset preset;
    const int image_height = static_cast<int>(std::lround(static_cast<double>(preset.image_width) / preset.aspect_ratio));
    const DefaultCameraIntrinsics intrinsics =
        derive_default_camera_intrinsics(CameraModelType::pinhole32, preset.image_width, image_height, hfov_deg);
    preset.camera = CameraSpec {
        .model = CameraModelType::pinhole32,
        .width = preset.image_width,
        .height = image_height,
        .fx = intrinsics.fx,
        .fy = intrinsics.fy,
        .cx = intrinsics.cx,
        .cy = intrinsics.cy,
    };
    preset.lookfrom = lookfrom;
    preset.lookat = lookat;
    preset.defocus_angle = defocus_angle;
    return preset;
}

Eigen::Vector3d scene_world_up(viewer::ViewerFrameConvention convention) {
    if (convention == viewer::ViewerFrameConvention::legacy_y_up) {
        return Eigen::Vector3d::UnitY();
    }
    return Eigen::Vector3d::UnitZ();
}

Eigen::Vector3d scene_neutral_forward(viewer::ViewerFrameConvention convention) {
    if (convention == viewer::ViewerFrameConvention::legacy_y_up) {
        return Eigen::Vector3d(0.0, 0.0, -1.0);
    }
    return Eigen::Vector3d(0.0, 1.0, 0.0);
}

Eigen::Vector3d scene_neutral_right() {
    return Eigen::Vector3d::UnitX();
}

viewer::BodyPose pose_from_look_at(const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat,
    viewer::ViewerFrameConvention convention) {
    const Eigen::Vector3d direction = (lookat - lookfrom).normalized();
    const Eigen::Vector3d up = scene_world_up(convention);
    const double pitch_rad = std::asin(std::clamp(direction.dot(up), -1.0, 1.0));
    const Eigen::Vector3d horizontal = direction - direction.dot(up) * up;
    const Eigen::Vector3d projected =
        horizontal.squaredNorm() > 1e-12 ? horizontal.normalized() : scene_neutral_forward(convention);
    const double yaw_rad = std::atan2(
        projected.dot(-scene_neutral_right()),
        projected.dot(scene_neutral_forward(convention)));
    return viewer::BodyPose {
        .position = lookfrom,
        .yaw_deg = yaw_rad * 180.0 / std::numbers::pi,
        .pitch_deg = pitch_rad * 180.0 / std::numbers::pi,
    };
}

RealtimeViewPreset make_realtime_view_preset(const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat,
    viewer::ViewerFrameConvention convention, double vfov_deg, double base_move_speed) {
    (void)vfov_deg;
    const DefaultCameraIntrinsics intrinsics = derive_default_camera_intrinsics(CameraModelType::equi62_lut1d,
        kBuiltinRealtimeWidth, kBuiltinRealtimeHeight, default_hfov_deg(CameraModelType::equi62_lut1d));
    return RealtimeViewPreset {
        .initial_body_pose = pose_from_look_at(lookfrom, lookat, convention),
        .frame_convention = convention,
        .camera = CameraSpec {
            .model = CameraModelType::equi62_lut1d,
            .width = kBuiltinRealtimeWidth,
            .height = kBuiltinRealtimeHeight,
            .fx = intrinsics.fx,
            .fy = intrinsics.fy,
            .cx = intrinsics.cx,
            .cy = intrinsics.cy,
        },
        .base_move_speed = base_move_speed,
    };
}

SceneIR make_bouncing_spheres_scene() {
    SceneIR scene;
    const int ground = add_diffuse_color(scene, Eigen::Vector3d {0.5, 0.5, 0.5});
    const int diffuse = add_diffuse_color(scene, Eigen::Vector3d {0.4, 0.2, 0.1});
    const int metal_texture = add_constant_texture(scene, Eigen::Vector3d {0.7, 0.6, 0.5});
    const int metal = scene.add_material(MetalMaterial {.albedo_texture = metal_texture, .fuzz = 0.0});
    const int glass = scene.add_material(DielectricMaterial {.ior = 1.5});
    const int blue_texture = add_constant_texture(scene, Eigen::Vector3d {0.2, 0.3, 0.8});
    const int blue_metal = scene.add_material(MetalMaterial {.albedo_texture = blue_texture, .fuzz = 0.25});

    add_sphere_instance(scene, ground, Eigen::Vector3d {0.0, -1000.0, 0.0}, 1000.0);
    add_sphere_instance(scene, glass, Eigen::Vector3d {0.0, 1.0, 0.0}, 1.0);
    add_sphere_instance(scene, diffuse, Eigen::Vector3d {-4.0, 1.0, 0.0}, 1.0);
    add_sphere_instance(scene, metal, Eigen::Vector3d {4.0, 1.0, 0.0}, 1.0);

    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            const Eigen::Vector3d center {static_cast<double>(x) * 1.2, 0.2, static_cast<double>(z) * 1.2};
            const int material = ((x + z) % 3 == 0) ? glass : (((x + z) % 2 == 0) ? diffuse : blue_metal);
            add_sphere_instance(scene, material, center, 0.2);
        }
    }
    return scene;
}

SceneIR make_checkered_spheres_scene() {
    SceneIR scene;
    const int even = add_constant_texture(scene, Eigen::Vector3d {0.2, 0.3, 0.1});
    const int odd = add_constant_texture(scene, Eigen::Vector3d {0.9, 0.9, 0.9});
    const int checker = scene.add_texture(CheckerTextureDesc {.scale = 0.32, .even_texture = even, .odd_texture = odd});
    const int checker_material = scene.add_material(DiffuseMaterial {.albedo_texture = checker});

    add_sphere_instance(scene, checker_material, Eigen::Vector3d {0.0, -10.0, 0.0}, 10.0);
    add_sphere_instance(scene, checker_material, Eigen::Vector3d {0.0, 10.0, 0.0}, 10.0);
    return scene;
}

SceneIR make_earth_sphere_scene() {
    SceneIR scene;
    const int earth_texture = scene.add_texture(ImageTextureDesc {.path = "earthmap.jpg"});
    const int earth_material = scene.add_material(DiffuseMaterial {.albedo_texture = earth_texture});
    add_sphere_instance(scene, earth_material, Eigen::Vector3d {0.0, 0.0, 0.0}, 2.0);
    return scene;
}

SceneIR make_perlin_spheres_scene() {
    SceneIR scene;
    const int noise = scene.add_texture(NoiseTextureDesc {.scale = 4.0});
    const int perlin = scene.add_material(DiffuseMaterial {.albedo_texture = noise});
    add_sphere_instance(scene, perlin, Eigen::Vector3d {0.0, -1000.0, 0.0}, 1000.0);
    add_sphere_instance(scene, perlin, Eigen::Vector3d {0.0, 2.0, 0.0}, 2.0);
    return scene;
}

SceneIR make_quads_scene() {
    SceneIR scene;
    const int left = add_diffuse_color(scene, Eigen::Vector3d {1.0, 0.2, 0.2});
    const int back = add_diffuse_color(scene, Eigen::Vector3d {0.2, 1.0, 0.2});
    const int right = add_diffuse_color(scene, Eigen::Vector3d {0.2, 0.2, 1.0});
    const int top = add_diffuse_color(scene, Eigen::Vector3d {1.0, 0.5, 0.0});
    const int bottom = add_diffuse_color(scene, Eigen::Vector3d {0.2, 0.8, 0.8});

    add_quad_instance(scene,
        left,
        Eigen::Vector3d {-3.0, -2.0, 5.0},
        Eigen::Vector3d {0.0, 0.0, -4.0},
        Eigen::Vector3d {0.0, 4.0, 0.0});
    add_quad_instance(scene,
        back,
        Eigen::Vector3d {-2.0, -2.0, 0.0},
        Eigen::Vector3d {4.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.0, 0.0});
    add_quad_instance(scene,
        right,
        Eigen::Vector3d {3.0, -2.0, 1.0},
        Eigen::Vector3d {0.0, 0.0, 4.0},
        Eigen::Vector3d {0.0, 4.0, 0.0});
    add_quad_instance(scene,
        top,
        Eigen::Vector3d {-2.0, 3.0, 1.0},
        Eigen::Vector3d {4.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 4.0});
    add_quad_instance(scene,
        bottom,
        Eigen::Vector3d {-2.0, -3.0, 5.0},
        Eigen::Vector3d {4.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, -4.0});
    return scene;
}

SceneIR make_simple_light_scene() {
    SceneIR scene;
    const int noise = scene.add_texture(NoiseTextureDesc {.scale = 4.0});
    const int perlin = scene.add_material(DiffuseMaterial {.albedo_texture = noise});
    const int light = add_emissive_color(scene, Eigen::Vector3d {4.0, 4.0, 4.0});

    add_sphere_instance(scene, perlin, Eigen::Vector3d {0.0, -1000.0, 0.0}, 1000.0);
    add_sphere_instance(scene, perlin, Eigen::Vector3d {0.0, 2.0, 0.0}, 2.0);
    add_sphere_instance(scene, light, Eigen::Vector3d {0.0, 7.0, 0.0}, 2.0);
    add_quad_instance(scene,
        light,
        Eigen::Vector3d {3.0, 1.0, -2.0},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 2.0, 0.0});
    return scene;
}

SceneIR make_cornell_box_scene() {
    SceneIR scene;
    const CornellMaterials materials = add_cornell_room(scene);
    const int tall_box = add_box_shape(scene, Eigen::Vector3d {0.0, 0.0, 0.0}, Eigen::Vector3d {165.0, 330.0, 165.0});
    const int short_box = add_box_shape(scene, Eigen::Vector3d {0.0, 0.0, 0.0}, Eigen::Vector3d {165.0, 165.0, 165.0});
    add_box_instance(scene, materials.white, tall_box, Eigen::Vector3d {265.0, 0.0, 295.0}, 15.0);
    add_box_instance(scene, materials.white, short_box, Eigen::Vector3d {130.0, 0.0, 65.0}, -18.0);
    return scene;
}

SceneIR make_cornell_box_and_sphere_scene() {
    SceneIR scene;
    const CornellMaterials materials = add_cornell_room(scene);
    const int tall_box = add_box_shape(scene, Eigen::Vector3d {0.0, 0.0, 0.0}, Eigen::Vector3d {165.0, 330.0, 165.0});
    add_box_instance(scene, materials.white, tall_box, Eigen::Vector3d {265.0, 0.0, 295.0}, 15.0);

    const int glass = scene.add_material(DielectricMaterial {.ior = 1.5});
    add_sphere_instance(scene, glass, Eigen::Vector3d {190.0, 90.0, 190.0}, 90.0);
    return scene;
}

SceneIR make_cornell_smoke_scene() {
    SceneIR scene;
    const CornellMaterials materials = add_cornell_room(scene);

    const int box1 = add_box_shape(scene, Eigen::Vector3d {0.0, 0.0, 0.0}, Eigen::Vector3d {165.0, 330.0, 165.0});
    const int box2 = add_box_shape(scene, Eigen::Vector3d {0.0, 0.0, 0.0}, Eigen::Vector3d {165.0, 165.0, 165.0});

    add_box_instance(scene, materials.white, box1, Eigen::Vector3d {265.0, 0.0, 295.0}, 15.0);
    add_box_instance(scene, materials.white, box2, Eigen::Vector3d {130.0, 0.0, 65.0}, -18.0);

    const int black_texture = add_constant_texture(scene, Eigen::Vector3d {0.0, 0.0, 0.0});
    const int white_texture = add_constant_texture(scene, Eigen::Vector3d {1.0, 1.0, 1.0});
    const int black_smoke = scene.add_material(IsotropicVolumeMaterial {.albedo_texture = black_texture});
    const int white_smoke = scene.add_material(IsotropicVolumeMaterial {.albedo_texture = white_texture});

    Transform box1_transform = Transform::identity();
    box1_transform.translation = Eigen::Vector3d {265.0, 0.0, 295.0};
    box1_transform.rotation =
        Eigen::AngleAxisd(15.0 * std::numbers::pi / 180.0, Eigen::Vector3d::UnitY()).toRotationMatrix();
    Transform box2_transform = Transform::identity();
    box2_transform.translation = Eigen::Vector3d {130.0, 0.0, 65.0};
    box2_transform.rotation =
        Eigen::AngleAxisd(-18.0 * std::numbers::pi / 180.0, Eigen::Vector3d::UnitY()).toRotationMatrix();

    scene.add_medium(MediumInstance {
        .shape_index = box1,
        .material_index = black_smoke,
        .density = 0.01,
        .transform = box1_transform,
    });
    scene.add_medium(MediumInstance {
        .shape_index = box2,
        .material_index = white_smoke,
        .density = 0.01,
        .transform = box2_transform,
    });
    return scene;
}

SceneIR make_rttnw_final_scene() {
    SceneIR scene;
    const int ground = add_diffuse_color(scene, Eigen::Vector3d {0.48, 0.83, 0.53});
    const int light = add_emissive_color(scene, Eigen::Vector3d {7.0, 7.0, 7.0});

    const int floor_box = add_box_shape(scene, Eigen::Vector3d {-1000.0, -1.0, -1000.0}, Eigen::Vector3d {1000.0, 1.0, 1000.0});
    scene.add_instance(SurfaceInstance {
        .shape_index = floor_box,
        .material_index = ground,
    });
    add_quad_instance(scene,
        light,
        Eigen::Vector3d {123.0, 554.0, 147.0},
        Eigen::Vector3d {300.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 265.0});

    const int warm = add_diffuse_color(scene, Eigen::Vector3d {0.7, 0.3, 0.1});
    const int glass = scene.add_material(DielectricMaterial {.ior = 1.5});
    const int metal_texture = add_constant_texture(scene, Eigen::Vector3d {0.8, 0.8, 0.9});
    const int metal = scene.add_material(MetalMaterial {.albedo_texture = metal_texture, .fuzz = 1.0});

    add_sphere_instance(scene, warm, Eigen::Vector3d {400.0, 400.0, 200.0}, 50.0);
    add_sphere_instance(scene, glass, Eigen::Vector3d {260.0, 150.0, 45.0}, 50.0);
    add_sphere_instance(scene, metal, Eigen::Vector3d {0.0, 150.0, 145.0}, 50.0);

    const int boundary = scene.add_shape(SphereShape {.center = Eigen::Vector3d {360.0, 150.0, 145.0}, .radius = 70.0});
    scene.add_instance(SurfaceInstance {
        .shape_index = boundary,
        .material_index = glass,
    });
    const int blue_scatter_texture = add_constant_texture(scene, Eigen::Vector3d {0.2, 0.4, 0.9});
    const int white_scatter_texture = add_constant_texture(scene, Eigen::Vector3d {1.0, 1.0, 1.0});
    const int blue_scatter = scene.add_material(IsotropicVolumeMaterial {.albedo_texture = blue_scatter_texture});
    const int white_scatter = scene.add_material(IsotropicVolumeMaterial {.albedo_texture = white_scatter_texture});
    scene.add_medium(MediumInstance {
        .shape_index = boundary,
        .material_index = blue_scatter,
        .density = 0.2,
        .transform = Transform::identity(),
    });

    const int atmosphere = scene.add_shape(SphereShape {.center = Eigen::Vector3d {0.0, 0.0, 0.0}, .radius = 5000.0});
    scene.add_medium(MediumInstance {
        .shape_index = atmosphere,
        .material_index = white_scatter,
        .density = 0.0001,
        .transform = Transform::identity(),
    });

    const int earth_texture = scene.add_texture(ImageTextureDesc {.path = "earthmap.jpg"});
    const int earth = scene.add_material(DiffuseMaterial {.albedo_texture = earth_texture});
    add_sphere_instance(scene, earth, Eigen::Vector3d {400.0, 200.0, 400.0}, 100.0);

    const int noise_texture = scene.add_texture(NoiseTextureDesc {.scale = 0.2});
    const int noise = scene.add_material(DiffuseMaterial {.albedo_texture = noise_texture});
    add_sphere_instance(scene, noise, Eigen::Vector3d {220.0, 280.0, 300.0}, 80.0);

    const int white = add_diffuse_color(scene, Eigen::Vector3d {0.73, 0.73, 0.73});
    for (int i = 0; i < 64; ++i) {
        const int row = i / 8;
        const int col = i % 8;
        const Eigen::Vector3d center {-100.0 + col * 24.0, 270.0 + (row % 3) * 20.0, 320.0 + row * 22.0};
        add_sphere_instance(scene, white, center, 10.0);
    }
    return scene;
}

SceneIR make_realtime_smoke_scene() {
    SceneIR scene;
    const int diffuse = add_diffuse_color(scene, Eigen::Vector3d {0.75, 0.25, 0.2});
    const int light = add_emissive_color(scene, Eigen::Vector3d {10.0, 10.0, 10.0});

    add_sphere_instance(scene, diffuse, legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5);
    add_quad_instance(scene,
        light,
        legacy_renderer_to_world(Eigen::Vector3d {-0.75, 1.25, -1.5}),
        legacy_renderer_to_world(Eigen::Vector3d {1.5, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.5}));
    return scene;
}

SceneIR make_final_room_scene() {
    SceneIR scene;
    const int white = add_diffuse_color(scene, Eigen::Vector3d {0.73, 0.73, 0.73});
    const int green = add_diffuse_color(scene, Eigen::Vector3d {0.30, 0.70, 0.35});
    const int red = add_diffuse_color(scene, Eigen::Vector3d {0.72, 0.25, 0.22});
    const int blue = add_diffuse_color(scene, Eigen::Vector3d {0.25, 0.35, 0.75});
    const int light = add_emissive_color(scene, Eigen::Vector3d {10.0, 10.0, 10.0});

    add_quad_instance(scene,
        white,
        legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}));
    add_quad_instance(scene,
        white,
        legacy_renderer_to_world(Eigen::Vector3d {-4.0, 3.5, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}));
    add_quad_instance(scene,
        green,
        legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}));
    add_quad_instance(scene,
        red,
        legacy_renderer_to_world(Eigen::Vector3d {4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 8.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}));
    add_quad_instance(scene,
        white,
        legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, -4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}));
    add_quad_instance(scene,
        blue,
        legacy_renderer_to_world(Eigen::Vector3d {-4.0, -1.0, 4.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 4.5, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {8.0, 0.0, 0.0}));
    add_quad_instance(scene,
        light,
        legacy_renderer_to_world(Eigen::Vector3d {-1.0, 3.15, -1.0}),
        legacy_renderer_to_world(Eigen::Vector3d {2.0, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 2.0}));
    add_quad_instance(scene,
        white,
        legacy_renderer_to_world(Eigen::Vector3d {-3.2, -0.25, -3.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.8}),
        legacy_renderer_to_world(Eigen::Vector3d {1.8, 0.0, 0.0}));
    add_quad_instance(scene,
        white,
        legacy_renderer_to_world(Eigen::Vector3d {1.2, 0.15, 1.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.6}),
        legacy_renderer_to_world(Eigen::Vector3d {1.6, 0.0, 0.0}));

    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.25, -1.2}), 0.55);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {-1.6, 0.35, 1.7}), 0.55);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {-3.1, 1.0, 0.8}), 0.55);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {3.0, 1.35, -0.9}), 0.65);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {1.1, 1.1, -3.0}), 0.60);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {-0.8, 2.55, 2.2}), 0.45);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {0.9, 0.55, -0.1}), 0.45);
    add_sphere_instance(scene, white, legacy_renderer_to_world(Eigen::Vector3d {-0.35, 0.4, -1.15}), 0.35);
    return scene;
}

const std::vector<SceneRegistryEntry> kSceneRegistry {
    {SceneMetadata {"bouncing_spheres", "Bouncing Spheres", Eigen::Vector3d {0.70, 0.80, 1.00}, true, true},
        &make_bouncing_spheres_scene},
    {SceneMetadata {"checkered_spheres", "Checkered Spheres", Eigen::Vector3d {0.70, 0.80, 1.00}, true, true},
        &make_checkered_spheres_scene},
    {SceneMetadata {"earth_sphere", "Earth Sphere", Eigen::Vector3d {0.70, 0.80, 1.00}, true, true},
        &make_earth_sphere_scene},
    {SceneMetadata {"perlin_spheres", "Perlin Spheres", Eigen::Vector3d {0.70, 0.80, 1.00}, true, true},
        &make_perlin_spheres_scene},
    {SceneMetadata {"quads", "Quads", Eigen::Vector3d {0.70, 0.80, 1.00}, true, true}, &make_quads_scene},
    {SceneMetadata {"simple_light", "Simple Light", Eigen::Vector3d::Zero(), true, true}, &make_simple_light_scene},
    {SceneMetadata {"cornell_smoke", "Cornell Smoke", Eigen::Vector3d::Zero(), true, true}, &make_cornell_smoke_scene},
    {SceneMetadata {"cornell_box", "Cornell Box", Eigen::Vector3d::Zero(), true, true}, &make_cornell_box_scene},
    {SceneMetadata {"cornell_box_and_sphere", "Cornell Box And Sphere", Eigen::Vector3d::Zero(), true, true},
        &make_cornell_box_and_sphere_scene},
    {SceneMetadata {"rttnw_final_scene", "RTTNW Final Scene", Eigen::Vector3d::Zero(), true, true},
        &make_rttnw_final_scene},
    {SceneMetadata {"smoke", "Realtime Smoke", Eigen::Vector3d::Zero(), false, true}, &make_realtime_smoke_scene},
    {SceneMetadata {"final_room", "Final Room", Eigen::Vector3d::Zero(), true, true}, &make_final_room_scene},
};

const std::vector<CpuPresetRegistryEntry> kCpuPresetRegistry {
    {CpuRenderPreset {"bouncing_spheres", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero(), 0.6)},
        true},
    {CpuRenderPreset {"checkered_spheres", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero())},
        true},
    {CpuRenderPreset {"earth_sphere", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {-3.0, 6.0, -10.0}, Eigen::Vector3d::Zero())},
        true},
    {CpuRenderPreset {"perlin_spheres", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero())},
        true},
    {CpuRenderPreset {"quads", "default", 500,
         make_cpu_camera(80.0, Eigen::Vector3d {0.0, 0.0, 9.0}, Eigen::Vector3d::Zero())},
        true},
    {CpuRenderPreset {"simple_light", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {26.0, 3.0, 6.0}, Eigen::Vector3d {0.0, 2.0, 0.0})},
        true},
    {CpuRenderPreset {"cornell_smoke", "default", 500,
         make_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        true},
    {CpuRenderPreset {"cornell_smoke", "extreme", 10000,
         make_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        false},
    {CpuRenderPreset {"cornell_box", "default", 1000,
         make_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        true},
    {CpuRenderPreset {"cornell_box", "extreme", 10000,
         make_explicit_pinhole_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        false},
    {CpuRenderPreset {"cornell_box_and_sphere", "default", 1000,
         make_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        true},
    {CpuRenderPreset {"cornell_box_and_sphere", "extreme", 10000,
         make_cpu_camera(40.0, Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        false},
    {CpuRenderPreset {"rttnw_final_scene", "default", 500,
         make_cpu_camera(40.0, Eigen::Vector3d {478.0, 278.0, -600.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        true},
    {CpuRenderPreset {"rttnw_final_scene", "extreme", 10000,
         make_cpu_camera(40.0, Eigen::Vector3d {478.0, 278.0, -600.0}, Eigen::Vector3d {278.0, 278.0, 0.0})},
        false},
    {CpuRenderPreset {"final_room", "default", 500,
         make_cpu_camera(20.0, Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero())},
        true},
};

const std::vector<RealtimePresetRegistryEntry> kRealtimePresetRegistry {
    {"bouncing_spheres",
        make_realtime_view_preset(Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero(),
            viewer::ViewerFrameConvention::legacy_y_up, 20.0, 2.0)},
    {"checkered_spheres",
        make_realtime_view_preset(Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero(),
            viewer::ViewerFrameConvention::legacy_y_up, 20.0, 2.0)},
    {"earth_sphere",
        make_realtime_view_preset(Eigen::Vector3d {-3.0, 6.0, -10.0}, Eigen::Vector3d::Zero(),
            viewer::ViewerFrameConvention::legacy_y_up, 20.0, 2.5)},
    {"perlin_spheres",
        make_realtime_view_preset(Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero(),
            viewer::ViewerFrameConvention::legacy_y_up, 20.0, 2.0)},
    {"quads",
        make_realtime_view_preset(Eigen::Vector3d {0.0, 0.0, 9.0}, Eigen::Vector3d::Zero(),
            viewer::ViewerFrameConvention::legacy_y_up, 80.0, 2.5)},
    {"simple_light",
        make_realtime_view_preset(Eigen::Vector3d {10.0, 3.0, 6.0}, Eigen::Vector3d {0.0, 2.0, 0.0},
            viewer::ViewerFrameConvention::legacy_y_up, 20.0, 4.0)},
    {"cornell_smoke",
        make_realtime_view_preset(Eigen::Vector3d {278.0, 278.0, -120.0}, Eigen::Vector3d {278.0, 278.0, 120.0},
            viewer::ViewerFrameConvention::legacy_y_up, 40.0, 120.0)},
    {"cornell_box",
        make_realtime_view_preset(Eigen::Vector3d {278.0, 278.0, -120.0}, Eigen::Vector3d {278.0, 278.0, 120.0},
            viewer::ViewerFrameConvention::legacy_y_up, 40.0, 120.0)},
    {"cornell_box_and_sphere",
        make_realtime_view_preset(Eigen::Vector3d {278.0, 278.0, -120.0}, Eigen::Vector3d {278.0, 278.0, 120.0},
            viewer::ViewerFrameConvention::legacy_y_up, 40.0, 120.0)},
    {"rttnw_final_scene",
        make_realtime_view_preset(Eigen::Vector3d {278.0, 278.0, -180.0}, Eigen::Vector3d {278.0, 278.0, 50.0},
            viewer::ViewerFrameConvention::legacy_y_up, 40.0, 180.0)},
    {"smoke",
        RealtimeViewPreset {
            .initial_body_pose = viewer::BodyPose {
                .position = Eigen::Vector3d(0.0, -2.5, 0.25),
                .yaw_deg = 0.0,
                .pitch_deg = 0.0,
            },
            .frame_convention = viewer::ViewerFrameConvention::world_z_up,
            .camera = CameraSpec {
                .model = CameraModelType::pinhole32,
                .width = kBuiltinRealtimeWidth,
                .height = kBuiltinRealtimeHeight,
                .fx = 360.0,
                .fy = 360.0,
                .cx = 320.0,
                .cy = 240.0,
            },
            .base_move_speed = 2.0,
        }},
    {"final_room",
        RealtimeViewPreset {
            .initial_body_pose = viewer::default_spawn_pose(),
            .frame_convention = viewer::ViewerFrameConvention::world_z_up,
            .camera = CameraSpec {
                .model = CameraModelType::pinhole32,
                .width = kBuiltinRealtimeWidth,
                .height = kBuiltinRealtimeHeight,
                .fx = 0.75 * static_cast<double>(kBuiltinRealtimeWidth),
                .fy = 0.75 * static_cast<double>(kBuiltinRealtimeHeight),
                .cx = 0.5 * static_cast<double>(kBuiltinRealtimeWidth),
                .cy = 0.5 * static_cast<double>(kBuiltinRealtimeHeight),
            },
            .base_move_speed = 1.8,
        }},
};

const SceneRegistryEntry* find_scene_registry_entry(std::string_view scene_id) {
    for (const SceneRegistryEntry& entry : kSceneRegistry) {
        if (entry.metadata.id == scene_id) {
            return &entry;
        }
    }
    return nullptr;
}

const CpuPresetRegistryEntry* find_cpu_preset_registry_entry(std::string_view scene_id, std::string_view preset_id) {
    for (const CpuPresetRegistryEntry& entry : kCpuPresetRegistry) {
        if (entry.preset.scene_id == scene_id && entry.preset.preset_id == preset_id) {
            return &entry;
        }
    }
    return nullptr;
}

const CpuPresetRegistryEntry* find_default_cpu_preset_registry_entry(std::string_view scene_id) {
    for (const CpuPresetRegistryEntry& entry : kCpuPresetRegistry) {
        if (entry.preset.scene_id == scene_id && entry.is_default) {
            return &entry;
        }
    }
    return nullptr;
}

const RealtimePresetRegistryEntry* find_realtime_preset_registry_entry(std::string_view scene_id) {
    for (const RealtimePresetRegistryEntry& entry : kRealtimePresetRegistry) {
        if (entry.scene_id == scene_id) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace

const std::vector<SceneDefinition>& builtin_scene_definitions() {
    static const std::vector<SceneDefinition> definitions = []() {
        std::vector<SceneDefinition> out;
        out.reserve(kSceneRegistry.size());
        for (const SceneRegistryEntry& entry : kSceneRegistry) {
            SceneDefinition definition;
            definition.metadata.id = std::string(entry.metadata.id);
            definition.metadata.label = std::string(entry.metadata.label);
            definition.metadata.background = entry.metadata.background;
            definition.metadata.supports_cpu_render = entry.metadata.supports_cpu_render;
            definition.metadata.supports_realtime = entry.metadata.supports_realtime;
            definition.scene_ir = entry.builder();

            for (const CpuPresetRegistryEntry& preset : kCpuPresetRegistry) {
                if (preset.preset.scene_id != entry.metadata.id) {
                    continue;
                }
                definition.cpu_presets.push_back(SceneDefinitionCpuRenderPreset {
                    .scene_id = std::string(preset.preset.scene_id),
                    .preset_id = std::string(preset.preset.preset_id),
                    .samples_per_pixel = preset.preset.samples_per_pixel,
                    .camera = preset.preset.camera,
                });
            }

            if (const RealtimePresetRegistryEntry* preset = find_realtime_preset_registry_entry(entry.metadata.id);
                preset != nullptr) {
                definition.realtime_preset = preset->preset;
            }

            out.push_back(std::move(definition));
        }
        return out;
    }();
    return definitions;
}

const SceneDefinition* find_builtin_scene_definition(std::string_view scene_id) {
    for (const SceneDefinition& definition : builtin_scene_definitions()) {
        if (definition.metadata.id == scene_id) {
            return &definition;
        }
    }
    return nullptr;
}

const std::vector<SceneMetadata>& scene_metadata() {
    return global_scene_file_catalog().entries();
}

const SceneMetadata* find_scene_metadata(std::string_view scene_id) {
    for (const SceneMetadata& entry : scene_metadata()) {
        if (entry.id == scene_id) {
            return &entry;
        }
    }
    return nullptr;
}

const CpuRenderPreset* find_cpu_render_preset(std::string_view scene_id, std::string_view preset_id) {
    return global_scene_file_catalog().find_cpu_render_preset(scene_id, preset_id);
}

const CpuRenderPreset* default_cpu_render_preset(std::string_view scene_id) {
    if (find_scene_metadata(scene_id) == nullptr) {
        throw std::invalid_argument("unknown shared scene id");
    }
    const CpuRenderPreset* preset = global_scene_file_catalog().default_cpu_render_preset(scene_id);
    if (preset == nullptr) {
        throw std::runtime_error("shared scene is missing its default CPU preset");
    }
    return preset;
}

const RealtimeViewPreset* find_realtime_view_preset(std::string_view scene_id) {
    return global_scene_file_catalog().find_realtime_view_preset(scene_id);
}

Eigen::Vector3d scene_background(std::string_view scene_id) {
    const SceneMetadata* metadata = find_scene_metadata(scene_id);
    if (metadata == nullptr) {
        throw std::invalid_argument("unknown shared scene id");
    }
    return metadata->background;
}

SceneIR build_scene(std::string_view scene_id) {
    const SceneDefinition* definition = global_scene_file_catalog().find_scene(scene_id);
    if (definition == nullptr) {
        throw std::invalid_argument("unknown shared scene id");
    }
    return definition->scene_ir;
}

}  // namespace rt::scene
