#pragma once

#include "defs.h"
#include "hittable.h"
#include "icecream.hpp"

class Sphere {
public:
    Sphere() = delete;
    Sphere(const Vec3d& center, const double radius)
        : m_center(center),
          m_radius(std::max(0.0, radius)) {}

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        const Vec3d oc = m_center - ray.origin();
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
        const Vec3d outward_normal = (hit_rec.p - m_center) / m_radius;
        hit_rec.set_face_normal(ray, outward_normal);

        return true;
    }

private:
    Vec3d m_center;
    double m_radius;
};
