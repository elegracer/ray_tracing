#include "realtime/gpu/device_frame_buffers.h"
#include "test_support.h"

int main() {
    rt::DeviceFrameBufferSet buffers;

    buffers.resize_frame(4, 3);
    const rt::DeviceFrameBuffers first_frame = buffers.frame();
    expect_true(first_frame.beauty != nullptr, "frame beauty allocated");
    expect_true(first_frame.normal != nullptr, "frame normal allocated");
    expect_true(first_frame.albedo != nullptr, "frame albedo allocated");
    expect_true(first_frame.depth != nullptr, "frame depth allocated");
    expect_true(buffers.frame_width() == 4, "frame width");
    expect_true(buffers.frame_height() == 3, "frame height");

    buffers.resize_frame(4, 3);
    expect_true(buffers.frame().beauty == first_frame.beauty, "same frame size reuses allocation");

    buffers.resize_frame(2, 2);
    expect_true(buffers.frame().beauty != nullptr, "resized frame beauty allocated");
    expect_true(buffers.frame_width() == 2, "resized frame width");
    expect_true(buffers.frame_height() == 2, "resized frame height");

    buffers.resize_history(2, 2);
    expect_true(buffers.history().beauty != nullptr, "history beauty allocated");
    expect_true(buffers.history_width() == 2, "history width");
    expect_true(buffers.history_height() == 2, "history height");
    expect_true(buffers.history_state().history_length == 0, "new history length reset");

    rt::LaunchHistoryState state = buffers.history_state();
    state.history_length = 5;
    state.prev_origin[0] = 1.0;
    state.prev_basis_x[0] = 2.0;
    buffers.apply_history_state(state);
    expect_true(buffers.history_state().history_length == 5, "history length applied");
    expect_near(buffers.history_state().prev_origin[0], 1.0, 1e-12, "prev origin applied");

    buffers.reset_history();
    expect_true(buffers.history().beauty == nullptr, "history reset clears beauty");
    expect_true(buffers.history_state().history_length == 0, "history reset clears length");

    buffers.reset_frame();
    expect_true(buffers.frame().beauty == nullptr, "frame reset clears beauty");
    expect_true(buffers.frame_width() == 0, "frame reset clears width");

    return 0;
}
