#pragma once

#include "proxy/proxy.h"

#include "defs.h"
#include "ray.h"
#include "interval.h"


struct HitRecord {
    Vec3d p;
    Vec3d normal;
    double t;
    bool front_face;

    void set_face_normal(const Ray& ray, const Vec3d& outward_normal) {
        // Sets the hit record normal vector
        // NOTE: the parameter `outward_normal` is assumed to have unit length
        front_face = ray.direction().dot(outward_normal) < 0.0;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

PRO_DEF_MEM_DISPATCH(MemHit, hit);

struct Hittable
    : pro::facade_builder ::support_copy<pro::constraint_level::nontrivial>::add_convention<MemHit,
          bool(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const>::build {};
