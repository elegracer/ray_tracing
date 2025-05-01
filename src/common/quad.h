#pragma once

#include "traits.h"

#include "hittable_list.h"
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

        m_area = n.norm();

        set_bounding_box();
    }

    void set_bounding_box() {
        // Compute the bounding box of all four verices
        auto bbox_diagonal1 = AABB {m_Q, m_Q + m_u + m_v};
        auto bbox_diagonal2 = AABB {m_Q + m_u, m_Q + m_v};
        m_bbox = AABB {bbox_diagonal1, bbox_diagonal2};
    }

    AABB bounding_box() const { return m_bbox; }

    double pdf_value(const Vec3d& origin, const Vec3d& direction) const {
        HitRecord hit_rec;
        if (!hit(Ray {origin, direction}, Interval {0.001, infinity}, hit_rec)) {
            return 0.0;
        }

        const double distance_sq = hit_rec.t * hit_rec.t * direction.squaredNorm();
        const double cosine = std::abs(direction.normalized().dot(hit_rec.normal));

        return distance_sq / (cosine * m_area);
    }

    Vec3d random(const Vec3d& origin) const {
        const Vec3d p = m_Q + (random_double() * m_u) + (random_double() * m_v);
        return p - origin;
    }

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
    double m_area;
};


inline pro::proxy<Hittable> box(const Vec3d& a, const Vec3d& b, pro::proxy<Material> mat) {
    // Returns the 3D box (six sides) that contains the two opposite vertices a & b

    auto sides = std::make_shared<HittableList>();

    // Construct the two opposite vertices with the minimum and maximum coordinates.
    const Vec3d min = a.array().min(b.array()).matrix();
    const Vec3d max = a.array().max(b.array()).matrix();

    const Vec3d dx {max.x() - min.x(), 0.0, 0.0};
    const Vec3d dy {0.0, max.y() - min.y(), 0.0};
    const Vec3d dz {0.0, 0.0, max.z() - min.z()};

    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {min.x(), min.y(), max.z()}, dx, dy,
        mat)); // front
    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {max.x(), min.y(), max.z()}, -dz, dy,
        mat)); // right
    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {max.x(), min.y(), min.z()}, -dx, dy,
        mat)); // back
    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {min.x(), min.y(), min.z()}, dz, dy,
        mat)); // left
    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {min.x(), max.y(), max.z()}, dx, -dz,
        mat)); // top
    sides->add(pro::make_proxy_shared<Hittable, Quad>(Vec3d {min.x(), min.y(), min.z()}, dx, dz,
        mat)); // bottom

    return sides;
}
