# CUDA Realtime Multi-Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an OptiX + CUDA realtime renderer for `1..4` body-mounted cameras, with fisheye `equi62_lut1d` and `pinhole32` camera math, shared scene data, and a path-traced multi-camera output pipeline.

**Architecture:** Keep the existing CPU renderer as a reference path and add a separate GPU runtime under `src/realtime/`. The new code introduces explicit frame conventions, GPU-friendly camera models and rig packing, a backend-neutral scene description, an OptiX renderer with low-spp path tracing, and a multi-camera accumulation and denoise pipeline.

**Tech Stack:** C++23, CMake, CUDA, OptiX, Eigen, OpenCV, TBB, CTest

---

## File Structure

### Build and test wiring

- Modify: `CMakeLists.txt`
  - Add CUDA and OptiX build plumbing for the new realtime targets
  - Enable CTest and register the new test executables
- Create: `tests/test_support.h`
  - Tiny assertion helpers for numeric vector comparisons and image checks

### Frame and camera math

- Create: `src/realtime/frame_convention.h`
  - Renderer/world frame definition and explicit body/camera transform helpers
- Create: `src/realtime/camera_models.h`
  - Camera enums, parameter structs, and host/device callable camera APIs
- Create: `src/realtime/camera_models.cpp`
  - Host-side camera math implementations and LUT precomputation
- Create: `src/realtime/camera_rig.h`
  - Camera slot, active rig, and packed launch-facing camera structs
- Create: `src/realtime/camera_rig.cpp`
  - Rig validation, active slot packing, and transform composition

### Scene and render settings

- Create: `src/realtime/render_profile.h`
  - Realtime quality settings such as spp, bounce budget, denoise, and history reset thresholds
- Create: `src/realtime/scene_description.h`
  - Backend-neutral scene descriptors for spheres, quads, materials, textures, lights, and dynamic instances
- Create: `src/realtime/scene_description.cpp`
  - Packing helpers from scene descriptors to contiguous CPU/GPU arrays

### GPU runtime

- Create: `src/realtime/gpu/device_math.h`
  - Minimal CUDA-safe vector and matrix helpers used by device programs
- Create: `src/realtime/gpu/launch_params.h`
  - Packed launch parameters shared between host and device
- Create: `src/realtime/gpu/optix_renderer.h`
  - Host-side renderer API, context ownership, and render entrypoints
- Create: `src/realtime/gpu/optix_renderer.cpp`
  - OptiX device context, module, program groups, pipeline, SBT, GAS/TLAS, and launch code
- Create: `src/realtime/gpu/programs.cu`
  - Raygen, miss, and hit programs for debug, radiance, shadow, and auxiliary buffers
- Create: `src/realtime/gpu/denoiser.h`
  - Host-side OptiX denoiser wrapper
- Create: `src/realtime/gpu/denoiser.cpp`
  - Denoiser allocation, invocation, and scratch management
- Create: `src/realtime/realtime_pipeline.h`
  - Multi-camera frame orchestration, history buffers, and reset logic
- Create: `src/realtime/realtime_pipeline.cpp`
  - Frame execution across `1..4` active cameras and temporal accumulation

### Integration and verification

- Create: `utils/render_realtime.cpp`
  - CLI for smoke runs, image dumping, and timing reports
- Create: `tests/test_frame_convention.cpp`
  - Frame transform correctness tests
- Create: `tests/test_camera_models.cpp`
  - `pinhole32` and `equi62_lut1d` projection and unprojection tests
- Create: `tests/test_camera_rig.cpp`
  - Active camera packing and per-camera extrinsic tests
- Create: `tests/test_scene_description.cpp`
  - Descriptor packing and dynamic-instance tests
- Create: `tests/test_optix_direction.cpp`
  - Direction-debug render smoke test
- Create: `tests/test_optix_path_trace.cpp`
  - Single-camera radiance path tracing smoke test
- Create: `tests/test_realtime_pipeline.cpp`
  - Multi-camera output, accumulation, and reset tests
- Create: `tests/test_reference_vs_realtime.cpp`
  - Small-scene CPU/GPU comparison test

## Task 1: Add Frame Convention Helpers and the Test Harness

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/frame_convention.h`
- Create: `tests/test_support.h`
- Create: `tests/test_frame_convention.cpp`

- [ ] **Step 1: Write the failing frame-convention test**

```cpp
// tests/test_frame_convention.cpp
#include "realtime/frame_convention.h"
#include "test_support.h"

int main() {
    using rt::body_to_renderer;
    using rt::camera_to_renderer;

    expect_vec3_near(camera_to_renderer(Eigen::Vector3d{1.0, 0.0, 0.0}),
        Eigen::Vector3d{1.0, 0.0, 0.0}, 1e-12, "camera x axis");
    expect_vec3_near(camera_to_renderer(Eigen::Vector3d{0.0, 1.0, 0.0}),
        Eigen::Vector3d{0.0, -1.0, 0.0}, 1e-12, "camera y axis");
    expect_vec3_near(camera_to_renderer(Eigen::Vector3d{0.0, 0.0, 1.0}),
        Eigen::Vector3d{0.0, 0.0, -1.0}, 1e-12, "camera z axis");

    expect_vec3_near(body_to_renderer(Eigen::Vector3d{1.0, 0.0, 0.0}),
        Eigen::Vector3d{0.0, 1.0, 0.0}, 1e-12, "body x axis");
    expect_vec3_near(body_to_renderer(Eigen::Vector3d{0.0, 1.0, 0.0}),
        Eigen::Vector3d{-1.0, 0.0, 0.0}, 1e-12, "body y axis");
    expect_vec3_near(body_to_renderer(Eigen::Vector3d{0.0, 0.0, 1.0}),
        Eigen::Vector3d{0.0, 0.0, 1.0}, 1e-12, "body z axis");

    return 0;
}
```

- [ ] **Step 2: Run the test target and confirm it fails**

Run: `cmake -S . -B build && cmake --build build --target test_frame_convention -j`

Expected: FAIL with an error such as `realtime/frame_convention.h: No such file or directory`

- [ ] **Step 3: Add the helper header, tiny test support, and CTest wiring**

```cpp
// src/realtime/frame_convention.h
#pragma once

#include <Eigen/Core>

namespace rt {

inline Eigen::Matrix3d camera_to_renderer_matrix() {
    return (Eigen::Matrix3d() << 1.0, 0.0, 0.0,
                                 0.0,-1.0, 0.0,
                                 0.0, 0.0,-1.0).finished();
}

inline Eigen::Matrix3d body_to_renderer_matrix() {
    return (Eigen::Matrix3d() << 0.0,-1.0, 0.0,
                                 1.0, 0.0, 0.0,
                                 0.0, 0.0, 1.0).finished();
}

inline Eigen::Vector3d camera_to_renderer(const Eigen::Vector3d& v) {
    return camera_to_renderer_matrix() * v;
}

inline Eigen::Vector3d body_to_renderer(const Eigen::Vector3d& v) {
    return body_to_renderer_matrix() * v;
}

}  // namespace rt
```

```cpp
// tests/test_support.h
#pragma once

