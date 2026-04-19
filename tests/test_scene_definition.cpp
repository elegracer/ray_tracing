#include "scene/scene_definition.h"

#include <stdexcept>

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    rt::scene::SceneDefinition scene;
    scene.metadata.id = "demo";
    scene.metadata.label = "Demo";
    scene.metadata.supports_cpu_render = true;
    scene.metadata.supports_realtime = true;
    scene.dependencies.push_back("assets/scenes/demo/scene.yaml");

    expect_true(scene.metadata.id == "demo", "scene id");
    expect_true(scene.cpu_presets.empty(), "cpu presets start empty");
    expect_true(scene.realtime_preset.has_value() == false, "no realtime preset by default");
    expect_true(scene.dependencies.size() == 1, "dependency recorded");
    return 0;
}
