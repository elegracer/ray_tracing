#include "realtime/viewer/scene_switch_controller.h"
#include "test_support.h"

int main() {
    rt::viewer::SceneSwitchController controller("final_room");
    expect_true(controller.current_scene_id() == "final_room", "initial scene");

    controller.request_scene("cornell_box");
    const rt::viewer::SceneSwitchResult unsupported = controller.resolve_pending();
    expect_true(!unsupported.applied, "unsupported scene not applied");
    expect_true(controller.current_scene_id() == "final_room", "unsupported keeps old scene");
    expect_true(!unsupported.error_message.empty(), "unsupported error");

    controller.request_scene("smoke");
    const rt::viewer::SceneSwitchResult applied = controller.resolve_pending();
    expect_true(applied.applied, "smoke applied");
    expect_true(controller.current_scene_id() == "smoke", "current scene updated");
    expect_true(applied.reset_pose, "supported switch resets pose");
    return 0;
}
