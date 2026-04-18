# Viewer Scene Selection And ImGui Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one shared scene catalog for `render_scene`, `render_realtime`, and `render_realtime_viewer`, then add viewer startup/runtime scene switching with an ImGui control panel.

**Architecture:** Keep scene metadata and backend-specific scene builders separate. A new shared catalog owns scene ids, labels, and capability flags; CPU rendering and realtime scene construction stay in separate helpers. The viewer keeps its existing GLFW/OpenGL presentation path and adds ImGui only for controls plus a small scene-switch controller that can be tested without a GUI.

**Tech Stack:** C++23, CMake, vcpkg, Eigen, OpenCV, GLFW, OpenGL compatibility context, ImGui with GLFW + OpenGL3 backend, existing custom test executables + CTest

---

### Task 1: Add A Shared Scene Catalog

**Files:**
- Create: `src/realtime/scene_catalog.h`
- Create: `src/realtime/scene_catalog.cpp`
- Create: `tests/test_scene_catalog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "realtime/scene_catalog.h"
#include "test_support.h"

#include <array>
#include <string_view>

int main() {
    constexpr std::array<std::string_view, 16> expected_ids {
        "bouncing_spheres",
        "checkered_spheres",
        "earth_sphere",
        "perlin_spheres",
        "quads",
        "simple_light",
        "cornell_smoke",
        "cornell_smoke_extreme",
        "cornell_box",
        "cornell_box_extreme",
        "cornell_box_and_sphere",
        "cornell_box_and_sphere_extreme",
        "rttnw_final_scene",
        "rttnw_final_scene_extreme",
        "smoke",
        "final_room",
    };

    const auto& scenes = rt::scene_catalog();
    expect_true(scenes.size() == expected_ids.size(), "scene count");

    for (std::string_view id : expected_ids) {
        const rt::SceneCatalogEntry* entry = rt::find_scene_catalog_entry(id);
        expect_true(entry != nullptr, "catalog entry exists");
        expect_true(entry->id == id, "catalog id matches");
        expect_true(!entry->label.empty(), "catalog label non-empty");
    }

    expect_true(rt::find_scene_catalog_entry("final_room")->supports_realtime, "final_room realtime");
    expect_true(!rt::find_scene_catalog_entry("cornell_box")->supports_realtime, "cornell_box not realtime yet");
    expect_true(rt::find_scene_catalog_entry("quads")->supports_cpu_render, "quads cpu");
    expect_true(!rt::find_scene_catalog_entry("smoke")->supports_cpu_render, "smoke realtime-only");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_scene_catalog`
Expected: FAIL because `src/realtime/scene_catalog.h` and `tests/test_scene_catalog.cpp` are not wired yet.

- [ ] **Step 3: Write minimal implementation**

`src/realtime/scene_catalog.h`

```cpp
#pragma once

#include <string_view>
#include <vector>

namespace rt {

struct SceneCatalogEntry {
    std::string_view id;
    std::string_view label;
    bool supports_cpu_render = false;
    bool supports_realtime = false;
};

const std::vector<SceneCatalogEntry>& scene_catalog();
const SceneCatalogEntry* find_scene_catalog_entry(std::string_view id);

}  // namespace rt
```

`src/realtime/scene_catalog.cpp`

```cpp
#include "realtime/scene_catalog.h"

namespace rt {
namespace {

const std::vector<SceneCatalogEntry> kSceneCatalog {
    {"bouncing_spheres", "Bouncing Spheres", true, false},
    {"checkered_spheres", "Checkered Spheres", true, false},
    {"earth_sphere", "Earth Sphere", true, false},
    {"perlin_spheres", "Perlin Spheres", true, false},
    {"quads", "Quads", true, false},
    {"simple_light", "Simple Light", true, false},
    {"cornell_smoke", "Cornell Smoke", true, false},
    {"cornell_smoke_extreme", "Cornell Smoke Extreme", true, false},
    {"cornell_box", "Cornell Box", true, false},
    {"cornell_box_extreme", "Cornell Box Extreme", true, false},
    {"cornell_box_and_sphere", "Cornell Box And Sphere", true, false},
    {"cornell_box_and_sphere_extreme", "Cornell Box And Sphere Extreme", true, false},
    {"rttnw_final_scene", "RTTNW Final Scene", true, false},
    {"rttnw_final_scene_extreme", "RTTNW Final Scene Extreme", true, false},
    {"smoke", "Realtime Smoke", false, true},
    {"final_room", "Final Room", false, true},
};

}  // namespace

const std::vector<SceneCatalogEntry>& scene_catalog() {
    return kSceneCatalog;
}

const SceneCatalogEntry* find_scene_catalog_entry(std::string_view id) {
    for (const SceneCatalogEntry& entry : kSceneCatalog) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace rt
```

