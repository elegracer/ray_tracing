#pragma once

#include "defs.h"
#include "hittable.h"

#include <vector>

class HittableList {
public:
    HittableList() = default;
    HittableList(pro::proxy<Hittable> object) { add(object); }

    void clear() { m_objects.clear(); }

    void add(pro::proxy<Hittable> object) { m_objects.push_back(object); }

    bool hit(const Ray& ray, const double ray_tmin, const double ray_tmax,
        HitRecord& hit_rec) const {
        HitRecord temp_hit_rec;
        bool hit_anything = false;
        double closest_so_far = ray_tmax;

        for (const auto& object : m_objects) {
            if (object->hit(ray, ray_tmin, closest_so_far, temp_hit_rec)) {
                hit_anything = true;
                closest_so_far = temp_hit_rec.t;
                hit_rec = temp_hit_rec;
            }
        }

        return hit_anything;
    }

private:
    std::vector<pro::proxy<Hittable>> m_objects;
};
