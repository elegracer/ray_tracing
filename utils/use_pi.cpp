#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

int main(int argc, const char* argv[]) {

    constexpr int sample_count = 1000000;

    int inside_circle_count = 0;
    int runs = 0;

    while (true) {
        ++runs;
        const double x = random_double(-1.0, 1.0);
        const double y = random_double(-1.0, 1.0);
        if (x * x + y * y < 1.0) {
            ++inside_circle_count;
        }

        if (runs % sample_count == 0) {
            fmt::print("Estimate of Pi: {:.12f}\n", 4.0 * inside_circle_count / runs);
        }
    }

    return EXIT_SUCCESS;
}
