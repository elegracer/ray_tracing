# Default GUI Realtime Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a default interactive GUI viewer for `final_room` that shows four body-mounted cameras in a `2x2` grid and supports mouse-look plus `WASD` flying movement.

**Architecture:** Keep the existing `render_realtime` CLI untouched for batch use and add a separate `render_realtime_viewer` executable for the human-facing interactive mode. Put body-pose math, default spawn data, and four-camera rig generation into a focused helper layer under `src/realtime/`, then let the viewer own the window/input loop and reuse `RendererPool` plus existing scene description APIs for rendering.

**Tech Stack:** C++23, CMake, GLFW, OpenGL, existing `src/realtime/` renderer path, Eigen, OpenCV, CTest

---

## File Structure

- Modify: `CMakeLists.txt`
  - Add `glfw3` / OpenGL viewer dependencies
  - Register the new viewer executable
  - Register new host-side tests for body navigation and rig generation
- Create: `src/realtime/viewer/body_pose.h`
  - Body pose state, mouse-look helpers, movement helpers, and spawn constants
- Create: `src/realtime/viewer/body_pose.cpp`
  - Implementation of yaw/pitch clamping, forward/right basis generation, and pose integration
- Create: `src/realtime/viewer/default_viewer_scene.h`
  - `final_room` scene builder and default viewer profile declaration
- Create: `src/realtime/viewer/default_viewer_scene.cpp`
  - Reusable `final_room` scene construction and viewer render-profile definition
- Create: `src/realtime/viewer/four_camera_rig.h`
  - Four-camera viewer rig builder API
- Create: `src/realtime/viewer/four_camera_rig.cpp`
  - Build four pinhole cameras from body pose with fixed yaw offsets
- Create: `utils/render_realtime_viewer.cpp`
  - GLFW window, input loop, render loop, texture upload, and `2x2` draw path
- Create: `tests/test_viewer_body_pose.cpp`
  - Validate spawn pose, pitch clamp, and flying-motion basis math
- Create: `tests/test_viewer_four_camera_rig.cpp`
  - Validate camera ordering, fixed yaw offsets, and shared position behavior

## Task 1: Add Viewer Body Pose Helpers

**Files:**
- Create: `src/realtime/viewer/body_pose.h`
- Create: `src/realtime/viewer/body_pose.cpp`
- Create: `tests/test_viewer_body_pose.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing body-pose test**

```cpp
// tests/test_viewer_body_pose.cpp
#include "realtime/viewer/body_pose.h"
#include "test_support.h"

#include <cmath>

int main() {
    using rt::viewer::BodyPose;
    using rt::viewer::clamp_pitch_deg;
    using rt::viewer::default_spawn_pose;
    using rt::viewer::forward_direction;
    using rt::viewer::integrate_wasd;
    using rt::viewer::right_direction;

    const BodyPose spawn = default_spawn_pose();
    expect_vec3_near(spawn.position, Eigen::Vector3d {0.0, 0.35, 0.8}, 1e-12, "default spawn position");
    expect_near(spawn.yaw_deg, 0.0, 1e-12, "default spawn yaw");
    expect_near(spawn.pitch_deg, 0.0, 1e-12, "default spawn pitch");

    expect_near(clamp_pitch_deg(95.0), 80.0, 1e-12, "pitch upper clamp");
    expect_near(clamp_pitch_deg(-91.0), -80.0, 1e-12, "pitch lower clamp");

    const BodyPose neutral {
        .position = Eigen::Vector3d::Zero(),
        .yaw_deg = 0.0,
        .pitch_deg = 0.0,
    };
    expect_vec3_near(forward_direction(neutral), Eigen::Vector3d {0.0, 0.0, -1.0}, 1e-12, "neutral forward");
    expect_vec3_near(right_direction(neutral), Eigen::Vector3d {1.0, 0.0, 0.0}, 1e-12, "neutral right");

    const BodyPose pitched {
        .position = Eigen::Vector3d::Zero(),
        .yaw_deg = 0.0,
        .pitch_deg = 30.0,
    };
    const Eigen::Vector3d pitched_forward = forward_direction(pitched);
    expect_true(pitched_forward.y() > 0.49, "pitch affects forward movement");
    expect_near(pitched_forward.norm(), 1.0, 1e-12, "forward normalized");

    BodyPose moved = neutral;
    integrate_wasd(moved, true, false, false, false, 0.5);
    expect_true(moved.position.z() < -0.49, "W moves forward");
    expect_near(moved.position.x(), 0.0, 1e-12, "W keeps x");

    BodyPose strafed = neutral;
    integrate_wasd(strafed, false, false, false, true, 0.5);
    expect_true(strafed.position.x() > 0.49, "D strafes right");
    expect_near(strafed.position.z(), 0.0, 1e-12, "D keeps z");
    return 0;
}
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_body_pose -j 4`
Expected: FAIL with an error such as `No rule to make target 'test_viewer_body_pose'` or `realtime/viewer/body_pose.h: No such file or directory`

- [ ] **Step 3: Add the body-pose API**

```cpp
// src/realtime/viewer/body_pose.h
#pragma once

