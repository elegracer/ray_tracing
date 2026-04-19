#include "realtime/camera_rig.h"

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

Equi62Lut1DParams to_equi62_lut1d_params(const scene::CameraSpec& spec) {
    return make_equi62_lut1d_params(spec.width, spec.height, spec.fx, spec.fy, spec.cx, spec.cy,
        spec.equi62_lut1d.radial, spec.equi62_lut1d.tangential);
}

}  // namespace

PackedCamera::PackedCamera()
    : equi {} {
    equi.tangential = Eigen::Vector2d::Zero();
    equi.lut.fill(0.0);
}

void CameraRig::add_camera(const scene::CameraSpec& spec) {
    if (spec.model == CameraModelType::pinhole32) {
        add_pinhole(to_pinhole32_params(spec), spec.T_bc, spec.width, spec.height);
        return;
    }

    add_equi62(to_equi62_lut1d_params(spec), spec.T_bc, spec.width, spec.height);
}

void CameraRig::add_pinhole(const Pinhole32Params& params, const Sophus::SE3d& T_bc, int width, int height) {
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

void CameraRig::add_equi62(const Equi62Lut1DParams& params, const Sophus::SE3d& T_bc, int width, int height) {
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

        packed.T_rc = Sophus::SE3d(
            camera_to_renderer_matrix() * slot.T_bc.rotationMatrix(),
            body_to_renderer_matrix() * slot.T_bc.translation());

        if (slot.model == CameraModelType::pinhole32) {
            packed.pinhole = std::get<Pinhole32Params>(slot.params);
        } else {
            packed.equi = std::get<Equi62Lut1DParams>(slot.params);
        }
    }

    return out;
}

}  // namespace rt
