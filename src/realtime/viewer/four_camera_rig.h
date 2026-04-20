#pragma once

#include "realtime/camera_rig.h"
#include "realtime/default_viewer_conventions.h"
#include "realtime/viewer/body_pose.h"
#include "scene/camera_spec.h"

#include <span>

namespace rt::viewer {

CameraRig make_default_viewer_rig(const BodyPose& pose, std::span<const scene::CameraSpec> camera_specs,
    int width, int height, ViewerFrameConvention convention);

CameraRig make_default_viewer_rig(const BodyPose& pose, const scene::CameraSpec& camera,
    int camera_count, int width, int height, ViewerFrameConvention convention);

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height, ViewerFrameConvention convention);

CameraRig make_default_viewer_rig(const BodyPose& pose, std::span<const scene::CameraSpec> camera_specs,
    int width, int height);

CameraRig make_default_viewer_rig(const BodyPose& pose, const scene::CameraSpec& camera, int camera_count, int width,
    int height);

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height);

}  // namespace rt::viewer
