#pragma once

#include "realtime/camera_rig.h"

#include <string_view>

namespace rt {

void validate_render_camera_request(
    const PackedCameraRig& rig, int camera_index, std::string_view caller);
void validate_render_pool_request(
    const PackedCameraRig& rig, int active_cameras, int renderer_count, std::string_view caller);

}  // namespace rt