`CMakeLists.txt`

```cmake
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/scene_catalog.cpp
)

add_executable(test_scene_catalog)
target_sources(test_scene_catalog
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_catalog.cpp
)
target_link_libraries(test_scene_catalog PRIVATE core)
add_test(NAME test_scene_catalog COMMAND test_scene_catalog)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_scene_catalog && ctest --test-dir build -R test_scene_catalog --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/scene_catalog.h src/realtime/scene_catalog.cpp tests/test_scene_catalog.cpp
git commit -m "feat: add shared scene catalog"
```

### Task 2: Extract Offline CPU Scene Dispatch Out Of `render_scene.cpp`

**Files:**
- Create: `src/core/offline_scene_renderer.h`
- Create: `src/core/offline_scene_renderer.cpp`
- Create: `tests/test_offline_scene_renderer.cpp`
- Modify: `utils/render_scene.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "core/offline_scene_renderer.h"
#include "test_support.h"

#include <stdexcept>

int main() {
    const cv::Mat image = rt::render_offline_scene("quads", 1);
    expect_true(!image.empty(), "quads image exists");
    expect_true(image.cols == 1280, "quads width");
    expect_true(image.rows > 0, "quads height");

    bool threw = false;
    try {
        (void)rt::render_offline_scene("smoke", 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect_true(threw, "realtime-only scene rejected");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_offline_scene_renderer`
Expected: FAIL because the new helper does not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/core/offline_scene_renderer.h`

```cpp
#pragma once

#include <opencv2/core/mat.hpp>

#include <string_view>

namespace rt {

cv::Mat render_offline_scene(std::string_view scene_id, int samples_per_pixel_override = -1);

}  // namespace rt
```

`src/core/offline_scene_renderer.cpp`

```cpp
#include "core/offline_scene_renderer.h"

#include <stdexcept>

namespace rt {

cv::Mat render_offline_scene(std::string_view scene_id, int samples_per_pixel_override) {
    const std::optional<int> spp =
        samples_per_pixel_override > 0 ? std::optional<int>(samples_per_pixel_override) : std::nullopt;

    if (scene_id == "bouncing_spheres") {
        return render_bouncing_spheres(spp);
    }
    if (scene_id == "checkered_spheres") {
        return render_checkered_spheres(spp);
    }
    if (scene_id == "earth_sphere") {
        return render_earth_sphere(spp);
    }
    if (scene_id == "perlin_spheres") {
        return render_perlin_spheres(spp);
    }
    if (scene_id == "quads") {
        return render_quads(spp);
    }
    if (scene_id == "simple_light") {
        return render_simple_light(spp);
    }
    if (scene_id == "cornell_smoke") {
        return render_cornell_smoke(spp);
    }
    if (scene_id == "cornell_smoke_extreme") {
        return render_cornell_smoke(10000);
    }
    if (scene_id == "cornell_box") {
        return render_cornell_box(spp);
    }
    if (scene_id == "cornell_box_extreme") {
        return render_cornell_box(10000);
    }
    if (scene_id == "cornell_box_and_sphere") {
        return render_cornell_box_and_sphere(spp);
    }
    if (scene_id == "cornell_box_and_sphere_extreme") {
        return render_cornell_box_and_sphere(10000);
    }
    if (scene_id == "rttnw_final_scene") {
        return render_rttnw_final_scene(spp);
    }
    if (scene_id == "rttnw_final_scene_extreme") {
        return render_rttnw_final_scene(10000);
    }
    throw std::invalid_argument("unsupported offline scene");
}

}  // namespace rt
```

`utils/render_scene.cpp`

```cpp
#include "core/offline_scene_renderer.h"
#include "realtime/scene_catalog.h"

std::vector<std::string> cpu_scene_choices;
for (const rt::SceneCatalogEntry& entry : rt::scene_catalog()) {
    if (entry.supports_cpu_render) {
        cpu_scene_choices.emplace_back(entry.id);
    }
}

