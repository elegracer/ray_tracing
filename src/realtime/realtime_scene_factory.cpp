#include "realtime/realtime_scene_factory.h"

#include "realtime/frame_convention.h"
#include "realtime/viewer/default_viewer_scene.h"

#include <Eigen/Core>

#include <stdexcept>

namespace rt {
namespace {

SceneDescription make_smoke_scene() {
    SceneDescription scene;
    const int diffuse = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(
        SpherePrimitive {diffuse, legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5, false});
    scene.add_quad(QuadPrimitive {
        light,
        legacy_renderer_to_world(Eigen::Vector3d {-0.75, 1.25, -1.5}),
        legacy_renderer_to_world(Eigen::Vector3d {1.5, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.5}),
        false,
    });
    return scene;
}

}  // namespace

bool realtime_scene_supported(std::string_view scene_id) {
    return scene_id == "smoke" || scene_id == "final_room";
}

SceneDescription make_realtime_scene(std::string_view scene_id) {
    if (scene_id == "smoke") {
        return make_smoke_scene();
    }
    if (scene_id == "final_room") {
        return viewer::make_final_room_scene();
    }
    throw std::invalid_argument("unsupported realtime scene");
}

viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id) {
    if (scene_id == "smoke") {
        return viewer::BodyPose {
            .position = Eigen::Vector3d(0.0, -2.5, 0.25),
            .yaw_deg = 0.0,
            .pitch_deg = 0.0,
        };
    }
    if (scene_id == "final_room") {
        return viewer::default_spawn_pose();
    }
    throw std::invalid_argument("unsupported realtime scene");
}

}  // namespace rt
