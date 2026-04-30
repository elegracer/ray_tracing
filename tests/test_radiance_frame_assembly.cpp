#include "realtime/gpu/radiance_frame_assembly.h"
#include "test_support.h"

int main() {
    const float4 beauty[] = {
        {0.04f, 0.16f, 0.36f, 1.0f},
        {-1.0f, 4.0f, 100.0f, 1.0f},
    };
    const float4 normal[] = {
        {0.1f, 0.2f, 0.3f, 1.0f},
        {0.4f, 0.5f, 0.6f, 1.0f},
    };
    const float4 albedo[] = {
        {0.7f, 0.8f, 0.9f, 1.0f},
        {1.0f, 0.9f, 0.8f, 1.0f},
    };
    const float depth[] = {1.25f, 2.5f};

    const std::vector<float> unpacked = rt::unpack_float4_rgba(beauty, 2);
    expect_true(unpacked.size() == 8, "unpacked size");
    expect_near(unpacked[0], 0.04, 1e-6, "unpacked first r");
    expect_near(unpacked[6], 100.0, 1e-6, "unpacked second b");

    const double expected_luminance =
        ((0.2 + 0.4 + 0.6) / 3.0 + (0.0 + 0.999 + 0.999) / 3.0) / 2.0;
    expect_near(rt::compute_frame_average_luminance(unpacked), expected_luminance, 1e-6, "luminance");
    expect_near(rt::compute_frame_average_luminance({}), 0.0, 1e-12, "empty luminance");

    const rt::RadianceFrame frame = rt::make_radiance_frame(2, 1, beauty, normal, albedo, depth);
    expect_true(frame.width == 2, "frame width");
    expect_true(frame.height == 1, "frame height");
    expect_true(frame.beauty_rgba.size() == 8, "beauty size");
    expect_true(frame.normal_rgba.size() == 8, "normal size");
    expect_true(frame.albedo_rgba.size() == 8, "albedo size");
    expect_true(frame.depth.size() == 2, "depth size");
    expect_near(frame.beauty_rgba[2], 0.36, 1e-6, "beauty b");
    expect_near(frame.normal_rgba[4], 0.4, 1e-6, "normal second r");
    expect_near(frame.albedo_rgba[5], 0.9, 1e-6, "albedo second g");
    expect_near(frame.depth[1], 2.5, 1e-6, "depth second");
    expect_near(frame.average_luminance, expected_luminance, 1e-6, "frame luminance");

    const rt::RadianceFrame empty = rt::make_empty_radiance_frame(3, 4);
    expect_true(empty.width == 3, "empty width");
    expect_true(empty.height == 4, "empty height");
    expect_true(empty.beauty_rgba.empty(), "empty beauty");
    expect_true(empty.average_luminance == 0.0, "empty avg");

    return 0;
}
