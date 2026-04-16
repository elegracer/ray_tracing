#pragma once

#include "common/common.h"

inline Vec3d camera_to_renderer(const Vec3d& v) {
    return {v.x(), -v.y(), -v.z()};
}

inline Vec3d body_to_renderer(const Vec3d& v) {
    return {-v.y(), v.x(), v.z()};
}
