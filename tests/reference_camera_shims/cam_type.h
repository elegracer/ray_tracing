#pragma once

namespace pico_cam {

enum CamType {
    CAM_MEI22 = 0,
    CAM_PINHOLE22,
    CAM_EQUI40,
    CAM_EQUI40_LUT1D,
    CAM_EQUI62,
    CAM_EQUI62_LUT1D,
    CAM_EQUI62_BILINEAR,
    CAM_PINHOLE32,
};

constexpr int HIGH_RESOLUTION_THRE = 1920;

}  // namespace pico_cam
