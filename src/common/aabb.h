#pragma once

#include "common/interval.h"
#include "common/ray.h"

struct AABB {

    Interval x;
    Interval y;
    Interval z;

    // The default AABB is empty, since intervals are empty by default
    AABB() = default;

    AABB(const Interval& x, const Interval& y, const Interval& z) : x(x), y(y), z(z) {}

    AABB(const Vec3d& a, const Vec3d& b) {
        // Treat the two point a and b for extrema for the bounding box,
        // so we don't require a particular minimum/maximum coordinate order.
        x = (a.x() <= b.x()) ? Interval {a.x(), b.x()} : Interval {b.x(), a.x()};
        y = (a.y() <= b.y()) ? Interval {a.y(), b.y()} : Interval {b.y(), a.y()};
        z = (a.z() <= b.z()) ? Interval {a.z(), b.z()} : Interval {b.z(), a.z()};
    }

    AABB(const AABB& box0, const AABB& box1) {
        x = Interval {box0.x, box1.x};
        y = Interval {box0.y, box1.y};
        z = Interval {box0.z, box1.z};
    }

    const Interval& axis_interval(const int n) const {
        switch (n) {
            case 1: return y;
            case 2: return z;
            default: return x;
        }
    }

    bool hit(const Ray& ray, Interval ray_t) const {
        const Vec3d ray_origin = ray.origin();
        const Vec3d ray_direction = ray.direction();

        for (int axis = 0; axis < 3; ++axis) {
            const Interval& ax = axis_interval(axis);
            const double adinv = 1.0 / ray_direction[axis];

            const double t0 = (ax.min - ray_origin[axis]) * adinv;
            const double t1 = (ax.max - ray_origin[axis]) * adinv;

            const Interval t = (t0 < t1) ? Interval {t0, t1} : Interval {t1, t0};

            ray_t.min = std::max(ray_t.min, t.min);
            ray_t.max = std::min(ray_t.max, t.max);
            if (ray_t.max <= ray_t.min) {
                return false;
            }
        }
        return true;
    }
};