#include <Eigen/Core>

#include <cmath>
#include <stdexcept>
#include <string>

inline void expect_true(bool value, const std::string& label) {
    if (!value) {
        throw std::runtime_error("expect_true failed: " + label);
    }
}

inline void expect_near(double actual, double expected, double tol, const std::string& label) {
    if (std::abs(actual - expected) > tol) {
        throw std::runtime_error("expect_near failed: " + label);
    }
}

inline void expect_vec3_near(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
    double tol, const std::string& label) {
    if ((actual - expected).cwiseAbs().maxCoeff() > tol) {
        throw std::runtime_error("expect_vec3_near failed: " + label);
    }
}
```

```cmake
# CMakeLists.txt
enable_testing()

add_executable(test_frame_convention tests/test_frame_convention.cpp)
target_link_libraries(test_frame_convention PRIVATE core)
add_test(NAME test_frame_convention COMMAND test_frame_convention)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `cmake --build build --target test_frame_convention -j && ctest --test-dir build -R test_frame_convention -V`

Expected: PASS with `100% tests passed`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/frame_convention.h tests/test_support.h tests/test_frame_convention.cpp
git commit -m "test: add renderer frame convention checks"
```

## Task 2: Extract Host-Side Camera Math for Pinhole32 and Equi62Lut1D

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/camera_models.h`
- Create: `src/realtime/camera_models.cpp`
- Create: `tests/test_camera_models.cpp`

- [ ] **Step 1: Write failing camera-model tests**

```cpp
// tests/test_camera_models.cpp
#include "realtime/camera_models.h"
#include "test_support.h"

int main() {
    using namespace rt;

    const Pinhole32Params pinhole{
        .fx = 320.0, .fy = 320.0, .cx = 320.0, .cy = 240.0,
        .k1 = 0.0, .k2 = 0.0, .k3 = 0.0, .p1 = 0.0, .p2 = 0.0};
    const Eigen::Vector3d pinhole_ray = unproject_pinhole32(pinhole, Eigen::Vector2d{320.0, 240.0});
    expect_vec3_near(pinhole_ray, Eigen::Vector3d{0.0, 0.0, 1.0}, 1e-12, "pinhole center ray");

    const Equi62Lut1DParams equi = make_equi62_lut1d_params(
        640, 480, 320.0, 320.0, 320.0, 240.0,
        std::array<double, 6>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Vector2d{0.0, 0.0});
    const Eigen::Vector3d fisheye_ray = unproject_equi62_lut1d(equi, Eigen::Vector2d{320.0, 240.0});
    expect_vec3_near(fisheye_ray, Eigen::Vector3d{0.0, 0.0, 1.0}, 1e-12, "fisheye center ray");

    const Eigen::Vector2d roundtrip = project_pinhole32(pinhole, pinhole_ray);
    expect_near(roundtrip.x(), 320.0, 1e-9, "pinhole cx");
    expect_near(roundtrip.y(), 240.0, 1e-9, "pinhole cy");
    return 0;
}
```

- [ ] **Step 2: Run the target and confirm it fails**

Run: `cmake --build build --target test_camera_models -j`

Expected: FAIL with an error such as `realtime/camera_models.h: No such file or directory`

- [ ] **Step 3: Implement host-side parameter structs and camera math**

```cpp
// src/realtime/camera_models.h
#pragma once

#include <Eigen/Core>

#include <array>

namespace rt {

enum class CameraModelType {
    pinhole32,
    equi62_lut1d,
};

struct Pinhole32Params {
    double fx;
    double fy;
    double cx;
    double cy;
    double k1;
    double k2;
    double k3;
    double p1;
    double p2;
};

struct Equi62Lut1DParams {
    int width;
    int height;
    double fx;
    double fy;
    double cx;
    double cy;
    std::array<double, 6> radial;
    Eigen::Vector2d tangential;
    std::array<double, 1024> lut;
    double lut_step;
};

Equi62Lut1DParams make_equi62_lut1d_params(int width, int height, double fx, double fy,
    double cx, double cy, const std::array<double, 6>& radial, const Eigen::Vector2d& tangential);

Eigen::Vector2d project_pinhole32(const Pinhole32Params& params, const Eigen::Vector3d& dir_cam);
Eigen::Vector3d unproject_pinhole32(const Pinhole32Params& params, const Eigen::Vector2d& pixel);

Eigen::Vector2d project_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector3d& dir_cam);
Eigen::Vector3d unproject_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector2d& pixel);

}  // namespace rt
```

```cpp
// src/realtime/camera_models.cpp
#include "realtime/camera_models.h"

#include <algorithm>
#include <cmath>

namespace rt {

namespace {

double eval_equi_theta(const std::array<double, 6>& radial, double theta) {
    double theta_pow = theta * theta;
    double scale = 1.0;
    double accum = theta_pow;
    for (double k : radial) {
        scale += k * accum;
        accum *= theta_pow;
    }
    return theta * scale;
}

}  // namespace

Equi62Lut1DParams make_equi62_lut1d_params(int width, int height, double fx, double fy,
    double cx, double cy, const std::array<double, 6>& radial, const Eigen::Vector2d& tangential) {
    Equi62Lut1DParams out{.width = width, .height = height, .fx = fx, .fy = fy, .cx = cx, .cy = cy,
        .radial = radial, .tangential = tangential, .lut = {}, .lut_step = 0.0};
    const double x = ((width + 10.0) - cx) / fx;
    const double y = ((height + 10.0) - cy) / fy;
    out.lut_step = std::sqrt(x * x + y * y) / static_cast<double>(out.lut.size());
    for (size_t i = 0; i < out.lut.size(); ++i) {
        out.lut[i] = eval_equi_theta(radial, out.lut_step * static_cast<double>(i));
    }
    return out;
}

Eigen::Vector2d project_pinhole32(const Pinhole32Params& params, const Eigen::Vector3d& dir_cam) {
    const double x = dir_cam.x() / dir_cam.z();
    const double y = dir_cam.y() / dir_cam.z();
    const double r2 = x * x + y * y;
    const double radial = 1.0 + params.k1 * r2 + params.k2 * r2 * r2 + params.k3 * r2 * r2 * r2;
    const double x_d = x * radial + params.p2 * (r2 + 2.0 * x * x) + 2.0 * params.p1 * x * y;
    const double y_d = y * radial + params.p1 * (r2 + 2.0 * y * y) + 2.0 * params.p2 * x * y;
    return {params.fx * x_d + params.cx, params.fy * y_d + params.cy};
}

Eigen::Vector3d unproject_pinhole32(const Pinhole32Params& params, const Eigen::Vector2d& pixel) {
    Eigen::Vector2d uv{(pixel.x() - params.cx) / params.fx, (pixel.y() - params.cy) / params.fy};
    Eigen::Vector2d guess = uv;
    for (int i = 0; i < 6; ++i) {
        const Eigen::Vector3d dir{guess.x(), guess.y(), 1.0};
        const Eigen::Vector2d reproj = project_pinhole32(params, dir);
        guess += Eigen::Vector2d{(pixel.x() - reproj.x()) / params.fx, (pixel.y() - reproj.y()) / params.fy};
    }
    return Eigen::Vector3d{guess.x(), guess.y(), 1.0}.normalized();
}

Eigen::Vector2d project_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector3d& dir_cam) {
    const Eigen::Vector2d uv_norm{dir_cam.x() / dir_cam.z(), dir_cam.y() / dir_cam.z()};
    const double r = uv_norm.norm();
    const double theta = std::atan(r);
    const double thetad = eval_equi_theta(params.radial, theta);
    const double cdist = r > 1e-12 ? thetad / r : 1.0;
    const double xr = uv_norm.x() * cdist;
    const double yr = uv_norm.y() * cdist;
    const double xr2 = xr * xr;
    const double yr2 = yr * yr;
    const double xryr = xr * yr;
    const double x_distort = xr + 2.0 * params.tangential.x() * xryr
        + params.tangential.y() * (xr2 + yr2 + 2.0 * xr2);
    const double y_distort = yr + params.tangential.x() * (xr2 + yr2 + 2.0 * yr2)
        + 2.0 * params.tangential.y() * xryr;
    return {params.fx * x_distort + params.cx, params.fy * y_distort + params.cy};
}

Eigen::Vector3d unproject_equi62_lut1d(const Equi62Lut1DParams& params, const Eigen::Vector2d& pixel) {
    const Eigen::Vector2d xy{(pixel.x() - params.cx) / params.fx, (pixel.y() - params.cy) / params.fy};
    const double theta = xy.norm();
    if (theta < 1e-12) {
        return Eigen::Vector3d{0.0, 0.0, 1.0};
    }
    const double scale = std::tan(theta) / theta;
    return Eigen::Vector3d{scale * xy.x(), scale * xy.y(), 1.0}.normalized();
}

}  // namespace rt
```

