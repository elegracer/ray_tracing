#include "scene/scene_definition.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace {

void expect_true(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct OwnedSceneFixture {
    rt::scene::SceneDefinition scene;
    const char* metadata_id_data = nullptr;
    const char* metadata_label_data = nullptr;
    const char* preset_scene_id_data = nullptr;
    const char* preset_id_data = nullptr;
};

OwnedSceneFixture make_scene_definition() {
    std::string metadata_id = "dynamic-demo";
    std::string metadata_label = "Dynamic Demo";
    std::string preset_scene_id = "dynamic-demo";
    std::string preset_id = "preview";

    OwnedSceneFixture fixture;
    fixture.metadata_id_data = metadata_id.data();
    fixture.metadata_label_data = metadata_label.data();
    fixture.preset_scene_id_data = preset_scene_id.data();
    fixture.preset_id_data = preset_id.data();

    fixture.scene.metadata.id = metadata_id;
    fixture.scene.metadata.label = metadata_label;
    fixture.scene.metadata.supports_cpu_render = true;
    fixture.scene.metadata.supports_realtime = true;

    rt::scene::SceneDefinitionCpuRenderPreset preset;
    preset.scene_id = preset_scene_id;
    preset.preset_id = preset_id;
    fixture.scene.cpu_presets.push_back(std::move(preset));
    fixture.scene.dependencies.push_back("assets/scenes/demo/scene.yaml");
    return fixture;
}

}  // namespace

int main() {
    const OwnedSceneFixture fixture = make_scene_definition();
    const rt::scene::SceneDefinition& scene = fixture.scene;

    expect_true(scene.metadata.id == "dynamic-demo", "scene id");
    expect_true(scene.metadata.label == "Dynamic Demo", "scene label");
    expect_true(scene.metadata.id.data() != fixture.metadata_id_data, "scene id owns its storage");
    expect_true(scene.metadata.label.data() != fixture.metadata_label_data, "scene label owns its storage");
    expect_true(scene.cpu_presets.size() == 1, "cpu preset recorded");
    expect_true(scene.cpu_presets.front().scene_id == "dynamic-demo", "cpu preset scene id");
    expect_true(scene.cpu_presets.front().preset_id == "preview", "cpu preset id");
    expect_true(scene.cpu_presets.front().scene_id.data() != fixture.preset_scene_id_data,
                "cpu preset scene id owns its storage");
    expect_true(scene.cpu_presets.front().preset_id.data() != fixture.preset_id_data,
                "cpu preset id owns its storage");
    expect_true(scene.realtime_preset.has_value() == false, "no realtime preset by default");
    expect_true(scene.dependencies.size() == 1, "dependency recorded");
    return 0;
}
