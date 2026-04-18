#pragma once

#include "realtime/camera_rig.h"
#include "realtime/viewer/body_pose.h"

#include <array>

namespace rt::viewer {

inline constexpr std::array<double, 4> kDefaultViewerYawOffsetsDeg {0.0, 90.0, -90.0, 180.0};

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height);

}  // namespace rt::viewer
