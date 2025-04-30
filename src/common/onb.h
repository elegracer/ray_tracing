#pragma once

#include "common.h"

struct ONB {
    ONB(const Vec3d& n) {
        const Vec3d z = n.normalized();
        const Vec3d a = (std::abs(z.x()) > 0.9) ? Vec3d {0.0, 1.0, 0.0} : Vec3d {1.0, 0.0, 0.0};
        const Vec3d x = z.cross(a).normalized();
        const Vec3d y = z.cross(x).normalized();
        m_Rlb.col(0) = x;
        m_Rlb.col(1) = y;
        m_Rlb.col(2) = z;
    }

    const Vec3d u() const { return m_Rlb.col(0); }
    const Vec3d v() const { return m_Rlb.col(1); }
    const Vec3d w() const { return m_Rlb.col(2); }

    Vec3d from_basis(const Vec3d& p) const {
        // Transform from basis coordinates to local space
        return m_Rlb * p;
    }

    Vec3d to_basis(const Vec3d& p) const {
        // Transform from local space to basis coordinates
        return m_Rlb.transpose() * p;
    }

    Eigen::Matrix3d m_Rlb; // 3D rotation from basic to local
};
