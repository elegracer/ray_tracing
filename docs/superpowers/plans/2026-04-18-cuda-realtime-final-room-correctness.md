# CUDA Realtime Final Room Correctness Scene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new `final_room` correctness scene and pinhole surround rig to `render_realtime` without changing the default smoke benchmark path.

**Architecture:** Keep the current smoke scene and smoke rig as the default CLI behavior, and add a separate correctness-only path selected by `--scene final_room`. Build the new room scene and its surround rig inside the realtime CLI layer using existing `SceneDescription` and `CameraRig` APIs, then add focused CLI verification that checks scene selection and output presence without turning image content into a brittle golden test.

**Tech Stack:** C++23, existing realtime `SceneDescription` / `CameraRig` APIs, argparse CLI flags, OpenCV PNG output, CTest/CMake script verification.

---

## File Structure

- Modify: `utils/render_realtime.cpp`
  - Add scene selection (`smoke|final_room`)
  - Add `final_room` scene builder
  - Add surround correctness rig builder
- Modify: `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`
  - Keep existing smoke profiling coverage
  - Allow verification of `final_room` selection metadata/output presence when explicitly requested
- Modify: `CMakeLists.txt`
  - Add a new CTest entry for the `final_room` correctness smoke run
- Modify: `README.md`
  - Document `--scene final_room` correctness usage briefly

## Task 1: Add CLI Scene Selection

**Files:**
- Modify: `utils/render_realtime.cpp`

- [ ] **Step 1: Add the failing CLI test hook in CMake script usage**

Update the verification contract so a future `final_room` test can request a scene name explicitly:

```cmake
# cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
if(NOT DEFINED PROFILE_NAME)
    set(PROFILE_NAME realtime)
endif()
if(NOT DEFINED SCENE_NAME)
    set(SCENE_NAME smoke)
endif()
```

Run: `ctest --test-dir build-clang-vcpkg-settings -R test_render_realtime_four_camera_profiling_cli -V`  
Expected: PASS before the CLI change, proving the verifier remains backward compatible

- [ ] **Step 2: Add the new `--scene` argument in the realtime CLI**

In `utils/render_realtime.cpp`, extend argument parsing:

```cpp
std::string scene_name = "smoke";

program.add_argument("--scene")
    .help("realtime scene: smoke|final_room")
    .default_value(scene_name)
    .store_into(scene_name);
```

and validate it:

```cpp
if (scene_name != "smoke" && scene_name != "final_room") {
    fmt::print(stderr, "--scene must be one of: smoke, final_room\n");
    return EXIT_FAILURE;
}
```

- [ ] **Step 3: Replace hardcoded smoke scene/rig construction with scene-selectable helpers**

Refactor the current direct calls:

```cpp
const rt::PackedScene packed_scene = make_smoke_scene().pack();
const rt::PackedCameraRig packed_rig = make_smoke_rig(camera_count).pack();
```

into:

```cpp
const rt::PackedScene packed_scene = make_scene(scene_name).pack();
const rt::PackedCameraRig packed_rig = make_rig(scene_name, camera_count).pack();
```

with temporary helper stubs that still route both names to the smoke implementations so the code compiles first.

- [ ] **Step 4: Verify green for the smoke path**