// Keep only CLI parsing, output path selection, and image writing here.
const cv::Mat img = rt::render_offline_scene(scene_to_render);
```

Replace the hard-coded `--scene` choices call with an `argparse` choices call built from `cpu_scene_choices`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_offline_scene_renderer render_scene && ctest --test-dir build -R test_offline_scene_renderer --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/core/offline_scene_renderer.h src/core/offline_scene_renderer.cpp tests/test_offline_scene_renderer.cpp utils/render_scene.cpp
git commit -m "refactor: extract offline scene renderer dispatch"
```

### Task 3: Add A Shared Realtime Scene Factory

**Files:**
- Create: `src/realtime/realtime_scene_factory.h`
- Create: `src/realtime/realtime_scene_factory.cpp`
- Create: `tests/test_realtime_scene_factory.cpp`
- Modify: `src/realtime/viewer/default_viewer_scene.h`
- Modify: `src/realtime/viewer/default_viewer_scene.cpp`
- Modify: `utils/render_realtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "test_support.h"

int main() {
    const rt::PackedScene smoke = rt::make_realtime_scene("smoke").pack();
    expect_true(smoke.sphere_count >= 1, "smoke scene has geometry");

    const rt::PackedScene final_room = rt::make_realtime_scene("final_room").pack();
    expect_true(final_room.quad_count >= 7, "final_room quads");

    expect_true(rt::realtime_scene_supported("smoke"), "smoke supported");
    expect_true(!rt::realtime_scene_supported("cornell_box"), "cornell_box unsupported");

    const rt::PackedScene default_view = rt::viewer::make_default_viewer_scene().pack();
    expect_true(default_view.quad_count == final_room.quad_count, "default viewer scene is final_room");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_realtime_scene_factory`
Expected: FAIL because the factory API does not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/realtime/realtime_scene_factory.h`

```cpp
#pragma once

#include "realtime/scene_description.h"
#include "realtime/viewer/body_pose.h"

#include <string_view>

namespace rt {

bool realtime_scene_supported(std::string_view scene_id);
SceneDescription make_realtime_scene(std::string_view scene_id);
viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id);

}  // namespace rt
```

`src/realtime/realtime_scene_factory.cpp`

```cpp
#include "realtime/realtime_scene_factory.h"

#include "realtime/frame_convention.h"

#include <stdexcept>

namespace rt {
namespace {

SceneDescription make_smoke_scene() {
    SceneDescription scene;
    const int diffuse = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.75, 0.25, 0.2}});
    const int light = scene.add_material(DiffuseLightMaterial {Eigen::Vector3d {10.0, 10.0, 10.0}});
    scene.add_sphere(SpherePrimitive {diffuse, legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, -1.0}), 0.5, false});
    scene.add_quad(QuadPrimitive {
        light,
        legacy_renderer_to_world(Eigen::Vector3d {-0.75, 1.25, -1.5}),
        legacy_renderer_to_world(Eigen::Vector3d {1.5, 0.0, 0.0}),
        legacy_renderer_to_world(Eigen::Vector3d {0.0, 0.0, 1.5}),
        false,
    });
    return scene;
}

}  // namespace

bool realtime_scene_supported(std::string_view scene_id) {
    return scene_id == "smoke" || scene_id == "final_room";
}

SceneDescription make_realtime_scene(std::string_view scene_id) {
    if (scene_id == "smoke") {
        return make_smoke_scene();
    }
    if (scene_id == "final_room") {
        return viewer::make_final_room_scene();
    }
    throw std::invalid_argument("unsupported realtime scene");
}

