#include "realtime/realtime_scene_factory.h"

#include "realtime/default_viewer_conventions.h"
#include "realtime/frame_convention.h"
#include "realtime/scene_catalog.h"
#include "scene/realtime_scene_adapter.h"
#include "scene/shared_scene_builders.h"

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rt {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kStereoBaseline = 0.03;

struct SceneViewPreset {
    double vfov_deg = 20.0;
    Eigen::Vector3d lookfrom = Eigen::Vector3d::Zero();
    Eigen::Vector3d lookat = Eigen::Vector3d::Zero();
};

const SceneViewPreset* find_scene_view_preset(std::string_view scene_id) {
    static const SceneViewPreset bouncing_like {
        .vfov_deg = 20.0,
        .lookfrom = Eigen::Vector3d {13.0, 2.0, 3.0},
        .lookat = Eigen::Vector3d::Zero(),
    };
    static const SceneViewPreset earth {
        .vfov_deg = 20.0,
        .lookfrom = Eigen::Vector3d {-3.0, 6.0, -10.0},
        .lookat = Eigen::Vector3d::Zero(),
    };
    static const SceneViewPreset quads {
        .vfov_deg = 80.0,
        .lookfrom = Eigen::Vector3d {0.0, 0.0, 9.0},
        .lookat = Eigen::Vector3d::Zero(),
    };
    static const SceneViewPreset simple_light {
        .vfov_deg = 20.0,
        .lookfrom = Eigen::Vector3d {26.0, 3.0, 6.0},
        .lookat = Eigen::Vector3d {0.0, 2.0, 0.0},
    };
    static const SceneViewPreset cornell {
        .vfov_deg = 40.0,
        .lookfrom = Eigen::Vector3d {278.0, 278.0, -800.0},
        .lookat = Eigen::Vector3d {278.0, 278.0, 0.0},
    };
    static const SceneViewPreset final_scene {
        .vfov_deg = 40.0,
        .lookfrom = Eigen::Vector3d {478.0, 278.0, -600.0},
        .lookat = Eigen::Vector3d {278.0, 278.0, 0.0},
    };

    if (scene_id == "bouncing_spheres" || scene_id == "checkered_spheres" || scene_id == "perlin_spheres") {
        return &bouncing_like;
    }
    if (scene_id == "earth_sphere") {
        return &earth;
    }
    if (scene_id == "quads") {
        return &quads;
    }
    if (scene_id == "simple_light") {
        return &simple_light;
    }
    if (scene_id == "cornell_smoke" || scene_id == "cornell_smoke_extreme" || scene_id == "cornell_box"
        || scene_id == "cornell_box_extreme" || scene_id == "cornell_box_and_sphere"
        || scene_id == "cornell_box_and_sphere_extreme") {
        return &cornell;
    }
    if (scene_id == "rttnw_final_scene" || scene_id == "rttnw_final_scene_extreme") {
        return &final_scene;
    }
    return nullptr;
}

Pinhole32Params make_pinhole_from_vfov(double vfov_deg, int width, int height) {
    const double theta = vfov_deg * kPi / 180.0;
    const double fy = 0.5 * static_cast<double>(height) / std::tan(theta * 0.5);
    return Pinhole32Params {
        .fx = fy,
        .fy = fy,
        .cx = 0.5 * static_cast<double>(width),
        .cy = 0.5 * static_cast<double>(height),
        .k1 = 0.0,
        .k2 = 0.0,
        .k3 = 0.0,
        .p1 = 0.0,
        .p2 = 0.0,
    };
}

Eigen::Matrix3d camera_rotation_from_look_at(const Eigen::Vector3d& lookfrom, const Eigen::Vector3d& lookat) {
    const Eigen::Vector3d forward = (lookat - lookfrom).normalized();
    Eigen::Vector3d right = Eigen::Vector3d::UnitY().cross(forward);
    if (right.squaredNorm() < 1e-12) {
        right = Eigen::Vector3d::UnitX();
    }
    right.normalize();
    const Eigen::Vector3d up = forward.cross(right).normalized();

    Eigen::Matrix3d rotation = Eigen::Matrix3d::Identity();
    rotation.col(0) = right;
    rotation.col(1) = up;
    rotation.col(2) = forward;
    return rotation;
}

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

CameraRig make_scene_view_rig(const SceneViewPreset& preset, int camera_count, int width, int height) {
    CameraRig rig;
    const Pinhole32Params pinhole = make_pinhole_from_vfov(preset.vfov_deg, width, height);
    const Eigen::Matrix3d R_rc = camera_rotation_from_look_at(preset.lookfrom, preset.lookat);
    const Eigen::Vector3d right = R_rc.col(0);
    const double center = 0.5 * static_cast<double>(camera_count - 1);

    for (int i = 0; i < camera_count; ++i) {
        const double offset = kStereoBaseline * (static_cast<double>(i) - center);
        const Eigen::Vector3d position = preset.lookfrom + right * offset;
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = body_to_renderer_matrix().transpose() * position;
        T_bc.linear() = camera_to_renderer_matrix().transpose() * R_rc;
        rig.add_pinhole(pinhole, T_bc, width, height);
    }

    return rig;
}

CameraRig make_final_room_rig(int camera_count, int width, int height) {
    CameraRig rig;
    const double fx = 0.75 * static_cast<double>(width);
    const double fy = 0.75 * static_cast<double>(height);
    const double cx = 0.5 * static_cast<double>(width);
    const double cy = 0.5 * static_cast<double>(height);

    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.linear() = front_camera_to_body_matrix().transpose()
            * Eigen::AngleAxisd(
                  kDefaultSurroundYawOffsetsDeg[static_cast<std::size_t>(i)] * kPi / 180.0,
                  Eigen::Vector3d::UnitX())
                  .toRotationMatrix()
            * front_camera_to_body_matrix();
        rig.add_pinhole(Pinhole32Params {fx, fy, cx, cy, 0.0, 0.0, 0.0, 0.0, 0.0}, T_bc, width, height);
    }

    return rig;
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

CameraRig default_camera_rig_for_scene(std::string_view scene_id, int camera_count, int width, int height) {
    if (!realtime_scene_supported(scene_id)) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    if (camera_count < 1 || camera_count > 4) {
        throw std::invalid_argument("camera_count must be in [1, 4]");
    }
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("camera rig dimensions must be positive");
    }

    if (scene_id == "final_room") {
        return make_final_room_rig(camera_count, width, height);
    }
    if (scene_id == "smoke") {
        const SceneViewPreset smoke {
            .vfov_deg = 67.38013505195957,
            .lookfrom = Eigen::Vector3d::Zero(),
            .lookat = legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}),
        };
        return make_scene_view_rig(smoke, camera_count, width, height);
    }

    const SceneViewPreset* preset = find_scene_view_preset(scene_id);
    if (preset == nullptr) {
        throw std::invalid_argument("unsupported realtime scene");
    }
    return make_scene_view_rig(*preset, camera_count, width, height);
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

    const SceneViewPreset* preset = find_scene_view_preset(scene_id);
    if (preset != nullptr) {
        return pose_from_look_at(preset->lookfrom, preset->lookat);
    }
    throw std::invalid_argument("unsupported realtime scene");
}

}  // namespace rt
