#include "realtime/gpu/host_radiance_staging.h"
#include "test_support.h"

int main() {
    rt::HostRadianceStagingPool pool;

    rt::HostRadianceStaging& first = pool.buffer_for(2, 1);
    expect_true(first.width == 2, "first width");
    expect_true(first.height == 1, "first height");
    expect_true(first.beauty != nullptr, "first beauty allocated");
    expect_true(first.normal != nullptr, "first normal allocated");
    expect_true(first.albedo != nullptr, "first albedo allocated");
    expect_true(first.depth != nullptr, "first depth allocated");

    first.beauty[0] = {0.04f, 0.16f, 0.36f, 1.0f};
    first.beauty[1] = {1.0f, 1.0f, 1.0f, 1.0f};
    first.normal[0] = {0.1f, 0.2f, 0.3f, 1.0f};
    first.normal[1] = {0.4f, 0.5f, 0.6f, 1.0f};
    first.albedo[0] = {0.7f, 0.8f, 0.9f, 1.0f};
    first.albedo[1] = {1.0f, 0.9f, 0.8f, 1.0f};
    first.depth[0] = 1.25f;
    first.depth[1] = 2.5f;

    const rt::RadianceFrame frame = rt::make_radiance_frame(first);
    expect_true(frame.width == 2, "frame width");
    expect_true(frame.height == 1, "frame height");
    expect_near(frame.beauty_rgba[2], 0.36, 1e-6, "beauty b");
    expect_near(frame.normal_rgba[4], 0.4, 1e-6, "normal second r");
    expect_near(frame.albedo_rgba[5], 0.9, 1e-6, "albedo second g");
    expect_near(frame.depth[1], 2.5, 1e-6, "depth second");

    rt::HostRadianceStaging& reused = pool.buffer_for(2, 1);
    expect_true(reused.beauty == first.beauty, "same size reuses beauty staging");

    rt::HostRadianceStaging& second = pool.buffer_for(3, 1);
    expect_true(second.width == 3, "second width");
    expect_true(second.beauty != nullptr, "second beauty allocated");
    expect_true(second.beauty != first.beauty, "different size gets distinct staging");

    pool.reset();
    rt::HostRadianceStaging& after_reset = pool.buffer_for(2, 1);
    expect_true(after_reset.beauty != nullptr, "after reset allocates beauty");

    return 0;
}
