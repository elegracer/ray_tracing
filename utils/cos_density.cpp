#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

inline double f(const Vec3d& d) {
    const double cos_theta = d.z();
    return cos_theta * cos_theta * cos_theta;
}

inline double pdf(const Vec3d& d) {
    return d.z() / pi;
}

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const Vec3d d = random_cosine_direction();
        sum += f(d) / pdf(d);
    }

    fmt::print("Pi/2 = {:.12f}\n", 0.5 * pi);
    fmt::print("Estimate = {:.12f}\n", sum / sample_count);

    return EXIT_SUCCESS;
}