Run: `cmake --build build-clang-vcpkg-settings --target render_realtime -j 4`  
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add utils/render_realtime.cpp cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
git commit -m "feat: add realtime scene selection"
```

## Task 2: Build the `final_room` Correctness Scene

**Files:**
- Modify: `utils/render_realtime.cpp`

- [ ] **Step 1: Write the failing scene-selection verification**

Extend the verifier so it can require scene metadata indirectly through a successful `final_room` run and output presence:

```cmake
# cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
execute_process(
    COMMAND "${RENDER_REALTIME_EXE}"
        --scene "${SCENE_NAME}"
        --camera-count 4
        --frames 2
        --profile "${PROFILE_NAME}"
        --skip-image-write
        --output-dir "${OUTPUT_DIR}"
    ...
)
```

and add a new CTest later in Task 4 that runs `SCENE_NAME=final_room`.

Run: `./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-red`  
Expected: FAIL before `final_room` exists

- [ ] **Step 2: Add the `final_room` scene builder**

In `utils/render_realtime.cpp`, add a new helper:

```cpp
rt::SceneDescription make_final_room_scene() {
    rt::SceneDescription scene;

    const int white = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.73, 0.73, 0.73}});
    const int green = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.30, 0.70, 0.35}});
    const int red = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.72, 0.25, 0.22}});
    const int blue = scene.add_material(rt::LambertianMaterial {Eigen::Vector3d {0.25, 0.35, 0.75}});
    const int light = scene.add_material(rt::DiffuseLightMaterial {Eigen::Vector3d {12.0, 12.0, 12.0}});

    // Room shell
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, 3.5, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        green,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        red,
        Eigen::Vector3d {4.0, -1.0, -4.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        Eigen::Vector3d {0.0, 0.0, 8.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-4.0, -1.0, -4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        blue,
        Eigen::Vector3d {-4.0, -1.0, 4.0},
        Eigen::Vector3d {8.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 4.5, 0.0},
        false,
    });

    // Internal light
    scene.add_quad(rt::QuadPrimitive {
        light,
        Eigen::Vector3d {-1.0, 3.15, -1.0},
        Eigen::Vector3d {2.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 2.0},
        false,
    });

    // Floor volumes
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {-3.2, -0.25, -3.0},
        Eigen::Vector3d {1.8, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.8},
        false,
    });
    scene.add_quad(rt::QuadPrimitive {
        white,
        Eigen::Vector3d {1.2, 0.15, 1.0},
        Eigen::Vector3d {1.6, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 1.6},
        false,
    });
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {0.0, 0.1, 0.0}, 0.75, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-1.6, 0.35, 1.7}, 0.55, false});

    // Wall / ceiling content
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-3.1, 1.0, 0.8}, 0.55, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {3.0, 1.35, -0.9}, 0.65, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {1.1, 1.1, -3.0}, 0.60, false});
    scene.add_sphere(rt::SpherePrimitive {white, Eigen::Vector3d {-0.8, 2.55, 2.2}, 0.45, false});

    return scene;
}
```

Use the existing primitive set only. Keep object count modest and prefer larger readable forms over dense clutter.

- [ ] **Step 3: Route `make_scene()` to the new scene**

```cpp
rt::SceneDescription make_scene(const std::string& scene_name) {
    if (scene_name == "final_room") {
        return make_final_room_scene();
    }
    return make_smoke_scene();
}
```

- [ ] **Step 4: Verify the scene path compiles and runs**

Run: `./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-scene-check`  
Expected: PASS and PNGs emitted, even if the rig is still the old smoke rig

- [ ] **Step 5: Commit**

```bash
git add utils/render_realtime.cpp
git commit -m "feat: add final room correctness scene"
```

## Task 3: Add the Pinhole Surround Correctness Rig

**Files:**
- Modify: `utils/render_realtime.cpp`

- [ ] **Step 1: Write the failing rig behavior check through image emission**

Use the same manual correctness command as the red driver:

```bash
./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-rig-red
```

Expected: current images are too similar because the smoke rig still only uses translation offsets

- [ ] **Step 2: Add a dedicated `final_room` surround rig**

In `utils/render_realtime.cpp`, add:

```cpp
rt::CameraRig make_final_room_rig(int camera_count) {
    rt::CameraRig rig;
    const double fx = 0.75 * static_cast<double>(kDefaultWidth);
    const double fy = 0.75 * static_cast<double>(kDefaultHeight);
    const double cx = 0.5 * static_cast<double>(kDefaultWidth);
    const double cy = 0.5 * static_cast<double>(kDefaultHeight);

    const std::array<Eigen::Vector3d, 4> translations = {
        Eigen::Vector3d {0.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 0.0},
        Eigen::Vector3d {0.0, 0.0, 0.0},
    };
    const std::array<double, 4> yaw_deg = {0.0, 90.0, 180.0, -90.0};

    for (int i = 0; i < camera_count; ++i) {
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        T_bc.translation() = translations[static_cast<std::size_t>(i)];
        T_bc.linear() =
            Eigen::AngleAxisd(yaw_deg[static_cast<std::size_t>(i)] * M_PI / 180.0, Eigen::Vector3d::UnitZ()).toRotationMatrix();
        rig.add_pinhole(rt::Pinhole32Params {fx, fy, cx, cy, 0.0, 0.0, 0.0, 0.0, 0.0},
            T_bc, kDefaultWidth, kDefaultHeight);
    }
    return rig;
}
```

Adjust the rotation axis/signs as needed to match the repo’s SLAM/body-to-camera convention, but keep the intent: four distinct surround look directions with fixed body-relative extrinsics.

- [ ] **Step 3: Route `make_rig()` by scene**

```cpp
rt::CameraRig make_rig(const std::string& scene_name, int camera_count) {
    if (scene_name == "final_room") {
        return make_final_room_rig(camera_count);
    }
    return make_smoke_rig(camera_count);
}
```

- [ ] **Step 4: Verify the four views are emitted with deterministic ordering**

Run: `./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-rig-check`  
Expected: files `frame_0000_cam_0.png` through `frame_0000_cam_3.png` exist and differ visibly on inspection

- [ ] **Step 5: Commit**

```bash
git add utils/render_realtime.cpp
git commit -m "feat: add final room surround rig"
```

## Task 4: Add Automated Correctness CLI Coverage

**Files:**
- Modify: `cmake/VerifyRenderRealtimeFourCameraProfiling.cmake`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

- [ ] **Step 1: Add a dedicated `final_room` CLI verification test**

In `CMakeLists.txt`, add:

```cmake
add_test(NAME test_render_realtime_final_room_quality_cli
    COMMAND ${CMAKE_COMMAND}
        -DRENDER_REALTIME_EXE=$<TARGET_FILE:render_realtime>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/test/render_realtime-final-room
        -DPROFILE_NAME=quality
        -DSCENE_NAME=final_room
        -DEXPECT_DENOISE_ENABLED=OFF
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRenderRealtimeFourCameraProfiling.cmake
)
```

- [ ] **Step 2: Extend the verifier for `final_room` correctness coverage**

Add only lightweight assertions:

```cmake
if(NOT metadata_profile STREQUAL PROFILE_NAME)
    message(FATAL_ERROR ...)
