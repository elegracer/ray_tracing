#pragma once

#include "common/defs.h"


class Interval {

public:
    Interval() = default;
    Interval(const double min, const double max) : min(min), max(max) {}

    double size() const { return max - min; }

    bool contains(const double x) const { return min <= x && x <= max; }

    bool surrounds(const double x) const { return min < x && x < max; }

    double min = +infinity;
    double max = -infinity;

    static const Interval empty;
    static const Interval universe;
};

inline const Interval Interval::empty = Interval {+infinity, -infinity};
inline const Interval Interval::universe = Interval {-infinity, +infinity};