#include <Eigen/Core>

namespace rt::viewer {

struct BodyPose {
    Eigen::Vector3d position = Eigen::Vector3d::Zero();
    double yaw_deg = 0.0;
    double pitch_deg = 0.0;
};

BodyPose default_spawn_pose();
double clamp_pitch_deg(double pitch_deg);
Eigen::Vector3d forward_direction(const BodyPose& pose);
Eigen::Vector3d right_direction(const BodyPose& pose);
void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel);
void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    double distance);

}  // namespace rt::viewer
```

```cpp
// src/realtime/viewer/body_pose.cpp
#include "realtime/viewer/body_pose.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>

namespace rt::viewer {

namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

Eigen::Matrix3d yaw_pitch_matrix(double yaw_deg, double pitch_deg) {
    const Eigen::AngleAxisd yaw(yaw_deg * kDegToRad, Eigen::Vector3d::UnitY());
    const Eigen::AngleAxisd pitch(pitch_deg * kDegToRad, Eigen::Vector3d::UnitX());
    return yaw.toRotationMatrix() * pitch.toRotationMatrix();
}

}  // namespace

BodyPose default_spawn_pose() {
    return BodyPose {
        .position = Eigen::Vector3d {0.0, 0.35, 0.8},
        .yaw_deg = 0.0,
        .pitch_deg = 0.0,
    };
}

double clamp_pitch_deg(double pitch_deg) {
    return std::clamp(pitch_deg, -80.0, 80.0);
}

Eigen::Vector3d forward_direction(const BodyPose& pose) {
    return (yaw_pitch_matrix(pose.yaw_deg, pose.pitch_deg) * Eigen::Vector3d {0.0, 0.0, -1.0}).normalized();
}

Eigen::Vector3d right_direction(const BodyPose& pose) {
    return (yaw_pitch_matrix(pose.yaw_deg, pose.pitch_deg) * Eigen::Vector3d {1.0, 0.0, 0.0}).normalized();
}

void integrate_mouse_look(BodyPose& pose, double delta_x, double delta_y, double degrees_per_pixel) {
    pose.yaw_deg += delta_x * degrees_per_pixel;
    pose.pitch_deg = clamp_pitch_deg(pose.pitch_deg - delta_y * degrees_per_pixel);
}

void integrate_wasd(BodyPose& pose, bool move_forward, bool move_backward, bool move_left, bool move_right,
    double distance) {
    Eigen::Vector3d delta = Eigen::Vector3d::Zero();
    if (move_forward) {
        delta += forward_direction(pose);
    }
    if (move_backward) {
        delta -= forward_direction(pose);
    }
    if (move_left) {
        delta -= right_direction(pose);
    }
    if (move_right) {
        delta += right_direction(pose);
    }
    if (delta.squaredNorm() > 0.0) {
        pose.position += delta.normalized() * distance;
    }
}

}  // namespace rt::viewer
```

- [ ] **Step 4: Wire the new test target into CMake**

```cmake
# CMakeLists.txt
add_executable(test_viewer_body_pose)
target_sources(test_viewer_body_pose
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_body_pose.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/body_pose.cpp
)
target_link_libraries(test_viewer_body_pose PRIVATE core)
add_test(NAME test_viewer_body_pose COMMAND test_viewer_body_pose)
```

- [ ] **Step 5: Run the test and verify it passes**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_body_pose -j 4 && ctest --test-dir build-clang-vcpkg-settings -R test_viewer_body_pose --output-on-failure`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt tests/test_viewer_body_pose.cpp src/realtime/viewer/body_pose.h src/realtime/viewer/body_pose.cpp
git commit -m "test: add viewer body pose helpers"
```

## Task 2: Extract Default Viewer Scene and Interactive Render Profile

**Files:**
- Create: `src/realtime/viewer/default_viewer_scene.h`
- Create: `src/realtime/viewer/default_viewer_scene.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_viewer_four_camera_rig.cpp`

- [ ] **Step 1: Write the failing scene/profile expectations in the rig test**

```cpp
// tests/test_viewer_four_camera_rig.cpp
#include "realtime/render_profile.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

