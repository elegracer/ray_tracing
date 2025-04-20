#pragma once

#include "common.h"


class Interval {

public:
    Interval() = default;
    Interval(const double min, const double max) : min(min), max(max) {}
    Interval(const Interval& a, const Interval& b) {
        // Create the interval tightly enclosing the two input intervals.
        min = std::min(a.min, b.min);
        max = std::max(a.max, b.max);
    }

    double size() const { return max - min; }

    bool contains(const double x) const { return min <= x && x <= max; }

    bool surrounds(const double x) const { return min < x && x < max; }

    double clamp(const double x) const { return std::clamp(x, min, max); }

    Interval expand(const double delta) const {
        const double padding = 0.5 * delta;
        return {min - padding, max + padding};
    }

    double min = +infinity;
    double max = -infinity;

    static const Interval empty;
    static const Interval universe;
};

inline const Interval Interval::empty = Interval {+infinity, -infinity};
inline const Interval Interval::universe = Interval {-infinity, +infinity};
