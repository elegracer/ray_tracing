#pragma once

#include "realtime/camera_models.h"
#include "realtime/frame_convention.h"
#include "scene/camera_spec.h"

#include <sophus/se3.hpp>

#include <array>
#include <variant>
#include <vector>

namespace rt {

struct PackedCamera {
    PackedCamera();

    int enabled = 0;
    int width = 0;
    int height = 0;
    CameraModelType model = CameraModelType::pinhole32;
    Sophus::SE3d T_rc {};
    Pinhole32Params pinhole{};
    Equi62Lut1DParams equi{};
};

struct PackedCameraRig {
    int active_count = 0;
    std::array<PackedCamera, 4> cameras{};
};

class CameraRig {
   public:
    void add_camera(const scene::CameraSpec& spec);
    void add_pinhole(const Pinhole32Params& params, const Sophus::SE3d& T_bc, int width, int height);
    void add_equi62(const Equi62Lut1DParams& params, const Sophus::SE3d& T_bc, int width, int height);

    PackedCameraRig pack() const;

   private:
    struct Slot {
        CameraModelType model;
        Sophus::SE3d T_bc {};
        int width = 0;
        int height = 0;
        std::variant<Pinhole32Params, Equi62Lut1DParams> params;
    };

    std::vector<Slot> slots_;
};

}  // namespace rt