viewer::BodyPose default_spawn_pose_for_scene(std::string_view scene_id) {
    if (scene_id == "final_room") {
        return viewer::default_spawn_pose();
    }
    return viewer::BodyPose {
        .position = Eigen::Vector3d(0.0, 0.0, 0.0),
        .yaw_deg = 0.0,
        .pitch_deg = 0.0,
    };
}

}  // namespace rt
```

Update `utils/render_realtime.cpp` so `make_scene()` delegates to `rt::make_realtime_scene(scene_name)` and its `--scene` validation uses the shared catalog instead of `scene_name != "smoke" && scene_name != "final_room"`.

Update `src/realtime/viewer/default_viewer_scene.cpp` so:

```cpp
SceneDescription make_default_viewer_scene() {
    return rt::make_realtime_scene("final_room");
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_realtime_scene_factory render_realtime && ctest --test-dir build -R "test_realtime_scene_factory|test_viewer_four_camera_rig|test_render_realtime_final_room_quality_cli" --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/realtime_scene_factory.h src/realtime/realtime_scene_factory.cpp src/realtime/viewer/default_viewer_scene.h src/realtime/viewer/default_viewer_scene.cpp tests/test_realtime_scene_factory.cpp utils/render_realtime.cpp
git commit -m "feat: add shared realtime scene factory"
```

### Task 4: Add A Viewer Scene-Switch Controller

**Files:**
- Create: `src/realtime/viewer/scene_switch_controller.h`
- Create: `src/realtime/viewer/scene_switch_controller.cpp`
- Create: `tests/test_viewer_scene_switch_controller.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_viewer_scene_switch_controller`
Expected: FAIL because the controller does not exist yet.

- [ ] **Step 3: Write minimal implementation**

`src/realtime/viewer/scene_switch_controller.h`

```cpp
#pragma once

#include <string>

namespace rt::viewer {

struct SceneSwitchResult {
    bool applied = false;
    bool reset_pose = false;
    std::string error_message;
};

class SceneSwitchController {
public:
    explicit SceneSwitchController(std::string initial_scene_id);

    void request_scene(std::string scene_id);
    SceneSwitchResult resolve_pending();

    const std::string& current_scene_id() const;
    const std::string& last_error() const;

private:
    std::string current_scene_id_;
    std::string pending_scene_id_;
    std::string last_error_;
};

}  // namespace rt::viewer
```

`src/realtime/viewer/scene_switch_controller.cpp`

```cpp
#include "realtime/viewer/scene_switch_controller.h"

#include "realtime/realtime_scene_factory.h"

namespace rt::viewer {

SceneSwitchController::SceneSwitchController(std::string initial_scene_id)
    : current_scene_id_(std::move(initial_scene_id)) {}

void SceneSwitchController::request_scene(std::string scene_id) {
    pending_scene_id_ = std::move(scene_id);
}

SceneSwitchResult SceneSwitchController::resolve_pending() {
    SceneSwitchResult result;
    if (pending_scene_id_.empty() || pending_scene_id_ == current_scene_id_) {
        return result;
    }
    if (!rt::realtime_scene_supported(pending_scene_id_)) {
        last_error_ = "Scene is not available in realtime";
        result.error_message = last_error_;
        pending_scene_id_.clear();
        return result;
    }

    current_scene_id_ = pending_scene_id_;
    pending_scene_id_.clear();
    last_error_.clear();
    result.applied = true;
    result.reset_pose = true;
    return result;
}

const std::string& SceneSwitchController::current_scene_id() const { return current_scene_id_; }
const std::string& SceneSwitchController::last_error() const { return last_error_; }

}  // namespace rt::viewer
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_viewer_scene_switch_controller && ctest --test-dir build -R test_viewer_scene_switch_controller --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/viewer/scene_switch_controller.h src/realtime/viewer/scene_switch_controller.cpp tests/test_viewer_scene_switch_controller.cpp
git commit -m "feat: add viewer scene switch controller"
```

### Task 5: Wire ImGui And Runtime Scene Switching Into The Viewer

**Files:**
- Modify: `vcpkg_json`
- Modify: `CMakeLists.txt`
- Modify: `utils/render_realtime_viewer.cpp`
- Modify: `utils/render_realtime.cpp`

- [ ] **Step 1: Write the failing build/runtime verification**

Add this dependency entry to `vcpkg_json` before any code changes:

```json
{
  "name": "imgui",
  "features": [
    "glfw-binding",
    "opengl3-binding"
  ]
}
```

Add this build-time expectation:

```cmake
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()
```

Run: `cmake -S . -B build`
Expected: FAIL until `find_package(imgui CONFIG REQUIRED)` and viewer linkage are added.

- [ ] **Step 2: Implement minimal build integration**

`CMakeLists.txt`

```cmake
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()

project(ray_tracing LANGUAGES C CXX CUDA VERSION 0.0.1.0)

find_package(imgui CONFIG REQUIRED)

if(ENABLE_GUI_VIEWER)
    target_link_libraries(render_realtime_viewer PRIVATE imgui::imgui)
endif()
```

`utils/render_realtime_viewer.cpp`

```cpp
#include "realtime/scene_catalog.h"
#include "realtime/realtime_scene_factory.h"
#include "realtime/viewer/scene_switch_controller.h"

#include <argparse/argparse.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
```

Add a CLI argument and viewer state setup near `main()` startup:

```cpp
std::string scene_name = "final_room";
program.add_argument("--scene")
    .help("viewer scene id")
    .default_value(scene_name)
    .store_into(scene_name);

rt::viewer::SceneSwitchController scene_controller(scene_name);
rt::viewer::BodyPose pose = rt::default_spawn_pose_for_scene(scene_name);
rt::PackedScene scene = rt::make_realtime_scene(scene_name).pack();
```

Initialize ImGui after creating the GLFW window:

```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 120");
```

Add a small always-visible control panel each frame:

```cpp
ImGui_ImplGlfw_NewFrame();
ImGui_ImplOpenGL3_NewFrame();
ImGui::NewFrame();

ImGui::Begin("Viewer");
ImGui::Text("Current scene: %s", scene_controller.current_scene_id().c_str());
if (ImGui::BeginCombo("Scene", scene_controller.current_scene_id().c_str())) {
    for (const rt::SceneCatalogEntry& entry : rt::scene_catalog()) {
        const bool supported = entry.supports_realtime;
        if (!supported) {
            ImGui::BeginDisabled();
        }
        const bool selected = entry.id == scene_controller.current_scene_id();
        if (ImGui::Selectable(std::string(entry.label).c_str(), selected)) {
            scene_controller.request_scene(std::string(entry.id));
        }
        if (!supported) {
            ImGui::EndDisabled();
        }
    }
    ImGui::EndCombo();
}
if (!scene_controller.last_error().empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", scene_controller.last_error().c_str());
}
ImGui::End();
```

At the safe point in the main loop, resolve the request and rebuild the scene:

```cpp
const rt::viewer::SceneSwitchResult switch_result = scene_controller.resolve_pending();
if (switch_result.applied) {
    pose = rt::default_spawn_pose_for_scene(scene_controller.current_scene_id());
    scene = rt::make_realtime_scene(scene_controller.current_scene_id()).pack();
    pool.prepare_scene(scene);
} else if (!switch_result.error_message.empty()) {
    fmt::print(stderr, "{}\n", switch_result.error_message);
}
```

Render ImGui after drawing the textured quads:

```cpp
ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

Shutdown ImGui before destroying the window:

```cpp
ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();
```

Also update `utils/render_realtime.cpp` so its `--scene` validation uses `scene_catalog()` entries where `supports_realtime` is true.

- [ ] **Step 3: Run verification**

Run: `cmake -S . -B build && cmake --build build --target render_realtime_viewer render_realtime test_scene_catalog test_offline_scene_renderer test_realtime_scene_factory test_viewer_scene_switch_controller`
Expected: PASS

Run: `ctest --test-dir build -R "test_scene_catalog|test_offline_scene_renderer|test_realtime_scene_factory|test_viewer_scene_switch_controller|test_viewer_four_camera_rig|test_render_realtime_final_room_quality_cli" --output-on-failure`
Expected: PASS

Run manually: `build/bin/render_realtime_viewer --scene final_room`
Expected:
- window opens
- ImGui panel is visible
- `final_room` is active on startup
- `smoke` can be selected at runtime and immediately re-renders
- CPU-only scenes are listed but disabled

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt utils/render_realtime.cpp utils/render_realtime_viewer.cpp vcpkg_json
git commit -m "feat: add imgui scene switching to viewer"
```

## Self-Review Checklist

- [ ] Confirm the scene catalog is the only authoritative source of executable-facing scene ids
- [ ] Confirm `render_scene.cpp` no longer owns the full scene-dispatch switch in `main()`
- [ ] Confirm realtime-only scenes and CPU-only scenes are distinguished by capability flags
- [ ] Confirm the viewer can keep the previous scene if a switch fails
- [ ] Confirm `vcpkg_json` only describes `imgui` and no local installation steps were added
- [ ] Confirm the CMake toolchain fallback is set before the `project` declaration
- [ ] Confirm ImGui integration uses `glfw-binding` + `opengl3-binding`, matching the globally installed vcpkg package
- [ ] Confirm the viewer uses the OpenGL3 ImGui backend with a compatibility GLSL string such as `#version 120`, so existing immediate-mode viewer drawing can remain in place during this change
