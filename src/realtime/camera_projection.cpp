#include "realtime/camera_projection.h"

#include "realtime/camera_models.h"

#include <stdexcept>

namespace rt {

Eigen::Vector2d project_camera_pixel(const PackedCamera& camera, const Eigen::Vector3d& dir_camera) {
    switch (camera.model) {
    case CameraModelType::pinhole32:
        return project_pinhole32(camera.pinhole, dir_camera);
    case CameraModelType::equi62_lut1d:
        return project_equi62_lut1d(camera.equi, dir_camera);
    }
    throw std::invalid_argument("unsupported camera model");
}

Eigen::Vector3d unproject_camera_pixel(const PackedCamera& camera, const Eigen::Vector2d& pixel) {
    switch (camera.model) {
    case CameraModelType::pinhole32:
        return unproject_pinhole32(camera.pinhole, pixel);
    case CameraModelType::equi62_lut1d:
        return unproject_equi62_lut1d(camera.equi, pixel);
    }
    throw std::invalid_argument("unsupported camera model");
}

Eigen::Vector3d world_direction_for_pixel(const PackedCamera& camera, const Eigen::Vector2d& pixel) {
    return (camera.T_rc.rotationMatrix() * unproject_camera_pixel(camera, pixel)).normalized();
}

}  // namespace rt