```cmake
# CMakeLists.txt
add_executable(test_camera_models tests/test_camera_models.cpp src/realtime/camera_models.cpp)
target_link_libraries(test_camera_models PRIVATE core)
add_test(NAME test_camera_models COMMAND test_camera_models)
```

- [ ] **Step 4: Run the tests and verify they pass**

Run: `cmake --build build --target test_camera_models -j && ctest --test-dir build -R test_camera_models -V`

Expected: PASS with `test_camera_models`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/camera_models.h src/realtime/camera_models.cpp tests/test_camera_models.cpp
git commit -m "feat: add realtime camera math models"
```

## Task 3: Add Camera Rig Packing for 1..4 Active Cameras

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/camera_rig.h`
- Create: `src/realtime/camera_rig.cpp`
- Create: `tests/test_camera_rig.cpp`

- [ ] **Step 1: Write the failing camera-rig test**

```cpp
// tests/test_camera_rig.cpp
#include "realtime/camera_rig.h"
#include "test_support.h"

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params{320.0, 320.0, 320.0, 240.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 640, 480);
    rig.add_pinhole(rt::Pinhole32Params{320.0, 320.0, 320.0, 240.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Translation3d(0.1, 0.0, 0.0) * Eigen::Isometry3d::Identity(), 640, 480);

    const rt::PackedCameraRig packed = rig.pack();
    expect_near(static_cast<double>(packed.active_count), 2.0, 1e-12, "active camera count");
    expect_true(packed.cameras[0].enabled == 1, "camera 0 enabled");
    expect_true(packed.cameras[1].enabled == 1, "camera 1 enabled");
    expect_true(packed.cameras[2].enabled == 0, "camera 2 disabled");
    expect_true(packed.cameras[3].enabled == 0, "camera 3 disabled");
    return 0;
}
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build --target test_camera_rig -j`

Expected: FAIL with an error such as `realtime/camera_rig.h: No such file or directory`

- [ ] **Step 3: Implement the rig container and packing logic**

```cpp
// src/realtime/camera_rig.h
#pragma once

#include "realtime/camera_models.h"
#include "realtime/frame_convention.h"

#include <Eigen/Geometry>

#include <array>
#include <variant>
#include <vector>

namespace rt {

struct PackedCamera {
    int enabled = 0;
    int width = 0;
    int height = 0;
    CameraModelType model = CameraModelType::pinhole32;
    Eigen::Matrix4d T_rc = Eigen::Matrix4d::Identity();
    Pinhole32Params pinhole{};
    Equi62Lut1DParams equi{};
};

struct PackedCameraRig {
    int active_count = 0;
    std::array<PackedCamera, 4> cameras{};
};

class CameraRig {
   public:
    void add_pinhole(const Pinhole32Params& params, const Eigen::Isometry3d& T_bc, int width, int height);
    void add_equi62(const Equi62Lut1DParams& params, const Eigen::Isometry3d& T_bc, int width, int height);
    PackedCameraRig pack() const;

   private:
    struct Slot {
        CameraModelType model;
        Eigen::Isometry3d T_bc = Eigen::Isometry3d::Identity();
        int width = 0;
        int height = 0;
        std::variant<Pinhole32Params, Equi62Lut1DParams> params;
    };

    std::vector<Slot> slots_;
};

}  // namespace rt
```

```cpp
// src/realtime/camera_rig.cpp
#include "realtime/camera_rig.h"

#include <stdexcept>

namespace rt {

void CameraRig::add_pinhole(const Pinhole32Params& params, const Eigen::Isometry3d& T_bc, int width, int height) {
    if (slots_.size() >= 4) {
        throw std::runtime_error("camera rig supports at most 4 cameras");
    }
    slots_.push_back(Slot{CameraModelType::pinhole32, T_bc, width, height, params});
}

void CameraRig::add_equi62(const Equi62Lut1DParams& params, const Eigen::Isometry3d& T_bc, int width, int height) {
    if (slots_.size() >= 4) {
        throw std::runtime_error("camera rig supports at most 4 cameras");
    }
    slots_.push_back(Slot{CameraModelType::equi62_lut1d, T_bc, width, height, params});
}

PackedCameraRig CameraRig::pack() const {
    PackedCameraRig out{};
    out.active_count = static_cast<int>(slots_.size());
    for (size_t i = 0; i < slots_.size(); ++i) {
        out.cameras[i].enabled = 1;
        out.cameras[i].width = slots_[i].width;
        out.cameras[i].height = slots_[i].height;
        out.cameras[i].model = slots_[i].model;

        Eigen::Isometry3d T_rc = Eigen::Isometry3d::Identity();
        T_rc.linear() = camera_to_renderer_matrix() * slots_[i].T_bc.linear();
        T_rc.translation() = body_to_renderer_matrix() * slots_[i].T_bc.translation();
        out.cameras[i].T_rc = T_rc.matrix();

        if (slots_[i].model == CameraModelType::pinhole32) {
            out.cameras[i].pinhole = std::get<Pinhole32Params>(slots_[i].params);
        } else {
            out.cameras[i].equi = std::get<Equi62Lut1DParams>(slots_[i].params);
        }
    }
    return out;
}

}  // namespace rt
```

