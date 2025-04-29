#include <fmt/core.h>
#include <fmt/ostream.h>

#include "common/common.h"

struct Sample {
    double x;
    double p_x;
};

int main(int argc, const char* argv[]) {

    constexpr int sample_count_sqrt = 1000;
    constexpr int sample_count = sample_count_sqrt * sample_count_sqrt;
    std::vector<Sample> samples(sample_count);
    double sum = 0.0;

    for (int i = 0; i < sample_count; ++i) {
        const double x = random_double(0.0, 2.0 * pi);
        const double sin_x = std::sin(x);
        const double p_x = std::exp(-x / (2.0 * pi)) * sin_x * sin_x;
        sum += p_x;

        samples[i].x = x;
        samples[i].p_x = p_x;
    }

    std::sort(samples.begin(), samples.end(),
        [](const auto& a, const auto& b) { return a.x < b.x; });

    const double half_sum = 0.5 * sum;
    double halfway_point = 0.0;
    double accum = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        accum += samples[i].p_x;
        if (accum >= half_sum) {
            halfway_point = samples[i].x;
            break;
        }
    }

    fmt::print("Average = {:.12f}\n", sum / sample_count);
    fmt::print("Area under curve = {:.12f}\n", 2.0 * pi * sum / sample_count);
    fmt::print("Halfway = {:.12f}\n", halfway_point);

    return EXIT_SUCCESS;
}
