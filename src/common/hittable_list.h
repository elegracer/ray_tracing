#pragma once

#include "traits.h"

#include "material.h"
#include "interval.h"
#include "aabb.h"

class HittableList {
public:
    HittableList() = default;
    explicit HittableList(pro::proxy<Hittable> object) { add(object); }

    void clear() { m_objects.clear(); }

    void add(pro::proxy<Hittable> object) {
        m_objects.push_back(object);
        m_bbox = AABB {m_bbox, object->bounding_box()};
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        HitRecord temp_hit_rec;
        bool hit_anything = false;
        double closest_so_far = ray_t.max;

        for (const auto& object : m_objects) {
            if (object->hit(ray, Interval {ray_t.min, closest_so_far}, temp_hit_rec)) {
                hit_anything = true;
                closest_so_far = temp_hit_rec.t;
                hit_rec = temp_hit_rec;
            }
        }

        return hit_anything;
    }

    AABB bounding_box() const { return m_bbox; }

    double pdf_value(const Vec3d& origin, const Vec3d& direction) const { return 0.0; }

    Vec3d random(const Vec3d& origin) const { return {1.0, 0.0, 0.0}; }

    std::vector<pro::proxy<Hittable>> m_objects;

private:
    AABB m_bbox;
};