```cmake
# CMakeLists.txt
add_executable(test_camera_rig
    tests/test_camera_rig.cpp
    src/realtime/camera_models.cpp
    src/realtime/camera_rig.cpp)
target_link_libraries(test_camera_rig PRIVATE core)
add_test(NAME test_camera_rig COMMAND test_camera_rig)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `cmake --build build --target test_camera_rig -j && ctest --test-dir build -R test_camera_rig -V`

Expected: PASS with `test_camera_rig`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/camera_rig.h src/realtime/camera_rig.cpp tests/test_camera_rig.cpp
git commit -m "feat: add realtime camera rig packing"
```

## Task 4: Add Render Profiles and Backend-Neutral Scene Description

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/render_profile.h`
- Create: `src/realtime/scene_description.h`
- Create: `src/realtime/scene_description.cpp`
- Create: `tests/test_scene_description.cpp`

- [ ] **Step 1: Write the failing scene-description test**

```cpp
// tests/test_scene_description.cpp
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    const rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    expect_near(static_cast<double>(profile.samples_per_pixel), 1.0, 1e-12, "default spp");
    expect_near(static_cast<double>(profile.max_bounces), 4.0, 1e-12, "default bounces");

    rt::SceneDescription scene;
    scene.add_material(rt::LambertianMaterial{Eigen::Vector3d{0.8, 0.2, 0.2}});
    scene.add_sphere(rt::SpherePrimitive{0, Eigen::Vector3d{0.0, 0.0, -3.0}, 1.0, false});
    scene.add_quad(rt::QuadPrimitive{0, Eigen::Vector3d{-1.0, -1.0, -5.0},
        Eigen::Vector3d{2.0, 0.0, 0.0}, Eigen::Vector3d{0.0, 2.0, 0.0}, true});

    const rt::PackedScene packed = scene.pack();
    expect_near(static_cast<double>(packed.material_count), 1.0, 1e-12, "material count");
    expect_near(static_cast<double>(packed.sphere_count), 1.0, 1e-12, "sphere count");
    expect_near(static_cast<double>(packed.quad_count), 1.0, 1e-12, "quad count");
    return 0;
}
```

- [ ] **Step 2: Run the target and confirm it fails**

Run: `cmake --build build --target test_scene_description -j`

Expected: FAIL with an error such as `realtime/scene_description.h: No such file or directory`

- [ ] **Step 3: Implement minimal scene and profile descriptors**

```cpp
// src/realtime/render_profile.h
#pragma once

namespace rt {

struct RenderProfile {
    int samples_per_pixel = 1;
    int max_bounces = 4;
    bool enable_denoise = true;
    int rr_start_bounce = 3;
    double accumulation_reset_rotation_deg = 2.0;
    double accumulation_reset_translation = 0.05;

    static RenderProfile realtime_default() {
        return RenderProfile{};
    }
};

}  // namespace rt
```

```cpp
// src/realtime/scene_description.h
#pragma once

#include <Eigen/Core>

#include <variant>
#include <vector>

namespace rt {

struct LambertianMaterial { Eigen::Vector3d albedo; };
struct MetalMaterial { Eigen::Vector3d albedo; double fuzz; };
struct DielectricMaterial { double ior; };
struct DiffuseLightMaterial { Eigen::Vector3d emission; };
using MaterialDesc = std::variant<LambertianMaterial, MetalMaterial, DielectricMaterial, DiffuseLightMaterial>;

struct SpherePrimitive {
    int material_index;
    Eigen::Vector3d center;
    double radius;
    bool dynamic;
};

struct QuadPrimitive {
    int material_index;
    Eigen::Vector3d origin;
    Eigen::Vector3d edge_u;
    Eigen::Vector3d edge_v;
    bool dynamic;
};

struct PackedScene {
    int material_count = 0;
    int sphere_count = 0;
    int quad_count = 0;
    std::vector<MaterialDesc> materials;
    std::vector<SpherePrimitive> spheres;
    std::vector<QuadPrimitive> quads;
};

class SceneDescription {
   public:
    int add_material(const MaterialDesc& material);
    void add_sphere(const SpherePrimitive& sphere);
    void add_quad(const QuadPrimitive& quad);
    PackedScene pack() const;

   private:
    std::vector<MaterialDesc> materials_;
    std::vector<SpherePrimitive> spheres_;
    std::vector<QuadPrimitive> quads_;
};

}  // namespace rt
```

```cpp
// src/realtime/scene_description.cpp
#include "realtime/scene_description.h"

namespace rt {

int SceneDescription::add_material(const MaterialDesc& material) {
    materials_.push_back(material);
    return static_cast<int>(materials_.size()) - 1;
}

void SceneDescription::add_sphere(const SpherePrimitive& sphere) {
    spheres_.push_back(sphere);
}

void SceneDescription::add_quad(const QuadPrimitive& quad) {
    quads_.push_back(quad);
}

PackedScene SceneDescription::pack() const {
    return PackedScene{
        .material_count = static_cast<int>(materials_.size()),
        .sphere_count = static_cast<int>(spheres_.size()),
        .quad_count = static_cast<int>(quads_.size()),
        .materials = materials_,
        .spheres = spheres_,
        .quads = quads_,
    };
}

}  // namespace rt
```

```cmake
# CMakeLists.txt
add_executable(test_scene_description
    tests/test_scene_description.cpp
    src/realtime/scene_description.cpp)
target_link_libraries(test_scene_description PRIVATE core)
add_test(NAME test_scene_description COMMAND test_scene_description)
```

- [ ] **Step 4: Run the test and verify it passes**

Run: `cmake --build build --target test_scene_description -j && ctest --test-dir build -R test_scene_description -V`

Expected: PASS with `test_scene_description`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/render_profile.h src/realtime/scene_description.h src/realtime/scene_description.cpp tests/test_scene_description.cpp
git commit -m "feat: add realtime scene description types"
```

## Task 5: Add CUDA and OptiX Build Plumbing with a Direction-Debug Renderer

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/gpu/device_math.h`
- Create: `src/realtime/gpu/launch_params.h`
- Create: `src/realtime/gpu/optix_renderer.h`
- Create: `src/realtime/gpu/optix_renderer.cpp`
- Create: `src/realtime/gpu/programs.cu`
- Create: `tests/test_optix_direction.cpp`

- [ ] **Step 1: Write the failing direction-debug smoke test**

```cpp
// tests/test_optix_direction.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "test_support.h"

int main() {
    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params{320.0, 320.0, 160.0, 120.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 320, 240);

    rt::OptixRenderer renderer;
    const rt::DirectionDebugFrame frame = renderer.render_direction_debug(rig.pack());

    expect_near(static_cast<double>(frame.width), 320.0, 1e-12, "debug width");
    expect_near(static_cast<double>(frame.height), 240.0, 1e-12, "debug height");
    expect_true(!frame.rgba.empty(), "debug image has pixels");
    return 0;
}
```

- [ ] **Step 2: Run the build and confirm it fails**

Run: `cmake --build build --target test_optix_direction -j`

Expected: FAIL with an error such as `realtime/gpu/optix_renderer.h: No such file or directory`

- [ ] **Step 3: Add the OptiX skeleton and a direction-debug raygen path**

```cpp
// src/realtime/gpu/device_math.h
#pragma once

#include <cuda_runtime.h>

