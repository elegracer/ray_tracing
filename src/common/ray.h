#pragma once

#include "common.h"
#include "common/openpbr_core.h"

class Ray {

public:
    Ray() = default;
    Ray(const Vec3d& origin, const Vec3d& direction) : m_origin(origin), m_direction(direction) {}
    Ray(const Vec3d& origin, const Vec3d& direction, const double time,
        const rt::OpenPbrSubsurfaceMedium& subsurface_medium = {},
        const void* subsurface_owner = nullptr)
        : m_origin(origin),
          m_direction(direction),
          m_time(time),
          m_subsurface_medium(subsurface_medium),
          m_subsurface_owner(subsurface_owner) {}

    auto&& origin(this auto&& self) { return self.m_origin; }
    auto&& direction(this auto&& self) { return self.m_direction; }

    double time() const { return m_time; }
    const rt::OpenPbrSubsurfaceMedium& subsurface_medium() const { return m_subsurface_medium; }
    const void* subsurface_owner() const { return m_subsurface_owner; }

    Vec3d at(const double t) const { return m_origin + m_direction * t; }

private:
    Vec3d m_origin = Vec3d::Zero();
    Vec3d m_direction = Vec3d::UnitX();
    double m_time = 0.0;
    rt::OpenPbrSubsurfaceMedium m_subsurface_medium {};
    const void* m_subsurface_owner = nullptr;
};
