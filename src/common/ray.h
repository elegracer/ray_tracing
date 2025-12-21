#pragma once

#include "defs.h"

class Ray {

public:
    HD_ATTR Ray() = delete;
    HD_ATTR Ray(const Eigen::Vector3d& origin, const Eigen::Vector3d& direction)
        : m_origin(origin),
          m_direction(direction) {}

    HD_ATTR auto&& origin() const { return m_origin; }
    HD_ATTR auto&& origin() { return m_origin; }
    HD_ATTR auto&& direction() const { return m_direction; }
    HD_ATTR auto&& direction() { return m_direction; }

    HD_ATTR Eigen::Vector3d at(const double t) const { return m_origin + m_direction * t; }

private:
    Eigen::Vector3d m_origin;
    Eigen::Vector3d m_direction;
};
