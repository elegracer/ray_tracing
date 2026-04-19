#include "realtime/viewer/move_speed.h"
#include "test_support.h"

int main() {
    expect_near(rt::viewer::clamp_move_speed(0.0001), 0.05, 1e-12, "move speed clamps to minimum");
    expect_near(rt::viewer::clamp_move_speed(100000.0), 5000.0, 1e-12, "move speed clamps to maximum");

    const double increased = rt::viewer::apply_scroll_speed_step(2.0, 1.0);
    const double decreased = rt::viewer::apply_scroll_speed_step(2.0, -1.0);
    expect_true(increased > 2.0, "scroll up increases move speed");
    expect_true(decreased < 2.0, "scroll down decreases move speed");

    rt::viewer::MoveSpeedState state(3.5);
    expect_near(state.current_speed(), 3.5, 1e-12, "state starts at scene default speed");
    state.apply_scroll(2.0);
    expect_true(state.current_speed() > state.default_speed(), "scroll state tracks faster speed");
    state.reset(1.25);
    expect_near(state.current_speed(), 1.25, 1e-12, "scene switch resets current speed");
    expect_near(state.default_speed(), 1.25, 1e-12, "scene switch updates default speed");
    return 0;
}