int main() {
    const rt::SceneDescription scene = rt::viewer::make_default_viewer_scene();
    const rt::PackedScene packed_scene = scene.pack();
    expect_true(packed_scene.material_count >= 5, "final_room materials");
    expect_true(packed_scene.sphere_count >= 6, "final_room spheres");
    expect_true(packed_scene.quad_count >= 7, "final_room quads");

    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();
    expect_true(profile.samples_per_pixel == 1, "viewer spp");
    expect_true(profile.max_bounces == 2, "viewer bounces");
    expect_true(!profile.enable_denoise, "viewer denoise disabled");
    return 0;
}
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_four_camera_rig -j 4`
Expected: FAIL with an error such as `realtime/viewer/default_viewer_scene.h: No such file or directory`

- [ ] **Step 3: Add reusable scene/profile declarations**

```cpp
// src/realtime/viewer/default_viewer_scene.h
#pragma once

#include "realtime/render_profile.h"
#include "realtime/scene_description.h"

namespace rt::viewer {

SceneDescription make_default_viewer_scene();
RenderProfile default_viewer_profile();

}  // namespace rt::viewer
```

- [ ] **Step 4: Move the `final_room` viewer scene and profile into a shared implementation**

```cpp
// src/realtime/viewer/default_viewer_scene.cpp
#include "realtime/viewer/default_viewer_scene.h"

#include <Eigen/Core>

namespace rt::viewer {

SceneDescription make_default_viewer_scene() {
    SceneDescription scene;
    const int white = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.73, 0.73, 0.73}});
    const int green = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.30, 0.70, 0.35}});
    const int red = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.72, 0.25, 0.22}});
    const int blue = scene.add_material(LambertianMaterial {Eigen::Vector3d {0.25, 0.35, 0.75}});
    const int light = scene.add_material(DiffuseLightMaterial {Eigen::Vector3d {12.0, 12.0, 12.0}});

    scene.add_quad(QuadPrimitive {white, Eigen::Vector3d {-4.0, -1.0, -4.0}, Eigen::Vector3d {0.0, 0.0, 8.0},
        Eigen::Vector3d {8.0, 0.0, 0.0}, false});
    scene.add_quad(QuadPrimitive {white, Eigen::Vector3d {-4.0, 3.5, -4.0}, Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0}, false});
    scene.add_quad(QuadPrimitive {green, Eigen::Vector3d {-4.0, -1.0, -4.0}, Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0}, false});
    scene.add_quad(QuadPrimitive {red, Eigen::Vector3d {4.0, -1.0, -4.0}, Eigen::Vector3d {0.0, 0.0, 8.0},
        Eigen::Vector3d {0.0, 4.5, 0.0}, false});
    scene.add_quad(QuadPrimitive {white, Eigen::Vector3d {-4.0, -1.0, -4.0}, Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.5, 0.0}, false});
    scene.add_quad(QuadPrimitive {blue, Eigen::Vector3d {-4.0, -1.0, 4.0}, Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {8.0, 0.0, 0.0}, false});
    scene.add_quad(QuadPrimitive {light, Eigen::Vector3d {-1.0, 3.15, -1.0}, Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 2.0}, false});

    scene.add_quad(QuadPrimitive {white, Eigen::Vector3d {-3.2, -0.25, -3.0}, Eigen::Vector3d {0.0, 0.0, 1.8},
        Eigen::Vector3d {1.8, 0.0, 0.0}, false});
    scene.add_quad(QuadPrimitive {white, Eigen::Vector3d {1.2, 0.15, 1.0}, Eigen::Vector3d {0.0, 0.0, 1.6},
        Eigen::Vector3d {1.6, 0.0, 0.0}, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {0.0, 0.25, -1.2}, 0.55, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {-1.6, 0.35, 1.7}, 0.55, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {-3.1, 1.0, 0.8}, 0.55, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {3.0, 1.35, -0.9}, 0.65, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {1.1, 1.1, -3.0}, 0.60, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {-0.8, 2.55, 2.2}, 0.45, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {0.9, 0.55, -0.1}, 0.45, false});
    scene.add_sphere(SpherePrimitive {white, Eigen::Vector3d {-0.35, 0.4, -1.15}, 0.35, false});
    return scene;
}