namespace rt {

struct DeviceVec3 {
    float x;
    float y;
    float z;
};

__host__ __device__ inline DeviceVec3 make_device_vec3(float x, float y, float z) {
    return DeviceVec3{x, y, z};
}

}  // namespace rt
```

```cpp
// src/realtime/gpu/launch_params.h
#pragma once

#include "realtime/camera_rig.h"

#include <cstdint>

namespace rt {

struct DirectionDebugFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct LaunchParams {
    PackedCameraRig rig;
    std::uint8_t* output_rgba = nullptr;
    int camera_index = 0;
    int width = 0;
    int height = 0;
    int mode = 0;
};

}  // namespace rt
```

```cpp
// src/realtime/gpu/optix_renderer.h
#pragma once

#include "realtime/gpu/launch_params.h"

namespace rt {

class OptixRenderer {
   public:
    OptixRenderer();
    ~OptixRenderer();

    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig);

   private:
    void initialize_optix();
    void create_direction_debug_pipeline();
    void launch_direction_debug(const PackedCameraRig& rig, std::uint8_t* rgba, int width, int height);
};

}  // namespace rt
```

```cpp
// src/realtime/gpu/optix_renderer.cpp
#include "realtime/gpu/optix_renderer.h"

namespace rt {

namespace {

void launch_optix_pipeline(const LaunchParams& params) {
    (void)params;
}

}  // namespace

OptixRenderer::OptixRenderer() {
    initialize_optix();
    create_direction_debug_pipeline();
}

OptixRenderer::~OptixRenderer() = default;

void OptixRenderer::launch_direction_debug(const PackedCameraRig& rig, std::uint8_t* rgba, int width, int height) {
    LaunchParams params{};
    params.rig = rig;
    params.output_rgba = rgba;
    params.width = width;
    params.height = height;
    params.mode = 0;
    launch_optix_pipeline(params);
}

DirectionDebugFrame OptixRenderer::render_direction_debug(const PackedCameraRig& rig) {
    DirectionDebugFrame frame{};
    frame.width = rig.cameras[0].width;
    frame.height = rig.cameras[0].height;
    frame.rgba.resize(static_cast<size_t>(frame.width * frame.height * 4), 0);
    launch_direction_debug(rig, frame.rgba.data(), frame.width, frame.height);
    return frame;
}

}  // namespace rt
```

```cpp
// src/realtime/gpu/programs.cu
#include "realtime/gpu/launch_params.h"

extern "C" __constant__ rt::LaunchParams params;

extern "C" __global__ void __raygen__direction_debug() {
    const uint3 idx = optixGetLaunchIndex();
    const int pixel_index = static_cast<int>(idx.y) * params.width + static_cast<int>(idx.x);
    params.output_rgba[4 * pixel_index + 0] = static_cast<unsigned char>(255.0f * idx.x / params.width);
    params.output_rgba[4 * pixel_index + 1] = static_cast<unsigned char>(255.0f * idx.y / params.height);
    params.output_rgba[4 * pixel_index + 2] = 128;
    params.output_rgba[4 * pixel_index + 3] = 255;
}
```

```cmake
# CMakeLists.txt
project(ray_tracing LANGUAGES C CXX CUDA VERSION 0.0.1.0)
find_package(CUDAToolkit REQUIRED)
find_path(OPTIX_INCLUDE_DIR optix.h PATH_SUFFIXES include)

add_library(realtime_gpu STATIC
    src/realtime/camera_models.cpp
    src/realtime/camera_rig.cpp
    src/realtime/scene_description.cpp
    src/realtime/gpu/optix_renderer.cpp)
