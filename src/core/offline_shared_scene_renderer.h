#pragma once

#include <opencv2/core/mat.hpp>

#include "realtime/camera_rig.h"

#include <string_view>

namespace rt {

cv::Mat render_shared_scene(std::string_view scene_id, int samples_per_pixel);
cv::Mat render_shared_scene_from_camera(std::string_view scene_id, const PackedCamera& camera, int samples_per_pixel);

}  // namespace rt
