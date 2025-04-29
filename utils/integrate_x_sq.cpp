#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

inline double icd(const double d) {
    return 2.0 * d;
}

inline double pdf(const double x) {
    return 0.5;
}

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const double x = icd(random_double());
        sum += x * x / pdf(x);
    }

    fmt::print("I = {:.12f}\n", sum / sample_count);

    return EXIT_SUCCESS;
}
