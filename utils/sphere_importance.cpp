#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

inline double f(const Vec3d& d) {
    return d.z() * d.z();
}

inline double pdf(const Vec3d& d) {
    return 1.0 / (4.0 * pi);
}

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const Vec3d d = random_unit_vector();
        const double f_d = f(d);
        sum += f_d / pdf(d);
    }

    fmt::print("I = {:.12f}\n", sum / sample_count);

    return EXIT_SUCCESS;
}
