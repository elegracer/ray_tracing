#pragma once

#include "proxy/proxy.h"

#include "ray.h"
#include "interval.h"
#include "material.h"
#include "aabb.h"

PRO_DEF_MEM_DISPATCH(MemHit, hit);
PRO_DEF_MEM_DISPATCH(MemBB, bounding_box);

struct Hittable                                         //
    : pro::facade_builder                               //
      ::support_copy<pro::constraint_level::nontrivial> //
      ::add_convention<MemHit,
          bool(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const> //
      ::add_convention<MemBB, AABB() const>                                      //
      ::build {};
