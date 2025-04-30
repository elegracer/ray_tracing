#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

inline double f(const double r2) {
    const double z = 1 - r2;
    const double cos_theta = z;
    return cos_theta * cos_theta * cos_theta;
}

inline double pdf() {
    return 1.0 / (2.0 * pi);
}

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const double r2 = random_double();
        sum += f(r2) / pdf();
    }

    fmt::print("Pi/2 = {:.12f}\n", 0.5 * pi);
    fmt::print("Estimate = {:.12f}\n", sum / sample_count);

    return EXIT_SUCCESS;
}
