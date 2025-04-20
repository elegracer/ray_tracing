#pragma once

#include "common.h"
#include "hittable.h"
#include "icecream.hpp"

class Sphere {
public:
    Sphere() = delete;

    // Stationary Sphere
    Sphere(const Vec3d& static_center, const double radius, pro::proxy<Material> mat)
        : m_center(static_center, {0.0, 0.0, 0.0}),
          m_radius(std::max(0.0, radius)),
          m_mat(mat) {
        const Vec3d radius_vec {radius, radius, radius};
        m_bbox = AABB {static_center - radius_vec, static_center + radius_vec};
    }

    // Moving Sphere
    Sphere(const Vec3d& center1, const Vec3d& center2, const double radius,
        pro::proxy<Material> mat)
        : m_center(center1, center2 - center1),
          m_radius(std::max(0.0, radius)),
          m_mat(mat) {
        const Vec3d radius_vec {radius, radius, radius};
        m_bbox = AABB {
            AABB {m_center.at(0.0) - radius_vec, m_center.at(0.0) + radius_vec}, //
            AABB {m_center.at(1.0) - radius_vec, m_center.at(1.0) + radius_vec}  //
        };
    }


    AABB bounding_box() const { return m_bbox; }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        const Vec3d current_center = m_center.at(ray.time());
        const Vec3d oc = current_center - ray.origin();
        const double a = ray.direction().squaredNorm();
        const double h = ray.direction().dot(oc);
        const double c = oc.squaredNorm() - m_radius * m_radius;
        const double discriminant = h * h - a * c;

        if (discriminant < 0.0) {
            return false;
        }

        const double sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range
        double root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root)) {
                return false;
            }
        }

        hit_rec.t = root;
        hit_rec.p = ray.at(hit_rec.t);
        const Vec3d outward_normal = (hit_rec.p - current_center) / m_radius;
        hit_rec.set_face_normal(ray, outward_normal);
        hit_rec.mat = m_mat;

        return true;
    }

private:
    Ray m_center;
    double m_radius;
    pro::proxy<Material> m_mat;
    AABB m_bbox;
};
