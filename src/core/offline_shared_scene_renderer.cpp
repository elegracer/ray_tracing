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

bool nearly_equal(const double lhs, const double rhs, const double tolerance = 1e-6) {
    return std::abs(lhs - rhs) <= tolerance;
}

void configure_offline_camera(const scene::CpuRenderPreset& preset, const int samples_per_pixel, Camera& cam) {
    cam.aspect_ratio = preset.camera.aspect_ratio;
    cam.image_width = preset.camera.image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = preset.camera.max_depth;
    cam.vfov = preset.camera.vfov;
    cam.lookfrom = preset.camera.lookfrom;
    cam.lookat = preset.camera.lookat;
    cam.vup = preset.camera.vup;
    cam.background = preset.camera.background;
    cam.defocus_angle = preset.camera.defocus_angle;
    cam.focus_dist = preset.camera.focus_dist;
}

void configure_camera_from_packed(const PackedCamera& packed, const int samples_per_pixel, Camera& cam) {
    if (packed.model != CameraModelType::pinhole32) {
        throw std::invalid_argument("offline shared-scene packed-camera render only supports pinhole cameras");
    }
    if (packed.width <= 0 || packed.height <= 0) {
        throw std::invalid_argument("packed camera dimensions must be positive");
    }
    if (packed.pinhole.fx <= 0.0 || packed.pinhole.fy <= 0.0) {
        throw std::invalid_argument("packed pinhole focal lengths must be positive");
    }
    const double expected_cx = 0.5 * static_cast<double>(packed.width);
    const double expected_cy = 0.5 * static_cast<double>(packed.height);
    const double expected_fx =
        packed.pinhole.fy * static_cast<double>(packed.width) / static_cast<double>(packed.height);
    if (!nearly_equal(packed.pinhole.fx, expected_fx) || !nearly_equal(packed.pinhole.cx, expected_cx)
        || !nearly_equal(packed.pinhole.cy, expected_cy) || !nearly_equal(packed.pinhole.k1, 0.0)
        || !nearly_equal(packed.pinhole.k2, 0.0) || !nearly_equal(packed.pinhole.k3, 0.0)
        || !nearly_equal(packed.pinhole.p1, 0.0) || !nearly_equal(packed.pinhole.p2, 0.0)) {
        throw std::invalid_argument(
            "offline shared-scene packed-camera render only supports centered pinhole cameras without distortion");
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

    const scene::CpuRenderPreset* preset = scene::default_cpu_render_preset(scene_id);
    const int resolved_spp = samples_per_pixel > 0 ? samples_per_pixel : preset->samples_per_pixel;
    const scene::SceneIR scene_ir = scene::build_scene(scene_id);
    const scene::CpuSceneAdapterResult adapted = scene::adapt_to_cpu(scene_ir);
    if (!adapted.world.has_value()) {
        throw std::runtime_error("adapted CPU world is empty");
    }

    Camera cam;
    configure_offline_camera(*preset, resolved_spp, cam);
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
    cam.render(adapted.world, adapted.lights);
    return cam.img.clone();
}

}  // namespace rt