RenderProfile default_viewer_profile() {
    return RenderProfile {
        .samples_per_pixel = 1,
        .max_bounces = 2,
        .enable_denoise = false,
        .rr_start_bounce = 2,
        .accumulation_reset_rotation_deg = 2.0,
        .accumulation_reset_translation = 0.05,
    };
}

}  // namespace rt::viewer
```

- [ ] **Step 5: Add the new source file to `core`**

```cmake
# CMakeLists.txt
target_sources(core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/default_viewer_scene.cpp
)

add_executable(test_viewer_four_camera_rig)
target_sources(test_viewer_four_camera_rig
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_viewer_four_camera_rig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/body_pose.cpp
)
target_link_libraries(test_viewer_four_camera_rig PRIVATE core)
add_test(NAME test_viewer_four_camera_rig COMMAND test_viewer_four_camera_rig)
```

- [ ] **Step 6: Run the target and verify it passes**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_four_camera_rig -j 4`
Expected: FAIL later because `realtime/viewer/four_camera_rig.h` still does not exist, proving the scene/profile dependency is wired in

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/realtime/viewer/default_viewer_scene.h src/realtime/viewer/default_viewer_scene.cpp
git commit -m "feat: add default viewer scene and profile"
```

## Task 3: Add Four-Camera Viewer Rig Generation

**Files:**
- Create: `src/realtime/viewer/four_camera_rig.h`
- Create: `src/realtime/viewer/four_camera_rig.cpp`
- Modify: `tests/test_viewer_four_camera_rig.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Expand the failing rig test**

```cpp
// tests/test_viewer_four_camera_rig.cpp
#include "realtime/frame_convention.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"
#include "test_support.h"

#include <Eigen/Geometry>

namespace {

double yaw_from_forward(const Eigen::Vector3d& forward) {
    return std::atan2(forward.x(), -forward.z()) * 180.0 / 3.14159265358979323846;
}

double wrap_degrees(double deg) {
    while (deg > 180.0) {
        deg -= 360.0;
    }
    while (deg < -180.0) {
        deg += 360.0;
    }
    return deg;
}

}  // namespace

int main() {
    const rt::SceneDescription scene = rt::viewer::make_default_viewer_scene();
    const rt::PackedScene packed_scene = scene.pack();
    expect_true(packed_scene.material_count >= 5, "final_room materials");
    expect_true(packed_scene.sphere_count >= 6, "final_room spheres");
    expect_true(packed_scene.quad_count >= 7, "final_room quads");

    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();
    expect_true(profile.samples_per_pixel == 1, "viewer spp");
    expect_true(profile.max_bounces == 2, "viewer bounces");
    expect_true(!profile.enable_denoise, "viewer denoise disabled");

    const rt::viewer::BodyPose pose {
        .position = Eigen::Vector3d {1.0, 2.0, 3.0},
        .yaw_deg = 15.0,
        .pitch_deg = 20.0,
    };
    const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, 640, 480).pack();
    expect_near(static_cast<double>(rig.active_count), 4.0, 1e-12, "active cameras");

    const Eigen::Vector3d expected_translation = rt::body_to_renderer(pose.position);
    for (int i = 0; i < 4; ++i) {
        expect_true(rig.cameras[static_cast<std::size_t>(i)].enabled == 1, "viewer camera enabled");
        expect_true(rig.cameras[static_cast<std::size_t>(i)].width == 640, "viewer width");
        expect_true(rig.cameras[static_cast<std::size_t>(i)].height == 480, "viewer height");
        expect_vec3_near(rig.cameras[static_cast<std::size_t>(i)].T_rc.block<3, 1>(0, 3),
            expected_translation, 1e-12, "shared translation");
    }

    const std::array<double, 4> expected_offsets = {0.0, 90.0, -90.0, 180.0};
    for (int i = 0; i < 4; ++i) {
        const Eigen::Vector3d camera_forward_renderer =
            rig.cameras[static_cast<std::size_t>(i)].T_rc.block<3, 3>(0, 0) * Eigen::Vector3d {0.0, 0.0, 1.0};
        const double actual_yaw = yaw_from_forward(camera_forward_renderer);
        const double expected_yaw = wrap_degrees(pose.yaw_deg + expected_offsets[static_cast<std::size_t>(i)]);
        expect_near(wrap_degrees(actual_yaw - expected_yaw), 0.0, 1e-9, "camera yaw offset");
        expect_true(camera_forward_renderer.y() > 0.2, "shared pitch tilts upward");
    }
    return 0;
}
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_four_camera_rig -j 4`
Expected: FAIL with `realtime/viewer/four_camera_rig.h: No such file or directory`

