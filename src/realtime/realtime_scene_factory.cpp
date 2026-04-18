#include "realtime/realtime_scene_factory.h"

#include "realtime/scene_catalog.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"

#include <Eigen/Core>

#include <cmath>
#include <stdexcept>

namespace rt {
namespace {

constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

viewer::BodyPose pose_from_look_at(const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat) {
    const Eigen::Vector3d direction = (lookat - lookfrom).normalized();
    const double pitch_rad = std::asin(std::clamp(direction.z(), -1.0, 1.0));
    const double yaw_rad = std::atan2(-direction.x(), direction.y());
    return viewer::BodyPose {
        .position = lookfrom,
        .yaw_deg = yaw_rad * kRadToDeg,
        .pitch_deg = pitch_rad * kRadToDeg,
    };
}

}  // namespace

bool realtime_scene_supported(std::string_view scene_id) {
    const SceneCatalogEntry* entry = find_scene_catalog_entry(scene_id);
    return entry != nullptr && entry->supports_realtime;
}

SceneDescription make_realtime_scene(std::string_view scene_id) {
    if (!realtime_scene_supported(scene_id)) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    return scene::adapt_to_realtime(scene::build_scene(scene_id));
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
    if (scene_id == "bouncing_spheres" || scene_id == "checkered_spheres" || scene_id == "perlin_spheres") {
        return pose_from_look_at(Eigen::Vector3d {13.0, 2.0, 3.0}, Eigen::Vector3d::Zero());
    }
    if (scene_id == "earth_sphere") {
        return pose_from_look_at(Eigen::Vector3d {-3.0, 6.0, -10.0}, Eigen::Vector3d::Zero());
    }
    if (scene_id == "quads") {
        return pose_from_look_at(Eigen::Vector3d {0.0, 0.0, 9.0}, Eigen::Vector3d::Zero());
    }
    if (scene_id == "simple_light") {
        return pose_from_look_at(Eigen::Vector3d {26.0, 3.0, 6.0}, Eigen::Vector3d {0.0, 2.0, 0.0});
    }
    if (scene_id == "cornell_smoke" || scene_id == "cornell_smoke_extreme" || scene_id == "cornell_box"
        || scene_id == "cornell_box_extreme" || scene_id == "cornell_box_and_sphere"
        || scene_id == "cornell_box_and_sphere_extreme") {
        return pose_from_look_at(Eigen::Vector3d {278.0, 278.0, -800.0}, Eigen::Vector3d {278.0, 278.0, 0.0});
    }
    if (scene_id == "rttnw_final_scene" || scene_id == "rttnw_final_scene_extreme") {
        return pose_from_look_at(Eigen::Vector3d {478.0, 278.0, -600.0}, Eigen::Vector3d {278.0, 278.0, 0.0});
    }
    throw std::invalid_argument("unsupported realtime scene");
}

}  // namespace rt
