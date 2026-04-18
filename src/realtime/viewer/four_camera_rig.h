#pragma once

#include "realtime/camera_rig.h"
#include "realtime/viewer/body_pose.h"

namespace rt::viewer {

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height);

}  // namespace rt::viewer

