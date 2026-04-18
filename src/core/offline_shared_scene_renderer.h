#pragma once

#include <opencv2/core/mat.hpp>

#include <string_view>

namespace rt {

cv::Mat render_shared_scene(std::string_view scene_id, int samples_per_pixel);

}  // namespace rt
