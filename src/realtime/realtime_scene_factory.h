#pragma once

#include "realtime/scene_description.h"
#include "realtime/viewer/body_pose.h"

#include <string_view>

namespace rt {

bool realtime_scene_supported(std::string_view scene_id);
SceneDescription make_realtime_scene(std::string_view scene_id);
viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id);

}  // namespace rt
