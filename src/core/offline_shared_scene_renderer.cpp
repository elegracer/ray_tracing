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

Pinhole32Params to_pinhole32_params(const scene::CameraSpec& spec) {
    return Pinhole32Params {
        spec.fx,
        spec.fy,
        spec.cx,
        spec.cy,
        spec.pinhole32.k1,
        spec.pinhole32.k2,
        spec.pinhole32.k3,
        spec.pinhole32.p1,
        spec.pinhole32.p2,
    };
}

Camera::SharedCameraRayConfig make_shared_camera_ray_config(
    const scene::CameraSpec& spec, const Eigen::Vector3d& origin, const Eigen::Matrix3d& camera_to_world) {
    if (spec.width <= 0 || spec.height <= 0) {
        throw std::invalid_argument("offline shared-scene camera dimensions must be positive");
    }
    if (spec.fx <= 0.0 || spec.fy <= 0.0) {
        throw std::invalid_argument("offline shared-scene camera focal lengths must be positive");
    }

    Camera::SharedCameraRayConfig config {};
    config.model = spec.model;
    config.origin = origin;
    config.camera_to_world = camera_to_world;
    if (spec.model == CameraModelType::pinhole32) {
        config.pinhole = to_pinhole32_params(spec);
    } else {
        config.equi = make_equi62_lut1d_params(spec.width, spec.height, spec.fx, spec.fy, spec.cx, spec.cy,
            spec.equi62_lut1d.radial, spec.equi62_lut1d.tangential);
    }
    return config;
}

Camera::SharedCameraRayConfig make_shared_camera_ray_config(const PackedCamera& packed) {
    if (packed.width <= 0 || packed.height <= 0) {
        throw std::invalid_argument("packed camera dimensions must be positive");
    }
    if (packed.model == CameraModelType::pinhole32) {
        if (packed.pinhole.fx <= 0.0 || packed.pinhole.fy <= 0.0) {
            throw std::invalid_argument("packed pinhole focal lengths must be positive");
        }
    } else if (packed.equi.fx <= 0.0 || packed.equi.fy <= 0.0) {
        throw std::invalid_argument("packed equi focal lengths must be positive");
    }

    Camera::SharedCameraRayConfig config {};
    config.model = packed.model;
    config.origin = packed.T_rc.translation();
    config.camera_to_world = packed.T_rc.rotationMatrix();
    if (packed.model == CameraModelType::pinhole32) {
        config.pinhole = packed.pinhole;
    } else {
        config.equi = packed.equi;
    }
    return config;
}

Eigen::Matrix3d make_camera_to_world(
    const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat, const Eigen::Vector3d& vup) {
    const Eigen::Vector3d w = (lookfrom - lookat).normalized();
    const Eigen::Vector3d u = vup.cross(w).normalized();
    const Eigen::Vector3d v = w.cross(u).normalized();

    Eigen::Matrix3d camera_to_world;
    camera_to_world.col(0) = u;
    camera_to_world.col(1) = -v;
    camera_to_world.col(2) = -w;
    return camera_to_world;
}

void configure_offline_camera(const scene::CpuRenderPreset& preset, const int samples_per_pixel, Camera& cam) {
    const scene::CameraSpec& spec = preset.camera.camera;
    cam.aspect_ratio = static_cast<double>(spec.width) / static_cast<double>(spec.height);
    cam.image_width = spec.width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = preset.camera.max_depth;
    cam.lookfrom = preset.camera.lookfrom;
    cam.lookat = preset.camera.lookat;
    cam.vup = preset.camera.vup;
    cam.defocus_angle = spec.model == CameraModelType::pinhole32 ? preset.camera.defocus_angle : 0.0;
    cam.focus_dist = preset.camera.focus_dist;
    cam.set_shared_camera_ray_config(make_shared_camera_ray_config(
        spec, preset.camera.lookfrom, make_camera_to_world(preset.camera.lookfrom, preset.camera.lookat, preset.camera.vup)));
}

void configure_camera_from_packed(const PackedCamera& packed, const int samples_per_pixel, Camera& cam) {
    cam.aspect_ratio = static_cast<double>(packed.width) / static_cast<double>(packed.height);
    cam.image_width = packed.width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = 50;
    cam.lookfrom = packed.T_rc.translation();
    cam.lookat = cam.lookfrom + packed.T_rc.rotationMatrix() * Eigen::Vector3d::UnitZ();
    cam.vup = packed.T_rc.rotationMatrix() * -Eigen::Vector3d::UnitY();
    cam.defocus_angle = 0.0;
    cam.focus_dist = 1.0;
    cam.set_shared_camera_ray_config(make_shared_camera_ray_config(packed));
}

}  // namespace

cv::Mat render_shared_scene(std::string_view scene_id, const int samples_per_pixel) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    if (entry == nullptr || !entry->supports_cpu_render) {
        throw std::invalid_argument("scene id is not available for offline CPU rendering");
    }

    const scene::CpuRenderPreset* preset = scene::default_cpu_render_preset(scene_id);
    const int resolved_spp = samples_per_pixel > 0 ? samples_per_pixel : preset->samples_per_pixel;
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);
    if (!adapted.world.has_value()) {
        throw std::runtime_error("adapted CPU world is empty");
    }

    Camera cam;
    configure_offline_camera(*preset, resolved_spp, cam);
    cam.background = scene::scene_background(scene_id);
    cam.render(adapted.world, adapted.lights);
    return cam.img.clone();
}

cv::Mat render_shared_scene_from_camera(std::string_view scene_id, const PackedCamera& camera,
    const int samples_per_pixel) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    if (entry == nullptr || !entry->supports_cpu_render) {
        throw std::invalid_argument("scene id is not available for offline CPU rendering");
    }

    const scene::CpuRenderPreset* preset = scene::default_cpu_render_preset(scene_id);
    const int resolved_spp = samples_per_pixel > 0 ? samples_per_pixel : preset->samples_per_pixel;
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);
    if (!adapted.world.has_value()) {
        throw std::runtime_error("adapted CPU world is empty");
    }

    Camera cam;
    configure_offline_camera(*preset, resolved_spp, cam);
    configure_camera_from_packed(camera, resolved_spp, cam);
    cam.background = scene::scene_background(scene_id);
    cam.render(adapted.world, adapted.lights);
    return cam.img.clone();
}

}  // namespace rt
