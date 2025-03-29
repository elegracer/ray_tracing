#pragma once

#include "defs.h"

class Ray {

public:
    Ray() = delete;
    Ray(const Eigen::Vector3d& origin, const Eigen::Vector3d& direction)
        : m_origin(origin),
          m_direction(direction) {}

    auto&& origin(this auto&& self) { return self.m_origin; }
    auto&& direction(this auto&& self) { return self.m_direction; }

    Eigen::Vector3d at(const double t) const { return m_origin + m_direction * t; }

private:
    Eigen::Vector3d m_origin;
    Eigen::Vector3d m_direction;
};
