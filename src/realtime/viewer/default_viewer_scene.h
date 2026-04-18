#pragma once

#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

namespace rt::viewer {

SceneDescription make_final_room_scene();
SceneDescription make_default_viewer_scene();
RenderProfile default_viewer_profile();

}  // namespace rt::viewer
