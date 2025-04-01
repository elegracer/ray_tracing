#pragma once

#include "common.h"

class Ray {

public:
    Ray() = delete;
    Ray(const Vec3d& origin, const Vec3d& direction) : m_origin(origin), m_direction(direction) {}

    auto&& origin(this auto&& self) { return self.m_origin; }
    auto&& direction(this auto&& self) { return self.m_direction; }

    Vec3d at(const double t) const { return m_origin + m_direction * t; }

private:
    Vec3d m_origin;
    Vec3d m_direction;
};
