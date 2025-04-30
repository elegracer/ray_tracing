#pragma once

#include "proxy/proxy.h"

#include "common.h"
#include "onb.h"

PRO_DEF_MEM_DISPATCH(PDFMemValue, value);
PRO_DEF_MEM_DISPATCH(PDFMemGenerate, generate);

struct PDF                                                                //
    : pro::facade_builder                                                 //
      ::support_copy<pro::constraint_level::nontrivial>                   //
      ::add_convention<PDFMemValue, double(const Vec3d& direction) const> //
      ::add_convention<PDFMemGenerate, Vec3d() const>                     //
      ::build {};

struct SpherePDF {
    double value(const Vec3d& direction) const { return 1.0 / (4.0 * pi); }
    Vec3d generate() const { return random_unit_vector(); }
};

struct CosinePDF {
    CosinePDF(const Vec3d& w) : m_uvw(w) {}

    double value(const Vec3d& direction) const {
        const double cosine_theta = direction.normalized().dot(m_uvw.w());
        return std::max(0.0, cosine_theta / pi);
    }

    Vec3d generate() const { return m_uvw.from_basis(random_cosine_direction()); }

    ONB m_uvw;
};
