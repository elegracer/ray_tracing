#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

int main(int argc, const char* argv[]) {

    constexpr int sample_count = 1000000;
    int inside_circle_count = 0;

    for (int i = 0; i < sample_count; ++i) {
        const double x = random_double(-1.0, 1.0);
        const double y = random_double(-1.0, 1.0);
        if (x * x + y * y < 1.0) {
            ++inside_circle_count;
        }
    }

    fmt::print("Estimate of Pi: {:.12f}\n", (4.0 * inside_circle_count) / sample_count);

    return EXIT_SUCCESS;
}
