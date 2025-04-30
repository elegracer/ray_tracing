#pragma once

#include "proxy/proxy.h"

#include "ray.h"
#include "interval.h"
#include "material.h"
#include "aabb.h"

PRO_DEF_MEM_DISPATCH(HittableMemHit, hit);
PRO_DEF_MEM_DISPATCH(HittableMemBB, bounding_box);

struct Hittable                                         //
    : pro::facade_builder                               //
      ::support_copy<pro::constraint_level::nontrivial> //
      ::add_convention<HittableMemHit,
          bool(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const> //
      ::add_convention<HittableMemBB, AABB() const>                              //
      ::build {};


struct Translate {
    Translate(const pro::proxy<Hittable>& object, const Vec3d& offset)
        : m_object(object),
          m_offset(offset) {
        m_bbox = m_object->bounding_box() + m_offset;
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        // Move the ray backwards by the offset
        Ray offset_r {ray.origin() - m_offset, ray.direction(), ray.time()};

        // Determine whether an intersection exists along the offset ray (and if so, where)
        if (!m_object->hit(offset_r, ray_t, hit_rec)) {
            return false;
        }

        // Move the intersection point forwards by the offset
        hit_rec.p += m_offset;

        return true;
    }

    AABB bounding_box() const { return m_bbox; }

    pro::proxy<Hittable> m_object;
    Vec3d m_offset;
    AABB m_bbox;
};


struct RotateY {
    RotateY(const pro::proxy<Hittable>& object, const double angle) : m_object(object) {
        const double radians = deg2rad(angle);
        m_sin_theta = std::sin(radians);
        m_cos_theta = std::cos(radians);
        m_bbox = m_object->bounding_box();

        Vec3d min {infinity, infinity, infinity};
        Vec3d max {-infinity, -infinity, -infinity};

        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                for (int k = 0; k < 2; ++k) {
                    const double x = i * m_bbox.x.max + (1 - i) * m_bbox.x.min;
                    const double y = j * m_bbox.y.max + (1 - j) * m_bbox.y.min;
                    const double z = k * m_bbox.z.max + (1 - k) * m_bbox.z.min;

                    const double newx = m_cos_theta * x + m_sin_theta * z;
                    const double newz = -m_sin_theta * x + m_cos_theta * z;

                    const Vec3d tester {newx, y, newz};

                    min = min.array().min(tester.array()).matrix();
                    max = max.array().max(tester.array()).matrix();
                }
            }
        }

        m_bbox = AABB {min, max};
    }


    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        // Transform the ray from world space to object space

        const Vec3d origin {                                                     //
            (m_cos_theta * ray.origin().x()) - (m_sin_theta * ray.origin().z()), //
            ray.origin().y(),                                                    //
            (m_sin_theta * ray.origin().x()) + (m_cos_theta * ray.origin().z())};
        const Vec3d direction {                                                        //
            (m_cos_theta * ray.direction().x()) - (m_sin_theta * ray.direction().z()), //
            ray.direction().y(),                                                       //
            (m_sin_theta * ray.direction().x()) + (m_cos_theta * ray.direction().z())};

        const Ray rotated_ray {origin, direction, ray.time()};

        // Determine whether an intersection exists in object space (and if so, where)

        if (!m_object->hit(rotated_ray, ray_t, hit_rec)) {
            return false;
        }

        // Transform the intersection from object space back to world space

        hit_rec.p = Vec3d {                                                //
            (m_cos_theta * hit_rec.p.x()) + (m_sin_theta * hit_rec.p.z()), //
            hit_rec.p.y(),                                                 //
            (-m_sin_theta * hit_rec.p.x()) + (m_cos_theta * hit_rec.p.z())};
        hit_rec.normal = Vec3d {                                                     //
            (m_cos_theta * hit_rec.normal.x()) + (m_sin_theta * hit_rec.normal.z()), //
            hit_rec.normal.y(),                                                      //
            (-m_sin_theta * hit_rec.normal.x()) + (m_cos_theta * hit_rec.normal.z())};

        return true;
    }

    AABB bounding_box() const { return m_bbox; }

    pro::proxy<Hittable> m_object;
    double m_cos_theta;
    double m_sin_theta;
    AABB m_bbox;
};
