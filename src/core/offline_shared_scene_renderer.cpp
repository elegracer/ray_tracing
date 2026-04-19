#include "core/offline_shared_scene_renderer.h"

#include <Eigen/Geometry>

#include "common/camera.h"
#include "realtime/camera_rig.h"
#include "realtime/scene_catalog.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"

#include <numbers>
#include <stdexcept>

namespace rt {
namespace {

void configure_offline_camera(std::string_view scene_id, const int samples_per_pixel, Camera& cam) {
    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 1280;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = 50;
    cam.vup = {0.0, 1.0, 0.0};
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    if (scene_id == "bouncing_spheres") {
        cam.background = {0.70, 0.80, 1.00};
        cam.vfov = 20.0;
        cam.lookfrom = {13.0, 2.0, 3.0};
        cam.lookat = {0.0, 0.0, 0.0};
        cam.defocus_angle = 0.6;
    } else if (scene_id == "checkered_spheres" || scene_id == "perlin_spheres") {
        cam.background = {0.70, 0.80, 1.00};
        cam.vfov = 20.0;
        cam.lookfrom = {13.0, 2.0, 3.0};
        cam.lookat = {0.0, 0.0, 0.0};
    } else if (scene_id == "earth_sphere") {
        cam.background = {0.70, 0.80, 1.00};
        cam.vfov = 20.0;
        cam.lookfrom = {-3.0, 6.0, -10.0};
        cam.lookat = {0.0, 0.0, 0.0};
    } else if (scene_id == "quads") {
        cam.background = {0.70, 0.80, 1.00};
        cam.vfov = 80.0;
        cam.lookfrom = {0.0, 0.0, 9.0};
        cam.lookat = {0.0, 0.0, 0.0};
    } else if (scene_id == "simple_light") {
        cam.background = {0.0, 0.0, 0.0};
        cam.vfov = 20.0;
        cam.lookfrom = {26.0, 3.0, 6.0};
        cam.lookat = {0.0, 2.0, 0.0};
    } else if (scene_id == "cornell_smoke" || scene_id == "cornell_smoke_extreme" || scene_id == "cornell_box"
               || scene_id == "cornell_box_extreme" || scene_id == "cornell_box_and_sphere"
               || scene_id == "cornell_box_and_sphere_extreme") {
        cam.background = {0.0, 0.0, 0.0};
        cam.vfov = 40.0;
        cam.lookfrom = {278.0, 278.0, -800.0};
        cam.lookat = {278.0, 278.0, 0.0};
    } else if (scene_id == "rttnw_final_scene" || scene_id == "rttnw_final_scene_extreme") {
        cam.background = {0.0, 0.0, 0.0};
        cam.vfov = 40.0;
        cam.lookfrom = {478.0, 278.0, -600.0};
        cam.lookat = {278.0, 278.0, 0.0};
    } else {
        cam.background = {0.70, 0.80, 1.00};
        cam.vfov = 20.0;
        cam.lookfrom = {13.0, 2.0, 3.0};
        cam.lookat = {0.0, 0.0, 0.0};
    }
}

void configure_camera_from_packed(const PackedCamera& packed, const int samples_per_pixel, Camera& cam) {
    if (packed.model != CameraModelType::pinhole32) {
        throw std::invalid_argument("offline shared-scene packed-camera render only supports pinhole cameras");
    }
    if (packed.width <= 0 || packed.height <= 0) {
        throw std::invalid_argument("packed camera dimensions must be positive");
    }
    if (packed.pinhole.fy <= 0.0) {
        throw std::invalid_argument("packed pinhole fy must be positive");
    }

    const Eigen::Vector3d origin = packed.T_rc.block<3, 1>(0, 3);
    const Eigen::Matrix3d rotation = packed.T_rc.block<3, 3>(0, 0);
    const Eigen::Vector3d forward = rotation * Eigen::Vector3d::UnitZ();
    const Eigen::Vector3d up = rotation * -Eigen::Vector3d::UnitY();
    const double vfov_rad =
        2.0 * std::atan(0.5 * static_cast<double>(packed.height) / packed.pinhole.fy);

    cam.aspect_ratio = static_cast<double>(packed.width) / static_cast<double>(packed.height);
    cam.image_width = packed.width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = 50;
    cam.vfov = vfov_rad * 180.0 / std::numbers::pi;
    cam.lookfrom = origin;
    cam.lookat = origin + forward;
    cam.vup = up;
    cam.defocus_angle = 0.0;
    cam.focus_dist = 1.0;
}

}  // namespace

cv::Mat render_shared_scene(std::string_view scene_id, const int samples_per_pixel) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    if (entry == nullptr || !entry->supports_cpu_render) {
        throw std::invalid_argument("scene id is not available for offline CPU rendering");
    }

    const int resolved_spp =
        samples_per_pixel > 0 ? samples_per_pixel : scene::scene_default_samples_per_pixel(scene_id);
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);
    if (!adapted.world.has_value()) {
        throw std::runtime_error("adapted CPU world is empty");
    }

    Camera cam;
    configure_offline_camera(scene_id, resolved_spp, cam);
    cam.render(adapted.world, adapted.lights);
    return cam.img.clone();
}

cv::Mat render_shared_scene_from_camera(std::string_view scene_id, const PackedCamera& camera,
    const int samples_per_pixel) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    if (entry == nullptr) {
        throw std::invalid_argument("scene id is not available for offline CPU rendering");
    }

    const int resolved_spp =
        samples_per_pixel > 0 ? samples_per_pixel : scene::scene_default_samples_per_pixel(scene_id);
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);
    if (!adapted.world.has_value()) {
        throw std::runtime_error("adapted CPU world is empty");
    }

    Camera cam;
    configure_offline_camera(scene_id, resolved_spp, cam);
    configure_camera_from_packed(camera, resolved_spp, cam);
    cam.render(adapted.world, adapted.lights);
    return cam.img.clone();
}

}  // namespace rt