- [ ] **Step 3: Add the rig-builder declarations**

```cpp
// src/realtime/viewer/four_camera_rig.h
#pragma once

#include "realtime/camera_rig.h"
#include "realtime/viewer/body_pose.h"

namespace rt::viewer {

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height);

}  // namespace rt::viewer
```

- [ ] **Step 4: Implement four-camera rig generation**

```cpp
// src/realtime/viewer/four_camera_rig.cpp
#include "realtime/viewer/four_camera_rig.h"

#include <Eigen/Geometry>

#include <array>

namespace rt::viewer {

namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

Eigen::Matrix3d yaw_pitch_matrix(double yaw_deg, double pitch_deg) {
    const Eigen::AngleAxisd yaw(yaw_deg * kDegToRad, Eigen::Vector3d::UnitY());
    const Eigen::AngleAxisd pitch(pitch_deg * kDegToRad, Eigen::Vector3d::UnitX());
    return yaw.toRotationMatrix() * pitch.toRotationMatrix();
}

}  // namespace

CameraRig make_default_viewer_rig(const BodyPose& pose, int width, int height) {
    CameraRig rig;
    const Pinhole32Params pinhole {
        0.75 * static_cast<double>(width),
        0.75 * static_cast<double>(height),
        0.5 * static_cast<double>(width),
        0.5 * static_cast<double>(height),
        0.0, 0.0, 0.0, 0.0, 0.0,
    };

    const std::array<double, 4> yaw_offsets = {0.0, 90.0, -90.0, 180.0};
    for (double yaw_offset : yaw_offsets) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = pose.position;
        T_bc.linear() = yaw_pitch_matrix(pose.yaw_deg + yaw_offset, pose.pitch_deg);
        rig.add_pinhole(pinhole, T_bc, width, height);
    }
    return rig;
}

}  // namespace rt::viewer
```

- [ ] **Step 5: Wire the test target into CMake**

```cmake
# CMakeLists.txt
target_sources(test_viewer_four_camera_rig
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
```

- [ ] **Step 6: Run the test and verify it passes**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_four_camera_rig -j 4 && ctest --test-dir build-clang-vcpkg-settings -R test_viewer_four_camera_rig --output-on-failure`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt tests/test_viewer_four_camera_rig.cpp src/realtime/viewer/four_camera_rig.h src/realtime/viewer/four_camera_rig.cpp
git commit -m "test: add default viewer four camera rig"
```

## Task 4: Add the Viewer Executable and `2x2` Display Loop

**Files:**
- Create: `utils/render_realtime_viewer.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the failing build hook for the viewer target**

```cmake
# CMakeLists.txt
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(render_realtime_viewer)
target_sources(render_realtime_viewer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/render_realtime_viewer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/body_pose.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/realtime/viewer/four_camera_rig.cpp
)
target_link_libraries(render_realtime_viewer
    PRIVATE
        realtime_gpu
        core
        glfw
        OpenGL::GL
)
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j 4`
Expected: FAIL with `utils/render_realtime_viewer.cpp: No such file or directory`

- [ ] **Step 3: Implement the interactive viewer**

```cpp
// utils/render_realtime_viewer.cpp
#include "realtime/gpu/renderer_pool.h"
#include "realtime/viewer/body_pose.h"
#include "realtime/viewer/default_viewer_scene.h"
#include "realtime/viewer/four_camera_rig.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 960;
constexpr int kViewWidth = 640;
constexpr int kViewHeight = 480;
constexpr double kLookDegreesPerPixel = 0.08;
constexpr double kMoveSpeedUnitsPerSecond = 1.8;

