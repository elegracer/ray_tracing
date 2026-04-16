#include "realtime/camera_rig.h"

#include <stdexcept>

namespace rt {

PackedCamera::PackedCamera()
    : equi {} {
    equi.tangential = Eigen::Vector2d::Zero();
    equi.lut.fill(0.0);
}

void CameraRig::add_pinhole(const Pinhole32Params& params, const Eigen::Isometry3d& T_bc, int width, int height) {
    if (slots_.size() >= 4) {
        throw std::runtime_error("camera rig supports at most 4 cameras");
    }

    slots_.push_back(Slot {
        CameraModelType::pinhole32,
        T_bc,
        width,
        height,
        params,
    });
}

void CameraRig::add_equi62(const Equi62Lut1DParams& params, const Eigen::Isometry3d& T_bc, int width, int height) {
    if (slots_.size() >= 4) {
        throw std::runtime_error("camera rig supports at most 4 cameras");
    }

    slots_.push_back(Slot {
        CameraModelType::equi62_lut1d,
        T_bc,
        width,
        height,
        params,
    });
}

PackedCameraRig CameraRig::pack() const {
    PackedCameraRig out {};
    out.active_count = static_cast<int>(slots_.size());

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const Slot& slot = slots_[i];
        PackedCamera& packed = out.cameras[i];
        packed.enabled = 1;
        packed.width = slot.width;
        packed.height = slot.height;
        packed.model = slot.model;

        Eigen::Isometry3d T_rc = Eigen::Isometry3d::Identity();
        T_rc.linear() = camera_to_renderer_matrix() * slot.T_bc.linear();
        T_rc.translation() = body_to_renderer_matrix() * slot.T_bc.translation();
        packed.T_rc = T_rc.matrix();

        if (slot.model == CameraModelType::pinhole32) {
            packed.pinhole = std::get<Pinhole32Params>(slot.params);
        } else {
            packed.equi = std::get<Equi62Lut1DParams>(slot.params);
        }
    }

    return out;
}

}  // namespace rt
