#pragma once

#include "realtime/camera_rig.h"
#include "realtime/scene_description.h"
#include "realtime/viewer/body_pose.h"

#include <string_view>

namespace rt {

bool realtime_scene_supported(std::string_view scene_id);
SceneDescription make_realtime_scene(std::string_view scene_id);
CameraRig default_camera_rig_for_scene(std::string_view scene_id, int camera_count, int width, int height);
viewer::ViewerFrameConvention viewer_frame_convention_for_scene(std::string_view scene_id);
viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id);
double default_move_speed_for_scene(std::string_view scene_id);

}  // namespace rt