std::vector<std::uint8_t> to_rgba8(const rt::RadianceFrame& frame) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(frame.width * frame.height * 4), 255);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t pixel = static_cast<std::size_t>(y * frame.width + x);
            const std::size_t src = pixel * 4U;
            const std::size_t dst = pixel * 4U;
            out[dst + 0] = static_cast<std::uint8_t>(255.0f * std::clamp(frame.beauty_rgba[src + 0], 0.0f, 1.0f));
            out[dst + 1] = static_cast<std::uint8_t>(255.0f * std::clamp(frame.beauty_rgba[src + 1], 0.0f, 1.0f));
            out[dst + 2] = static_cast<std::uint8_t>(255.0f * std::clamp(frame.beauty_rgba[src + 2], 0.0f, 1.0f));
            out[dst + 3] = 255;
        }
    }
    return out;
}

void upload_texture(GLuint texture, const rt::RadianceFrame& frame) {
    const std::vector<std::uint8_t> rgba8 = to_rgba8(frame);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba8.data());
}

void draw_textured_quad(GLuint texture, int x, int y, int width, int height, int window_width, int window_height) {
    const float left = 2.0f * static_cast<float>(x) / static_cast<float>(window_width) - 1.0f;
    const float right = 2.0f * static_cast<float>(x + width) / static_cast<float>(window_width) - 1.0f;
    const float top = 1.0f - 2.0f * static_cast<float>(y) / static_cast<float>(window_height);
    const float bottom = 1.0f - 2.0f * static_cast<float>(y + height) / static_cast<float>(window_height);

    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(left, bottom);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(left, top);
    glEnd();
}

}  // namespace

int main() {
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "render_realtime_viewer", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::array<GLuint, 4> textures {};
    glGenTextures(4, textures.data());
    for (GLuint texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kViewWidth, kViewHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    rt::viewer::BodyPose pose = rt::viewer::default_spawn_pose();
    const rt::PackedScene scene = rt::viewer::make_default_viewer_scene().pack();
    const rt::RenderProfile profile = rt::viewer::default_viewer_profile();
    rt::RendererPool pool(4);
    pool.prepare_scene(scene);

    double last_time = glfwGetTime();
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;
    glfwGetCursorPos(window, &last_cursor_x, &last_cursor_y);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const double dt = now - last_time;
        last_time = now;

        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(window, &cursor_x, &cursor_y);
        rt::viewer::integrate_mouse_look(
            pose, cursor_x - last_cursor_x, cursor_y - last_cursor_y, kLookDegreesPerPixel);
        last_cursor_x = cursor_x;
        last_cursor_y = cursor_y;

        rt::viewer::integrate_wasd(
            pose,
            glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
            kMoveSpeedUnitsPerSecond * dt);

        const rt::PackedCameraRig rig = rt::viewer::make_default_viewer_rig(pose, kViewWidth, kViewHeight).pack();
        const std::vector<rt::CameraRenderResult> frames = pool.render_frame(rig, profile, 4);

        for (const rt::CameraRenderResult& result : frames) {
            upload_texture(textures.at(static_cast<std::size_t>(result.camera_index)), result.profiled.frame);
        }

        glViewport(0, 0, kWindowWidth, kWindowHeight);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_TEXTURE_2D);

        draw_textured_quad(textures[0], 0, 0, kWindowWidth / 2, kWindowHeight / 2, kWindowWidth, kWindowHeight);
        draw_textured_quad(textures[1], kWindowWidth / 2, 0, kWindowWidth / 2, kWindowHeight / 2, kWindowWidth, kWindowHeight);
        draw_textured_quad(textures[2], 0, kWindowHeight / 2, kWindowWidth / 2, kWindowHeight / 2, kWindowWidth, kWindowHeight);
        draw_textured_quad(textures[3], kWindowWidth / 2, kWindowHeight / 2, kWindowWidth / 2, kWindowHeight / 2, kWindowWidth, kWindowHeight);

        glfwSwapBuffers(window);
    }

    glDeleteTextures(4, textures.data());
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