endif()
if(NOT metadata_denoise_enabled STREQUAL "false")
    message(FATAL_ERROR ...)
endif()
file(GLOB emitted_pngs "${OUTPUT_DIR}/*.png")
if(SCENE_NAME STREQUAL "final_room")
    list(LENGTH emitted_pngs emitted_png_count)
    if(NOT emitted_png_count EQUAL 0)
        message(FATAL_ERROR "final_room verification should stay in skip-write mode")
    endif()
endif()
```

Keep this verification focused on successful rendering, metadata consistency, and deterministic per-camera ordering. Do not add brittle image-content matching.

- [ ] **Step 3: Document the new correctness scene**

In `README.md`, add a brief section or note:

```md
For visual correctness checks, render the enclosed `final_room` scene:

    ./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-check
```

- [ ] **Step 4: Verify green**

Run:

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j 4
ctest --test-dir build-clang-vcpkg-settings -R 'test_render_realtime_four_camera_profiling_cli|test_render_realtime_four_camera_quality_cli|test_render_realtime_final_room_quality_cli|test_render_realtime_cli_skip_write' -V
./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-check
```

Expected:

- all listed CTests pass
- `build/final-room-check/frame_0000_cam_0.png` through `frame_0000_cam_3.png` exist
- smoke profiling tests continue to use the default smoke scene

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake/VerifyRenderRealtimeFourCameraProfiling.cmake README.md
git commit -m "test: cover final room correctness scene"
```

## Self-Review

Spec coverage:

- CLI-selectable `final_room`: Tasks 1 and 2
- enclosed room with floor / walls / ceiling / internal light: Task 2
- pinhole four-camera surround rig: Task 3
- smoke remains default benchmark path: Tasks 1 and 4
- lightweight correctness verification only: Task 4

Placeholder scan:

- no `TODO` / `TBD`
- each task names exact files and commands
- red/green commands are explicit

Type consistency:

- `make_scene(scene_name)` returns `SceneDescription`
- `make_rig(scene_name, camera_count)` returns `CameraRig`
- CLI keeps using `PackedScene` / `PackedCameraRig` after `pack()`
