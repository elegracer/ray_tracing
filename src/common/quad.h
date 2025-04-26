#pragma once

#include "material.h"
#include "aabb.h"

struct Quad {

    Quad(const Vec3d& Q, const Vec3d& u, const Vec3d& v, pro::proxy<Material> mat)
        : m_Q(Q),
          m_u(u),
          m_v(v),
          m_mat(mat) {

        const Vec3d n = m_u.cross(m_v);
        m_normal = n.normalized();
        m_D = m_normal.dot(m_Q);
        m_w = n / n.dot(n);

        set_bounding_box();
    }

    void set_bounding_box() {
        // Compute the bounding box of all four verices
        auto bbox_diagonal1 = AABB {m_Q, m_Q + m_u + m_v};
        auto bbox_diagonal2 = AABB {m_Q + m_u, m_Q + m_v};
        m_bbox = AABB {bbox_diagonal1, bbox_diagonal2};
    }

    AABB bounding_box() const { return m_bbox; }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        const double denominator = m_normal.dot(ray.direction());

        // No hit if the ray is parallel to the plane
        if (std::abs(denominator) < 1e-8) {
            return false;
        }

        // Return false if the hit point parameter t is outside the ray interval.
        const double t = (m_D - m_normal.dot(ray.origin())) / denominator;
        if (!ray_t.contains(t)) {
            return false;
        }

        // Determine if the hit point lies within the planar shape using its plane coordinates.
        const Vec3d intersection = ray.at(t);
        const Vec3d planar_hitpt_vector = intersection - m_Q;
        const double alpha = m_w.dot(planar_hitpt_vector.cross(m_v));
        const double beta = m_w.dot(m_u.cross(planar_hitpt_vector));

        if (!is_interior(alpha, beta, hit_rec)) {
            return false;
        }

        // Ray hits the 2D shape. Set the rest of the hir record and return true
        hit_rec.t = t;
        hit_rec.p = intersection;
        hit_rec.mat = m_mat;
        hit_rec.set_face_normal(ray, m_normal);

        return true;
    }

    bool is_interior(const double a, const double b, HitRecord& hit_rec) const {
        const auto unit_interval = Interval {0.0, 1.0};
        // Given the hit point in plane coordinates,
        //  return false if it is outside the primitive,
        //  otherwise set the hit record UV coordinates and return true.

        if (!unit_interval.contains(a) || !unit_interval.contains(b)) {
            return false;
        }

        hit_rec.u = a;
        hit_rec.v = b;
        return true;
    }

    Vec3d m_Q;
    Vec3d m_u;
    Vec3d m_v;
    Vec3d m_w;
    pro::proxy<Material> m_mat;
    AABB m_bbox;

    // plane parameters
    Vec3d m_normal;
    double m_D;
};
