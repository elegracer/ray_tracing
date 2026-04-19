#include "realtime/display_transfer.h"
#include "test_support.h"

int main() {
    expect_near(rt::linear_to_display(0.0f), 0.0, 1e-12, "zero stays black");
    expect_near(rt::linear_to_display(0.04f), 0.2, 1e-6, "display transform applies gamma-2");
    expect_near(rt::linear_to_display(-1.0f), 0.0, 1e-12, "negative radiance clamps to black");
    expect_near(rt::linear_to_display(4.0f), 0.999, 1e-6, "bright radiance clamps after display transform");

    expect_true(rt::linear_to_display_u8(0.04f) == 51, "u8 conversion uses display-transformed brightness");
    expect_true(rt::linear_to_display_u8(4.0f) == 255, "u8 conversion saturates near white");
    return 0;
}