- [ ] **Step 4: Build the viewer and verify it links**

Run: `cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j 4`
Expected: PASS

- [ ] **Step 5: Run the viewer manually**

Run: `./bin/render_realtime_viewer`
Expected: a window opens on `final_room`, shows four equal panels, mouse movement changes look direction, and `WASD` moves the body

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt utils/render_realtime_viewer.cpp
git commit -m "feat: add default gui realtime viewer"
```

## Task 5: Reuse Shared Scene Logic from the Batch CLI

**Files:**
- Modify: `utils/render_realtime.cpp`
- Modify: `src/realtime/viewer/default_viewer_scene.h`
- Modify: `src/realtime/viewer/default_viewer_scene.cpp`

- [ ] **Step 1: Replace the duplicated `final_room` scene builder in the CLI**

```cpp
// utils/render_realtime.cpp
#include "realtime/viewer/default_viewer_scene.h"

rt::SceneDescription make_scene(const std::string& scene_name) {
    if (scene_name == "final_room") {
        return rt::viewer::make_default_viewer_scene();
    }
    return make_smoke_scene();
}
```

- [ ] **Step 2: Remove the old local `make_final_room_scene()`**

```cpp
// utils/render_realtime.cpp
// After make_scene() forwards to rt::viewer::make_default_viewer_scene(),
// remove the entire local make_final_room_scene() definition so the CLI
// and viewer share one final_room source of truth.
```

- [ ] **Step 3: Rebuild the CLI and viewer**

Run: `cmake --build build-clang-vcpkg-settings --target render_realtime render_realtime_viewer -j 4`
Expected: PASS

- [ ] **Step 4: Re-run the existing `final_room` CLI correctness test**

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_final_room_quality_cli --output-on-failure`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add utils/render_realtime.cpp src/realtime/viewer/default_viewer_scene.h src/realtime/viewer/default_viewer_scene.cpp
git commit -m "refactor: share default viewer scene with cli"
```

## Task 6: Add Viewer Documentation

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document the new default viewer**

````md
## GUI Viewer

For the default interactive mode, build and run:

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j
./bin/render_realtime_viewer
```

Behavior:

- starts in `final_room`
- shows four pinhole cameras in a `2x2` grid
- mouse controls body `yaw + pitch`
- `WASD` moves the body through the scene
````

- [ ] **Step 2: Re-read the README section in rendered plain text**

Run: `sed -n '1,120p' README.md`
Expected: the GUI section is concise and consistent with the viewer defaults

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: add gui viewer usage"
```

## Task 7: Final Verification

**Files:**
- No code changes

- [ ] **Step 1: Build all affected targets**

Run: `cmake --build build-clang-vcpkg-settings --target test_viewer_body_pose test_viewer_four_camera_rig render_realtime render_realtime_viewer -j 4`
Expected: PASS

- [ ] **Step 2: Run the focused automated tests**

Run: `ctest --test-dir build-clang-vcpkg-settings -R 'test_viewer_body_pose|test_viewer_four_camera_rig|test_render_realtime_final_room_quality_cli' --output-on-failure`
Expected: PASS

- [ ] **Step 3: Run the viewer manually**

Run: `./bin/render_realtime_viewer`
Expected: manual confirmation that the four panels update continuously and body navigation works as specified

- [ ] **Step 4: Record any manual-only limitations in the final handoff**

```text
If the viewer still uses CPU download -> OpenGL upload and no denoiser, state that explicitly in the handoff.
```

## Self-Review

Spec coverage:

- default interactive GUI executable: Task 4
- `final_room` as default scene: Tasks 2, 4, 5
- `2x2` equal viewer layout: Task 4
- body-controlled motion: Tasks 1 and 4
- four rigidly attached cameras with fixed yaw offsets: Task 3
- hardcoded spawn pose: Task 1
- keep batch CLI intact: Tasks 5 and 7

Placeholder scan:

- no `TODO` / `TBD`
- each task contains exact file paths
- code steps include concrete snippets
- build/test commands are explicit

Type consistency:

- viewer pose type is `rt::viewer::BodyPose`
- shared scene entrypoint is `rt::viewer::make_default_viewer_scene()`
- shared profile entrypoint is `rt::viewer::default_viewer_profile()`
- rig builder entrypoint is `rt::viewer::make_default_viewer_rig(...)`
