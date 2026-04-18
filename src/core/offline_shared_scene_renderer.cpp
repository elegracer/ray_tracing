#include "core/offline_shared_scene_renderer.h"

#include <Eigen/Geometry>

#include "common/camera.h"
#include "realtime/scene_catalog.h"
#include "scene/cpu_scene_adapter.h"
#include "scene/shared_scene_builders.h"

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
        throw std::invalid_argument("no offline camera preset for scene id");
    }
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

}  // namespace rt
