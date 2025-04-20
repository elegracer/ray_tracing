#pragma once

#include "hittable_list.h"

struct BVHNode {
    BVHNode(HittableList list) : BVHNode(list.m_objects, 0, list.m_objects.size()) {
        // There's a C++ subtlety here. This constructor (without span indices) creates an
        // implicit copy of the hittable list, which we will modify. The lifetime of the copied
        // list only extends until this constructor exits. That's OK, because we only need to
        // persist the resulting bounding volume hierarchy.
    }

    BVHNode(std::vector<pro::proxy<Hittable>>& objects, const size_t start, const size_t end) {
        const int axis = random_int(0, 2);

        const auto comparator = (axis == 0)   ? box_x_compare
                                : (axis == 1) ? box_y_compare
                                              : box_z_compare;

        const size_t object_span = end - start;

        if (object_span == 1) {
            m_left = m_right = objects[start];
        } else if (object_span == 2) {
            m_left = objects[start];
            m_right = objects[start + 1];
        } else {
            std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);

            const size_t mid = start + object_span / 2;
            m_left = pro::make_proxy_shared<Hittable, BVHNode>(objects, start, mid);
            m_right = pro::make_proxy_shared<Hittable, BVHNode>(objects, mid, end);
        }

        m_bbox = AABB {m_left->bounding_box(), m_right->bounding_box()};
    }

    bool hit(const Ray& ray, const Interval& ray_t, HitRecord& hit_rec) const {
        if (!m_bbox.hit(ray, ray_t)) {
            return false;
        }

        const bool hit_left = m_left->hit(ray, ray_t, hit_rec);
        const bool hit_right =
            m_right->hit(ray, Interval {ray_t.min, hit_left ? hit_rec.t : ray_t.max}, hit_rec);

        return hit_left || hit_right;
    }

    AABB bounding_box() const { return m_bbox; }

    pro::proxy<Hittable> m_left;
    pro::proxy<Hittable> m_right;
    AABB m_bbox;


    static bool box_compare(const pro::proxy<Hittable>& a, const pro::proxy<Hittable>& b,
        const int axis_index) {
        return a->bounding_box().axis_interval(axis_index).min
               < b->bounding_box().axis_interval(axis_index).min;
    }

    static bool box_x_compare(const pro::proxy<Hittable>& a, const pro::proxy<Hittable>& b) {
        return box_compare(a, b, 0);
    }

    static bool box_y_compare(const pro::proxy<Hittable>& a, const pro::proxy<Hittable>& b) {
        return box_compare(a, b, 1);
    }

    static bool box_z_compare(const pro::proxy<Hittable>& a, const pro::proxy<Hittable>& b) {
        return box_compare(a, b, 2);
    }
};
