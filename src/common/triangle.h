#pragma once

#include "aabb.h"
#include "common.h"
#include "interval.h"
#include "material.h"
#include "ray.h"

#include <cmath>

struct Triangle {
    Triangle(const Vec3d& a, const Vec3d& b, const Vec3d& c, pro::proxy<Material> mat)
        : m_a(a),
          m_b(b),
          m_c(c),
          m_edge_ab(b - a),
          m_edge_ac(c - a),
          m_mat(mat) {
        const Vec3d normal = m_edge_ab.cross(m_edge_ac);
        m_area = 0.5 * normal.norm();
        m_bbox = AABB {AABB {m_a, m_b}, AABB {AABB {m_a, m_c}, AABB {m_b, m_c}}};
    }

    AABB bounding_box() const { return m_bbox; }

    double pdf_value(const Vec3d& origin, const Vec3d& direction) const {
        if (m_area <= 1e-12) {
            return 0.0;
        }

        HitRecord hit_rec;
        if (!hit(Ray {origin, direction}, Interval {0.001, infinity}, hit_rec)) {
            return 0.0;
        }

        const double distance_sq = hit_rec.t * hit_rec.t * direction.squaredNorm();
        const double cosine = std::abs(direction.normalized().dot(hit_rec.normal));
        if (cosine <= 1e-12) {
            return 0.0;
        }

        return distance_sq / (cosine * m_area);
    }

    Vec3d random(const Vec3d& origin) const {
        const double sqrt_r1 = std::sqrt(random_double());
        const double r2 = random_double();
        const double w0 = 1.0 - sqrt_r1;
        const double w1 = sqrt_r1 * (1.0 - r2);
        const double w2 = sqrt_r1 * r2;
        const Vec3d p = (w0 * m_a) + (w1 * m_b) + (w2 * m_c);
        return p - origin;
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        constexpr double kEpsilon = 1e-8;

        const Vec3d pvec = ray.direction().cross(m_edge_ac);
        const double det = m_edge_ab.dot(pvec);
        if (std::abs(det) <= kEpsilon) {
            return false;
        }

        const double inv_det = 1.0 / det;
        const Vec3d tvec = ray.origin() - m_a;
        const double u = tvec.dot(pvec) * inv_det;
        if (u < 0.0 || u > 1.0) {
            return false;
        }

        const Vec3d qvec = tvec.cross(m_edge_ab);
        const double v = ray.direction().dot(qvec) * inv_det;
        if (v < 0.0 || (u + v) > 1.0) {
            return false;
        }

        const double t = m_edge_ac.dot(qvec) * inv_det;
        if (!ray_t.surrounds(t)) {
            return false;
        }

        const Vec3d outward_normal = m_edge_ab.cross(m_edge_ac).normalized();
        hit_rec.t = t;
        hit_rec.p = ray.at(t);
        hit_rec.u = u;
        hit_rec.v = v;
        hit_rec.mat = m_mat;
        hit_rec.set_face_normal(ray, outward_normal);
        return true;
    }

    Vec3d m_a;
    Vec3d m_b;
    Vec3d m_c;
    Vec3d m_edge_ab;
    Vec3d m_edge_ac;
    pro::proxy<Material> m_mat;
    AABB m_bbox;
    double m_area = 0.0;
};
