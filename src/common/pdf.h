#pragma once

#include "traits.h"

#include "common.h"
#include "onb.h"


struct SpherePDF {
    double value(const Vec3d& direction) const { return 1.0 / (4.0 * pi); }
    Vec3d generate() const { return random_unit_vector(); }
};

struct CosinePDF {
    CosinePDF(const Vec3d& w) : m_uvw(w) {}

    double value(const Vec3d& direction) const {
        const double cosine_theta = direction.normalized().dot(m_uvw.w());
        return std::max(1e-8, cosine_theta / pi);
    }

    Vec3d generate() const { return m_uvw.from_basis(random_cosine_direction()); }

    ONB m_uvw;
};

struct UniformHemispherePDF {
    UniformHemispherePDF(const Vec3d& w) : m_uvw(w) {}

    double value(const Vec3d&) const { return 1.0 / (2.0 * pi); }

    Vec3d generate() const { return m_uvw.from_basis(random_uniform_hemisphere_direction()); }

    ONB m_uvw;
};

struct HittablePDF {
    HittablePDF(const pro::proxy<Hittable>& objects, const Vec3d& origin)
        : m_objects(objects),
          m_origin(origin) {}

    double value(const Vec3d& direction) const { return m_objects->pdf_value(m_origin, direction); }

    Vec3d generate() const { return m_objects->random(m_origin); }

    pro::proxy<Hittable> m_objects;
    Vec3d m_origin;
};
