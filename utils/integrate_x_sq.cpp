#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    int a = 0;
    int b = 2;
    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const double x = random_double(a, b);
        sum += x * x;
    }

    fmt::print("I = {:.12f}\n", (b - a) * (sum / sample_count));

    return EXIT_SUCCESS;
}