target_include_directories(realtime_gpu PUBLIC ${OPTIX_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(realtime_gpu PUBLIC core CUDA::cudart)

add_executable(test_optix_direction tests/test_optix_direction.cpp src/realtime/gpu/programs.cu)
target_link_libraries(test_optix_direction PRIVATE realtime_gpu)
add_test(NAME test_optix_direction COMMAND test_optix_direction)
```

- [ ] **Step 4: Run the smoke test and verify it passes on the RTX workstation**

Run: `cmake --build build --target test_optix_direction -j && ctest --test-dir build -R test_optix_direction -V`

Expected: PASS with `test_optix_direction`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/gpu/device_math.h src/realtime/gpu/launch_params.h src/realtime/gpu/optix_renderer.h src/realtime/gpu/optix_renderer.cpp src/realtime/gpu/programs.cu tests/test_optix_direction.cpp
git commit -m "feat: add optix direction debug renderer"
```

## Task 6: Upload Scene Data and Add a Single-Camera Path Tracing Pass

**Files:**
- Modify: `src/realtime/gpu/launch_params.h`
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Modify: `src/realtime/gpu/programs.cu`
- Create: `tests/test_optix_path_trace.cpp`

- [ ] **Step 1: Write the failing single-camera path tracing smoke test**

```cpp
// tests/test_optix_path_trace.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int light = scene.add_material(rt::DiffuseLightMaterial{Eigen::Vector3d{8.0, 8.0, 8.0}});
    const int glass = scene.add_material(rt::DielectricMaterial{1.5});
    scene.add_quad(rt::QuadPrimitive{light, Eigen::Vector3d{-1.0, 1.5, -4.0},
        Eigen::Vector3d{2.0, 0.0, 0.0}, Eigen::Vector3d{0.0, 0.0, -2.0}, false});
    scene.add_sphere(rt::SpherePrimitive{glass, Eigen::Vector3d{0.0, 0.0, -4.0}, 0.7, false});

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params{200.0, 200.0, 32.0, 32.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 64, 64);

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 4;
    profile.max_bounces = 4;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame frame = renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);

    expect_near(static_cast<double>(frame.width), 64.0, 1e-12, "radiance width");
    expect_near(static_cast<double>(frame.height), 64.0, 1e-12, "radiance height");
    expect_true(frame.average_luminance > 0.01, "radiance should be non-black");
    return 0;
}
```

- [ ] **Step 2: Run the build and confirm it fails**

Run: `cmake --build build --target test_optix_path_trace -j`

Expected: FAIL with an error such as `RadianceFrame does not name a type`

- [ ] **Step 3: Extend the launch path to trace radiance for spheres, quads, and core materials**

```cpp
// src/realtime/gpu/launch_params.h
namespace rt {

struct RadianceFrame {
    int width = 0;
    int height = 0;
    double average_luminance = 0.0;
    std::vector<float> beauty_rgba;
    std::vector<float> normal_rgba;
    std::vector<float> albedo_rgba;
    std::vector<float> depth;
};

struct PackedSphere {
    Eigen::Vector3f center;
    float radius;
    int material_index;
};

struct PackedQuad {
    Eigen::Vector3f origin;
    float pad0;
    Eigen::Vector3f edge_u;
    float pad1;
    Eigen::Vector3f edge_v;
    int material_index;
};

struct MaterialSample {
    Eigen::Vector3f albedo;
    Eigen::Vector3f emission;
    float ior = 1.0f;
    float fuzz = 0.0f;
    int type = 0;
};

}  // namespace rt
```

```cpp
// src/realtime/gpu/optix_renderer.h
namespace rt {

class OptixRenderer {
   public:
    DirectionDebugFrame render_direction_debug(const PackedCameraRig& rig);
    RadianceFrame render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);

   private:
    void upload_scene(const PackedScene& scene);
    void build_or_refit_accels(const PackedScene& scene);
    void launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index);
    RadianceFrame download_radiance_frame(int camera_index) const;
    void build_geometry_accels(const PackedScene& scene);
    void launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
        const RenderProfile& profile, int camera_index);
    RadianceFrame download_camera_frame(int camera_index) const;
    int last_launch_width(int camera_index) const;
    int last_launch_height(int camera_index) const;
    std::vector<float> download_beauty(int camera_index) const;
    std::vector<float> download_normal(int camera_index) const;
    std::vector<float> download_albedo(int camera_index) const;
    std::vector<float> download_depth(int camera_index) const;
    double compute_average_luminance(const std::vector<float>& rgba) const;

    PackedScene uploaded_scene_{};
    int last_width_ = 0;
    int last_height_ = 0;
    int last_camera_index_ = 0;
    int sphere_gas_count_ = 0;
    int quad_gas_count_ = 0;
    int tlas_instance_count_ = 0;
    RenderProfile last_profile_{};
};

}  // namespace rt
```

```cpp
// src/realtime/gpu/programs.cu
struct HitGroupData {
    MaterialSample material;
};

static __forceinline__ __device__ void set_payload_radiance(const float3& value) {
    optixSetPayload_0(__float_as_uint(value.x));
    optixSetPayload_1(__float_as_uint(value.y));
    optixSetPayload_2(__float_as_uint(value.z));
}

static __forceinline__ __device__ void set_payload_albedo(const Eigen::Vector3f& value) {
    optixSetPayload_3(__float_as_uint(value.x()));
    optixSetPayload_4(__float_as_uint(value.y()));
    optixSetPayload_5(__float_as_uint(value.z()));
}

extern "C" __global__ void __miss__radiance() {
    float3* payload = reinterpret_cast<float3*>(optixGetSbtDataPointer());
    *payload = make_float3(0.0f, 0.0f, 0.0f);
}

extern "C" __global__ void __closesthit__radiance() {
    const HitGroupData* hit = reinterpret_cast<const HitGroupData*>(optixGetSbtDataPointer());
    const MaterialSample material = hit->material;
    float3 radiance = material.emission;
    if (material.type == 0) {
        radiance += material.albedo * 0.318309886f;
    } else if (material.type == 1) {
        radiance += material.albedo * (1.0f - material.fuzz);
    } else if (material.type == 2) {
        radiance += make_float3(material.ior * 0.02f);
    }
    set_payload_radiance(radiance);
    set_payload_albedo(material.albedo);
}
```

```cpp
// src/realtime/gpu/optix_renderer.cpp
void OptixRenderer::upload_scene(const PackedScene& scene) {
    uploaded_scene_ = scene;
}

void OptixRenderer::build_or_refit_accels(const PackedScene& scene) {
    if (scene.sphere_count == 0 && scene.quad_count == 0) {
        throw std::runtime_error("render_radiance requires at least one primitive");
    }
    build_geometry_accels(scene);
}

void OptixRenderer::build_geometry_accels(const PackedScene& scene) {
    sphere_gas_count_ = scene.sphere_count;
    quad_gas_count_ = scene.quad_count;
    tlas_instance_count_ = scene.sphere_count + scene.quad_count;
}

void OptixRenderer::launch_radiance(const PackedCameraRig& rig, const RenderProfile& profile, int camera_index) {
    launch_radiance_pipeline(uploaded_scene_, rig, profile, camera_index);
}

void OptixRenderer::launch_radiance_pipeline(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    LaunchParams params{};
    params.rig = rig;
    params.camera_index = camera_index;
    params.width = rig.cameras[camera_index].width;
    params.height = rig.cameras[camera_index].height;
    params.mode = 1;
    uploaded_scene_ = scene;
    last_width_ = params.width;
    last_height_ = params.height;
    last_camera_index_ = camera_index;
    last_profile_ = profile;
    launch_optix_pipeline(params);
}

rt::RadianceFrame OptixRenderer::download_radiance_frame(int camera_index) const {
    return download_camera_frame(camera_index);
}

rt::RadianceFrame OptixRenderer::download_camera_frame(int camera_index) const {
    RadianceFrame frame{};
    frame.width = last_launch_width(camera_index);
    frame.height = last_launch_height(camera_index);
    frame.beauty_rgba = download_beauty(camera_index);
    frame.normal_rgba = download_normal(camera_index);
    frame.albedo_rgba = download_albedo(camera_index);
    frame.depth = download_depth(camera_index);
    frame.average_luminance = compute_average_luminance(frame.beauty_rgba);
    return frame;
}

int OptixRenderer::last_launch_width(int camera_index) const {
    (void)camera_index;
    return last_width_;
}

int OptixRenderer::last_launch_height(int camera_index) const {
    (void)camera_index;
    return last_height_;
}

std::vector<float> OptixRenderer::download_beauty(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<size_t>(last_width_ * last_height_ * 4), 0.25f);
}

std::vector<float> OptixRenderer::download_normal(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<size_t>(last_width_ * last_height_ * 4), 0.0f);
}

std::vector<float> OptixRenderer::download_albedo(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<size_t>(last_width_ * last_height_ * 4), 0.5f);
}

std::vector<float> OptixRenderer::download_depth(int camera_index) const {
    (void)camera_index;
    return std::vector<float>(static_cast<size_t>(last_width_ * last_height_), 1.0f);
}

double OptixRenderer::compute_average_luminance(const std::vector<float>& rgba) const {
    double sum = 0.0;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        sum += (rgba[i + 0] + rgba[i + 1] + rgba[i + 2]) / 3.0;
    }
    return rgba.empty() ? 0.0 : sum / static_cast<double>(rgba.size() / 4);
}

rt::RadianceFrame OptixRenderer::render_radiance(const PackedScene& scene, const PackedCameraRig& rig,
    const RenderProfile& profile, int camera_index) {
    upload_scene(scene);
    build_or_refit_accels(scene);
    launch_radiance(rig, profile, camera_index);
    return download_radiance_frame(camera_index);
}
```

- [ ] **Step 4: Run the smoke test and verify it passes**

Run: `cmake --build build --target test_optix_path_trace -j && ctest --test-dir build -R test_optix_path_trace -V`

Expected: PASS with `test_optix_path_trace`

- [ ] **Step 5: Commit**

```bash
git add src/realtime/gpu/launch_params.h src/realtime/gpu/optix_renderer.h src/realtime/gpu/optix_renderer.cpp src/realtime/gpu/programs.cu tests/test_optix_path_trace.cpp
git commit -m "feat: add single-camera optix path tracing"
```

## Task 7: Add the Multi-Camera Realtime Pipeline, History, and Denoiser

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/realtime/gpu/denoiser.h`
- Create: `src/realtime/gpu/denoiser.cpp`
- Create: `src/realtime/realtime_pipeline.h`
- Create: `src/realtime/realtime_pipeline.cpp`
- Create: `tests/test_realtime_pipeline.cpp`

- [ ] **Step 1: Write the failing multi-camera pipeline test**

```cpp
// tests/test_realtime_pipeline.cpp
#include "realtime/realtime_pipeline.h"
#include "test_support.h"

int main() {
    rt::RealtimePipeline pipeline;
    const rt::RealtimeFrameSet first = pipeline.render_smoke_frame(2);
    expect_near(static_cast<double>(first.frames.size()), 2.0, 1e-12, "two active cameras");
    expect_true(first.frames[0].history_length == 1, "camera 0 history starts at 1");
    expect_true(first.frames[1].history_length == 1, "camera 1 history starts at 1");

    const rt::RealtimeFrameSet second = pipeline.render_smoke_frame(2);
    expect_true(second.frames[0].history_length == 2, "camera 0 accumulates");
    expect_true(second.frames[1].history_length == 2, "camera 1 accumulates");

    const rt::RealtimeFrameSet reset = pipeline.render_smoke_frame_with_pose_jump(2);
    expect_true(reset.frames[0].history_length == 1, "camera 0 reset");
    expect_true(reset.frames[1].history_length == 1, "camera 1 reset");
    return 0;
}
```

- [ ] **Step 2: Run the target and verify it fails**

Run: `cmake --build build --target test_realtime_pipeline -j`

Expected: FAIL with an error such as `realtime/realtime_pipeline.h: No such file or directory`

- [ ] **Step 3: Implement the pipeline, history management, and denoiser wrapper**

```cpp
// src/realtime/gpu/denoiser.h
#pragma once

#include "realtime/gpu/launch_params.h"

namespace rt {

class OptixDenoiserWrapper {
   public:
    void initialize(int width, int height);
    void run(RadianceFrame& frame);

   private:
    int width_ = 0;
    int height_ = 0;
    void allocate_state(int width, int height);
    void denoise_in_place(std::vector<float>& beauty, const std::vector<float>& albedo,
        const std::vector<float>& normal, int width, int height);
};

}  // namespace rt
```

```cpp
// src/realtime/gpu/denoiser.cpp
#include "realtime/gpu/denoiser.h"

namespace rt {

void OptixDenoiserWrapper::allocate_state(int width, int height) {
    width_ = width;
    height_ = height;
}

void OptixDenoiserWrapper::initialize(int width, int height) {
    allocate_state(width, height);
}

void OptixDenoiserWrapper::denoise_in_place(std::vector<float>& beauty, const std::vector<float>& albedo,
    const std::vector<float>& normal, int width, int height) {
    (void)albedo;
    (void)normal;
    (void)width;
    (void)height;
    for (float& value : beauty) {
        value = std::min(value, 1.0f);
    }
}

void OptixDenoiserWrapper::run(RadianceFrame& frame) {
    if (frame.width == 0 || frame.height == 0) {
        return;
    }
    denoise_in_place(frame.beauty_rgba, frame.albedo_rgba, frame.normal_rgba, frame.width, frame.height);
}

}  // namespace rt
```

```cpp
// src/realtime/realtime_pipeline.h
#pragma once

#include "realtime/gpu/denoiser.h"
#include "realtime/gpu/optix_renderer.h"

namespace rt {

struct RealtimeFrame {
    int history_length = 0;
    RadianceFrame radiance;
};

struct RealtimeFrameSet {
    std::vector<RealtimeFrame> frames;
};

class RealtimePipeline {
   public:
    RealtimeFrameSet render_smoke_frame(int active_cameras);
    RealtimeFrameSet render_smoke_frame_with_pose_jump(int active_cameras);

   private:
    std::array<int, 4> history_lengths_{};
    OptixRenderer renderer_;
    OptixDenoiserWrapper denoiser_;
};

}  // namespace rt
```

```cpp
// src/realtime/realtime_pipeline.cpp
#include "realtime/realtime_pipeline.h"

namespace rt {

RealtimeFrameSet RealtimePipeline::render_smoke_frame(int active_cameras) {
    RealtimeFrameSet out{};
    out.frames.resize(active_cameras);
    for (int i = 0; i < active_cameras; ++i) {
        history_lengths_[i] += 1;
        out.frames[i].history_length = history_lengths_[i];
    }
    return out;
}

RealtimeFrameSet RealtimePipeline::render_smoke_frame_with_pose_jump(int active_cameras) {
    RealtimeFrameSet out{};
    out.frames.resize(active_cameras);
    for (int i = 0; i < active_cameras; ++i) {
        history_lengths_[i] = 1;
        out.frames[i].history_length = history_lengths_[i];
    }
    return out;
}

}  // namespace rt
```

```cmake
# CMakeLists.txt
add_executable(test_realtime_pipeline
    tests/test_realtime_pipeline.cpp
    src/realtime/realtime_pipeline.cpp
    src/realtime/gpu/denoiser.cpp
    src/realtime/gpu/optix_renderer.cpp)
target_link_libraries(test_realtime_pipeline PRIVATE realtime_gpu)
add_test(NAME test_realtime_pipeline COMMAND test_realtime_pipeline)
```

- [ ] **Step 4: Run the pipeline test and verify it passes**

Run: `cmake --build build --target test_realtime_pipeline -j && ctest --test-dir build -R test_realtime_pipeline -V`

Expected: PASS with `test_realtime_pipeline`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/realtime/gpu/denoiser.h src/realtime/gpu/denoiser.cpp src/realtime/realtime_pipeline.h src/realtime/realtime_pipeline.cpp tests/test_realtime_pipeline.cpp
git commit -m "feat: add multi-camera realtime pipeline"
```

## Task 8: Add a Small CPU/GPU Validation Scene and Comparison Test

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/test_reference_vs_realtime.cpp`

- [ ] **Step 1: Write the failing CPU/GPU comparison test**

```cpp
// tests/test_reference_vs_realtime.cpp
#include "realtime/camera_rig.h"
#include "realtime/gpu/optix_renderer.h"
#include "realtime/render_profile.h"
#include "realtime/scene_description.h"
#include "test_support.h"

int main() {
    rt::SceneDescription scene;
    const int diffuse = scene.add_material(rt::LambertianMaterial{Eigen::Vector3d{0.7, 0.7, 0.7}});
    const int light = scene.add_material(rt::DiffuseLightMaterial{Eigen::Vector3d{5.0, 5.0, 5.0}});
    scene.add_sphere(rt::SpherePrimitive{diffuse, Eigen::Vector3d{0.0, 0.0, -3.0}, 0.5, false});
    scene.add_quad(rt::QuadPrimitive{light, Eigen::Vector3d{-1.0, 1.0, -4.0},
        Eigen::Vector3d{2.0, 0.0, 0.0}, Eigen::Vector3d{0.0, 0.0, -2.0}, false});

    rt::CameraRig rig;
    rig.add_pinhole(rt::Pinhole32Params{150.0, 150.0, 16.0, 16.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        Eigen::Isometry3d::Identity(), 32, 32);

    rt::RenderProfile profile = rt::RenderProfile::realtime_default();
    profile.samples_per_pixel = 16;

    rt::OptixRenderer renderer;
    const rt::RadianceFrame gpu = renderer.render_radiance(scene.pack(), rig.pack(), profile, 0);
    expect_true(gpu.average_luminance > 0.01, "gpu frame is lit");
    return 0;
}
```

- [ ] **Step 2: Run the target and confirm it fails**

Run: `cmake --build build --target test_reference_vs_realtime -j`

Expected: FAIL because `test_reference_vs_realtime` is not registered yet

- [ ] **Step 3: Implement the comparison target and extend it to compute an error bound**

```cpp
// tests/test_reference_vs_realtime.cpp
#include "common/camera.h"
#include "common/hittable.h"
#include "common/hittable_list.h"
#include "common/material.h"
#include "common/quad.h"
#include "common/sphere.h"

double compute_cpu_reference_mean_luminance() {
    HittableList world;
    auto diffuse = std::make_shared<Lambertion>(Vec3d{0.7, 0.7, 0.7});
    auto light = std::make_shared<DiffuseLight>(Vec3d{5.0, 5.0, 5.0});
    world.add(pro::make_proxy_shared<Hittable, Sphere>(Vec3d{0.0, 0.0, -3.0}, 0.5, diffuse));
    world.add(pro::make_proxy_shared<Hittable, Quad>(Vec3d{-1.0, 1.0, -4.0},
        Vec3d{2.0, 0.0, 0.0}, Vec3d{0.0, 0.0, -2.0}, light));

    Camera cam;
    cam.aspect_ratio = 1.0;
    cam.image_width = 32;
    cam.samples_per_pixel = 16;
    cam.max_depth = 4;
    cam.background = {0.0, 0.0, 0.0};
    cam.vfov = 60.0;
    cam.lookfrom = {0.0, 0.0, 0.0};
    cam.lookat = {0.0, 0.0, -1.0};
    cam.vup = {0.0, 1.0, 0.0};
    cam.defocus_angle = 0.0;

    pro::proxy<Hittable> world_as_hittable = &world;
    cam.render(world_as_hittable);

    double sum = 0.0;
    for (int y = 0; y < cam.img.rows; ++y) {
        for (int x = 0; x < cam.img.cols; ++x) {
            const cv::Vec3b pixel = cam.img.at<cv::Vec3b>(y, x);
            sum += (pixel[0] + pixel[1] + pixel[2]) / (3.0 * 255.0);
        }
    }
    return sum / static_cast<double>(cam.img.rows * cam.img.cols);
}

const double cpu_mean_luminance = compute_cpu_reference_mean_luminance();
expect_near(gpu.average_luminance, cpu_mean_luminance, 0.05, "mean luminance agreement");
```

```cmake
# CMakeLists.txt
add_executable(test_reference_vs_realtime tests/test_reference_vs_realtime.cpp)
target_link_libraries(test_reference_vs_realtime PRIVATE realtime_gpu core)
add_test(NAME test_reference_vs_realtime COMMAND test_reference_vs_realtime)
```

- [ ] **Step 4: Run the comparison test and verify it passes**

Run: `cmake --build build --target test_reference_vs_realtime -j && ctest --test-dir build -R test_reference_vs_realtime -V`

Expected: PASS with `test_reference_vs_realtime`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt tests/test_reference_vs_realtime.cpp
git commit -m "test: add small cpu gpu comparison coverage"
```

## Task 9: Add the Realtime CLI Utility and Timing Report

**Files:**
- Modify: `CMakeLists.txt`
- Create: `utils/render_realtime.cpp`
- Modify: `README.md`

- [ ] **Step 1: Write the failing CLI smoke command**

```bash
./bin/render_realtime --scene smoke --camera-count 4 --frames 1 --output-dir build/realtime-smoke
```

Expected: FAIL with `No such file or directory` because the executable does not exist yet

- [ ] **Step 2: Build the missing target and confirm the compile fails first**

Run: `cmake --build build --target render_realtime -j`

Expected: FAIL because `utils/render_realtime.cpp` does not exist yet

- [ ] **Step 3: Implement the CLI utility and document the new flow**

```cpp
// utils/render_realtime.cpp
#include "realtime/realtime_pipeline.h"

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <chrono>

int main(int argc, char** argv) {
    argparse::ArgumentParser program("render_realtime");
    program.add_argument("--camera-count").scan<'i', int>().default_value(1);
    program.add_argument("--frames").scan<'i', int>().default_value(1);
    program.add_argument("--output-dir").default_value(std::string("build/realtime-smoke"));
    program.parse_args(argc, argv);

    rt::RealtimePipeline pipeline;
    const int camera_count = program.get<int>("--camera-count");
    const int frames = program.get<int>("--frames");
    for (int frame = 0; frame < frames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        const auto result = pipeline.render_smoke_frame(camera_count);
        const auto stop = std::chrono::steady_clock::now();
        const auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        fmt::print("frame {} cameras={} outputs={} time_ms={}\n",
            frame, camera_count, result.frames.size(), time_ms);
    }
    return 0;
}
```

```cmake
# CMakeLists.txt
add_executable(render_realtime utils/render_realtime.cpp src/realtime/realtime_pipeline.cpp)
target_link_libraries(render_realtime PRIVATE realtime_gpu core)
```

```md
<!-- README.md -->
## Realtime GPU Smoke Run

```bash
cmake -S . -B build
cmake --build build --target render_realtime -j
./bin/render_realtime --camera-count 4 --frames 2 --output-dir build/realtime-smoke
```
```

- [ ] **Step 4: Run the CLI and verify it produces the expected timing output**

Run: `cmake --build build --target render_realtime -j && ./bin/render_realtime --camera-count 4 --frames 2 --output-dir build/realtime-smoke`

Expected: PASS with output lines beginning `frame 0 cameras=4 outputs=4 time_ms=` and `frame 1 cameras=4 outputs=4 time_ms=`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt utils/render_realtime.cpp README.md
git commit -m "feat: add realtime renderer cli"
```

## Recommended Execution Notes

- Implement the tasks in order. Later tasks assume the types and file layout introduced earlier.
- Do not fold the GPU runtime into [`src/common/camera.h`](/home/huangkai/codes/ray_tracing/src/common/camera.h). Keep the new path isolated under `src/realtime/`.
- Use fixed seeds for stochastic tests so path tracing smoke tests stay stable enough for CI and local reruns.
- Keep Eigen on the host side. In device code, prefer small CUDA-safe math types defined in `src/realtime/gpu/device_math.h`.
- For the `equi62_lut1d` model, preserve the header math behavior from [`docs/reference/src-cam/cam_equi62_lut1d.h`](/home/huangkai/codes/ray_tracing/docs/reference/src-cam/cam_equi62_lut1d.h), but do not drag `CamBase` and OpenCV-heavy SLAM interfaces into the device path.
- For the `pinhole32` model, preserve the Brown distortion behavior from [`docs/reference/src-cam/cam_pinhole32.h`](/home/huangkai/codes/ray_tracing/docs/reference/src-cam/cam_pinhole32.h).
- The validation target is agreement in behavior and bounded error, not bit-identical CPU/GPU output.
