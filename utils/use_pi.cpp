#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;

    int inside_circle_count = 0;
    int inside_circle_count_stratified = 0;
    int runs = 0;

    for (int i = 0; i < sample_count_sqrt; ++i) {
        for (int j = 0; j < sample_count_sqrt; ++j) {
            {

                const double x = random_double(-1.0, 1.0);
                const double y = random_double(-1.0, 1.0);
                if (x * x + y * y < 1.0) {
                    ++inside_circle_count;
                }
            }
            {
                const double x = 2.0 * ((i + random_double()) / sample_count_sqrt) - 1.0;
                const double y = 2.0 * ((j + random_double()) / sample_count_sqrt) - 1.0;
                if (x * x + y * y < 1.0) {
                    ++inside_circle_count_stratified;
                }
            }
        }
    }

    fmt::print("Regular    Estimate of Pi: {:.12f}\n", //
        4.0 * inside_circle_count / sample_count);
    fmt::print("Stratified Estimate of Pi: {:.12f}\n", //
        4.0 * inside_circle_count_stratified / sample_count);

    return EXIT_SUCCESS;
}
