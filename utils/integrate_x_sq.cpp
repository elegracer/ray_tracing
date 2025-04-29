#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

inline double icd(const double d) {
    return 8.0 * std::pow(d, 1.0 / 3.0);
}

inline double pdf(const double x) {
    return (3.0 / 8.0) * x * x;
}

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const double z = random_double();
        if (z < 1e-12) {
            continue;
        }

        const double x = icd(z);
        sum += x * x / pdf(x);
    }

    fmt::print("I = {:.12f}\n", sum / sample_count);

    return EXIT_SUCCESS;
}
