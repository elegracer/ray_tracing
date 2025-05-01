#pragma once

#include "traits.h"

#include "material.h"
#include "interval.h"
#include "aabb.h"

struct ConstantMedium {

    explicit ConstantMedium(const pro::proxy<Hittable>& boundary, const double density,
        const pro::proxy<Texture>& tex)
        : m_boundary(boundary),
          m_neg_inv_density(-1.0 / density),
          m_phase_function(pro::make_proxy_shared<Material, Isotropic>(tex)) {}

    explicit ConstantMedium(const pro::proxy<Hittable>& boundary, const double density,
        const Vec3d& albedo)
        : m_boundary(boundary),
          m_neg_inv_density(-1.0 / density),
          m_phase_function(pro::make_proxy_shared<Material, Isotropic>(albedo)) {}


    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        HitRecord hit_rec1, hit_rec2;

        if (!m_boundary->hit(ray, Interval::universe, hit_rec1)) {
            return false;
        }

        if (!m_boundary->hit(ray, Interval {hit_rec1.t + 0.0001, infinity}, hit_rec2)) {
            return false;
        }

        hit_rec1.t = std::max(hit_rec1.t, ray_t.min);
        hit_rec2.t = std::min(hit_rec2.t, ray_t.max);
        if (hit_rec1.t >= hit_rec2.t) {
            return false;
        }
        hit_rec1.t = std::max(hit_rec1.t, 0.0);

        const double ray_length = ray.direction().norm();
        const double distance_inside_boundary = (hit_rec2.t - hit_rec1.t) * ray_length;
        const double hit_distance = m_neg_inv_density * std::log(random_double());

        if (hit_distance > distance_inside_boundary) {
            return false;
        }

        hit_rec.t = hit_rec1.t + hit_distance / ray_length;
        hit_rec.p = ray.at(hit_rec.t);

        hit_rec.normal = Vec3d {1.0, 0.0, 0.0}; // arbitrary
        hit_rec.front_face = true;              // also arbitrary
        hit_rec.mat = m_phase_function;

        return true;
    }

    AABB bounding_box() const { return m_boundary->bounding_box(); }

    double pdf_value(const Vec3d& origin, const Vec3d& direction) const { return 0.0; }

    Vec3d random(const Vec3d& origin) const { return {1.0, 0.0, 0.0}; }

    pro::proxy<Hittable> m_boundary;
    double m_neg_inv_density;
    pro::proxy<Material> m_phase_function;
};
