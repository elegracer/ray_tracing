# GPU-Side Temporal Accumulation with Screen-Space Reprojection — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move temporal accumulation from CPU to GPU with screen-space reprojection to eliminate camera-rotation ghosting.

**Architecture:** Add a `resolve_reprojection_kernel` in `programs.cu` that runs after `radiance_kernel`, reading current-frame beauty/normal/depth plus previous-frame history buffers and camera matrix to produce motion-compensated output on GPU. Simplify `ViewerQualityController::resolve_beauty_view()` to pass-through. `OptixRenderer` manages per-camera history buffers and prev-camera snapshots internally.

**Tech Stack:** CUDA, existing `DeviceFrameBuffers`/`DeviceActiveCamera` types, Eigen on host, project conventions.

---

## File Structure

| File | Role | Change |
|------|------|--------|
| `src/realtime/gpu/launch_params.h` | Add `DeviceFrameBuffers history`, `prev_origin`, `prev_basis_*`, `history_length` to `LaunchParams` | Modify |
| `src/realtime/gpu/programs.cu` | Add `project_*` helpers + `resolve_reprojection_kernel` + `launch_resolve_kernel` | Modify |
| `src/realtime/gpu/optix_renderer.h` | Add `device_history_` member, camera snapshot fields, `reset_accumulation()` method | Modify |
| `src/realtime/gpu/optix_renderer.cpp` | Manage GPU history buffers, wire resolve kernel after radiance kernel, camera snapshot | Modify |
| `src/realtime/gpu/renderer_pool.h` | Add `reset_accumulation()` declaration | Modify |
| `src/realtime/gpu/renderer_pool.cpp` | Add `reset_accumulation()` body | Modify |
| `src/realtime/viewer/viewer_quality_controller.h` | Simplify `CameraHistory`, remove `pose_exceeded_reset_threshold` | Modify |
| `src/realtime/viewer/viewer_quality_controller.cpp` | Simplify `resolve_beauty_view()` to pass-through, simplify `begin_frame()` | Modify |
| `utils/render_realtime_viewer.cpp` | Call `pool.reset_accumulation()` on preview mode | Modify |

---

### Task 1: Extend LaunchParams and add forward declarations

**Files:**
- Modify: `src/realtime/gpu/launch_params.h` (1 change)
- Modify: `src/realtime/gpu/programs.cu` (add forward declarations near top)
- Modify: `src/realtime/gpu/optix_renderer.cpp` (add forward declaration)

- [ ] **Step 1: Add history and prev-camera fields to LaunchParams**

In `src/realtime/gpu/launch_params.h`, insert these new fields at the end of `LaunchParams` (before the closing `};` of the struct, after `int mode = 0;`):

```cpp
    // --- temporal reprojection ---
    DeviceFrameBuffers history {};
    double prev_origin[3] {};
    double prev_basis_x[3] {};
    double prev_basis_y[3] {};
    double prev_basis_z[3] {};
    int history_length = 0;
```

- [ ] **Step 2: Add forward declarations in programs.cu**

In `src/realtime/gpu/programs.cu`, after the existing forward declarations (after line 44), add:

```cpp
__device__ float3 project_pinhole32(const DevicePinhole32Params& params, const float3& dir_camera);
__device__ float3 project_equi62_lut1d(const DeviceEqui62Lut1DParams& params, const float3& dir_camera);
__device__ float2 project_camera_pixel(const DeviceActiveCamera& camera, const float3& dir_camera);
__device__ float3 decode_normal(const float4& encoded);
```

- [ ] **Step 3: Declare launch_resolve_kernel in optix_renderer.cpp**

In `src/realtime/gpu/optix_renderer.cpp`, after line 19 (`void launch_radiance_kernel(const LaunchParams& params, cudaStream_t stream);`):

```cpp
void launch_resolve_kernel(const LaunchParams& params, cudaStream_t stream);
```

- [ ] **Step 4: Build to verify no regressions yet**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: Build succeeds (new fields are unused but valid).

- [ ] **Step 5: Commit**

```bash
git add src/realtime/gpu/launch_params.h src/realtime/gpu/programs.cu src/realtime/gpu/optix_renderer.cpp
git commit -m "feat: extend LaunchParams with history and prev-camera fields for temporal reprojection"
```

---

### Task 2: Implement camera projection helpers

**Files:**
- Modify: `src/realtime/gpu/programs.cu`

- [ ] **Step 1: Implement `project_pinhole32` — 3D camera-space direction → pixel coordinates**

This is the inverse of `unproject_pinhole32`. Reuses the existing `distort_pinhole_normalized` (defined at line 216).

Insert after the `unproject_pinhole32` function (after line 279):

```cpp
__device__ float3 project_pinhole32(const DevicePinhole32Params& params, const float3& dir_camera) {
    if (fabsf(dir_camera.z) < 1e-7f) {
        return make_float3(-1.0f, -1.0f, 0.0f);
    }
    const double inv_z = 1.0 / static_cast<double>(dir_camera.z);
    const Double2 xy_undistorted = make_double2_xy(
        static_cast<double>(dir_camera.x) * inv_z,
        static_cast<double>(dir_camera.y) * inv_z);
    const Double2 xy_distorted = distort_pinhole_normalized(params, xy_undistorted);
    return make_float3(
        static_cast<float>(xy_distorted.x * params.fx + params.cx),
        static_cast<float>(xy_distorted.y * params.fy + params.cy),
        1.0f);
}
```

- [ ] **Step 2: Implement `invert_lut_theta` — invert the 1D distortion LUT**

Insert after the existing `interpolate_lut_theta` function (after line 326):

```cpp
__device__ double invert_lut_theta(const DeviceEqui62Lut1DParams& params, double theta) {
    if (theta <= 0.0 || params.lut_step <= 0.0) {
        return 0.0;
    }
    if (theta >= params.lut[1023]) {
        return 1023.0 * params.lut_step;
    }
    for (int i = 0; i < 1023; ++i) {
        if (params.lut[i] <= theta && theta <= params.lut[i + 1]) {
            const double denom = params.lut[i + 1] - params.lut[i];
            const double alpha = denom > 0.0 ? (theta - params.lut[i]) / denom : 0.0;
            return (static_cast<double>(i) + alpha) * params.lut_step;
        }
    }
    return 1023.0 * params.lut_step;
}
```

- [ ] **Step 3: Implement `project_equi62_lut1d` — 3D camera-space direction → pixel coordinates**

This is the inverse of `unproject_equi62_lut1d`. Insert after `invert_lut_theta`:

```cpp
__device__ float3 project_equi62_lut1d(const DeviceEqui62Lut1DParams& params, const float3& dir_camera) {
    const double dx = static_cast<double>(dir_camera.x);
    const double dy = static_cast<double>(dir_camera.y);
    const double dz = static_cast<double>(dir_camera.z);
    const double r = sqrt(dx * dx + dy * dy);
    if (r < kCameraEpsilon) {
        return make_float3(static_cast<float>(params.cx), static_cast<float>(params.cy), 1.0f);
    }
    const double theta = atan2(r, dz);
    const double rd = invert_lut_theta(params, theta);
    const double azim_cos = dx / r;
    const double azim_sin = dy / r;
    const Double2 xy_radial = make_double2_xy(azim_cos * rd, azim_sin * rd);
    const Double2 xy_distorted = apply_equi_tangential(xy_radial, params.tangential);
    return make_float3(
        static_cast<float>(xy_distorted.x * params.fx + params.cx),
        static_cast<float>(xy_distorted.y * params.fy + params.cy),
        1.0f);
}
```

- [ ] **Step 4: Implement `project_camera_pixel` — camera-model dispatch**

```cpp
__device__ float2 project_camera_pixel(const DeviceActiveCamera& camera, const float3& dir_camera) {
    float3 pixel;
    if (camera.model == CameraModelType::equi62_lut1d) {
        pixel = project_equi62_lut1d(camera.equi, dir_camera);
    } else {
        pixel = project_pinhole32(camera.pinhole, dir_camera);
    }
    return make_float2(pixel.x, pixel.y);
}
```

- [ ] **Step 5: Implement `decode_normal` helper**

```cpp
__device__ float3 decode_normal(const float4& encoded) {
    return make_float3(
        encoded.x * 2.0f - 1.0f,
        encoded.y * 2.0f - 1.0f,
        encoded.z * 2.0f - 1.0f);
}
```

- [ ] **Step 6: Build to verify compilation**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: Build succeeds.

- [ ] **Step 7: Commit**

```bash
git add src/realtime/gpu/programs.cu
git commit -m "feat: add camera projection helpers for screen-space reprojection"
```

---

### Task 3: Implement the resolve reprojection kernel

**Files:**
- Modify: `src/realtime/gpu/programs.cu`

- [ ] **Step 1: Implement `resolve_reprojection_kernel`**

Insert after the existing `radiance_kernel` definition ends (after line 824, before the `throw_cuda_error` function):

```cpp
__global__ void resolve_reprojection_kernel(const LaunchParams* params_ptr) {
    const LaunchParams& params = *params_ptr;
    const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
    if (x >= params.width || y >= params.height) {
        return;
    }

    const int pixel_index = y * params.width + x;

    if (params.history_length <= 0
        || params.history.beauty == nullptr
        || params.history.normal == nullptr
        || params.history.depth == nullptr) {
        return;
    }

    const float4 current_beauty = params.frame.beauty[pixel_index];
    const float4 current_normal = params.frame.normal[pixel_index];
    const float current_depth = params.frame.depth[pixel_index];

    // Reconstruct world-space position from current depth and current camera
    const float pixel_x = static_cast<float>(x) + 0.5f;
    const float pixel_y = static_cast<float>(y) + 0.5f;
    const float3 dir_camera = unproject_camera_ray(params.active_camera, pixel_x, pixel_y);
    const float3 dir_world = transform_direction(params.active_camera, dir_camera);
    const float3 origin = camera_origin(params.active_camera);
    const float3 world_pos = add3(origin, mul3(dir_world, current_depth));

    // Transform world position to previous camera space
    const float3 world_offset = make_float3(
        static_cast<float>(world_pos.x - params.prev_origin[0]),
        static_cast<float>(world_pos.y - params.prev_origin[1]),
        static_cast<float>(world_pos.z - params.prev_origin[2]));

    const float3 bx = make_float3(
        static_cast<float>(params.prev_basis_x[0]),
        static_cast<float>(params.prev_basis_x[1]),
        static_cast<float>(params.prev_basis_x[2]));
    const float3 by = make_float3(
        static_cast<float>(params.prev_basis_y[0]),
        static_cast<float>(params.prev_basis_y[1]),
        static_cast<float>(params.prev_basis_y[2]));
    const float3 bz = make_float3(
        static_cast<float>(params.prev_basis_z[0]),
        static_cast<float>(params.prev_basis_z[1]),
        static_cast<float>(params.prev_basis_z[2]));

    // prev_cam_space = (dot(offset, bx), dot(offset, by), dot(offset, bz))
    const float3 prev_cam_space = make_float3(
        dot3(world_offset, bx),
        dot3(world_offset, by),
        dot3(world_offset, bz));

    const float2 prev_pixel = project_camera_pixel(params.active_camera, prev_cam_space);

    const int prev_x = static_cast<int>(prev_pixel.x);
    const int prev_y = static_cast<int>(prev_pixel.y);

    bool valid = false;
    if (prev_x >= 0 && prev_x < params.width && prev_y >= 0 && prev_y < params.height) {
        const int prev_idx = prev_y * params.width + prev_x;

        const float3 history_normal = decode_normal(params.history.normal[prev_idx]);
        const float3 curr_normal_decoded = decode_normal(current_normal);
        const float normal_dot = dot3(curr_normal_decoded, history_normal);

        const float history_depth = params.history.depth[prev_idx];
        const float depth_ratio = history_depth > 0.0f ? current_depth / history_depth : 1.0f;

        if (normal_dot > 0.85f && depth_ratio > 0.9f && depth_ratio < 1.1f) {
            valid = true;
        }
    }

    if (valid) {
        const int prev_idx = prev_y * params.width + prev_x;
        const float4 h_beauty = params.history.beauty[prev_idx];
        const int next_len = params.history_length + 1;
        const float blend = 1.0f / static_cast<float>(next_len);
        params.frame.beauty[pixel_index] = make_float4(
            h_beauty.x + (current_beauty.x - h_beauty.x) * blend,
            h_beauty.y + (current_beauty.y - h_beauty.y) * blend,
            h_beauty.z + (current_beauty.z - h_beauty.z) * blend,
            1.0f);
    }
}
```

- [ ] **Step 2: Implement `launch_resolve_kernel` wrapper**

Insert after the existing `launch_radiance_kernel` function (after line 1080 at end of file):

```cpp
void launch_resolve_kernel(const LaunchParams& params, cudaStream_t stream) {
    LaunchParams* device_params = nullptr;
    throw_cuda_error(
        cudaMalloc(reinterpret_cast<void**>(&device_params), sizeof(LaunchParams)), "cudaMalloc()");
    try {
        throw_cuda_error(
            cudaMemcpyAsync(device_params, &params, sizeof(LaunchParams),
                cudaMemcpyHostToDevice, stream), "cudaMemcpyAsync()");
        const dim3 block_size(8, 8, 1);
        resolve_reprojection_kernel<<<make_grid(params.width, params.height, block_size),
            block_size, 0, stream>>>(device_params);
        throw_cuda_error(cudaGetLastError(), "cudaGetLastError()");
        throw_cuda_error(cudaFree(device_params), "cudaFree()");
    } catch (...) {
        cudaFree(device_params);
        throw;
    }
}
```

- [ ] **Step 3: Build to verify compilation**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/realtime/gpu/programs.cu
git commit -m "feat: add resolve_reprojection_kernel for GPU-side temporal accumulation"
```

---

### Task 4: Add GPU history buffer management to OptixRenderer

**Files:**
- Modify: `src/realtime/gpu/optix_renderer.h`
- Modify: `src/realtime/gpu/optix_renderer.cpp`

- [ ] **Step 1: Add history members and API to header**

In `src/realtime/gpu/optix_renderer.h`, add `reset_accumulation()` to the public section near `prepare_scene`:

```cpp
    void reset_accumulation();
```

Add these private members before the closing `};` of the class:

```cpp
    void allocate_history_buffers(int width, int height);
    void free_history_buffers();
    void swap_history_buffers();
    void populate_launch_history(LaunchParams& params);
    void snapshot_camera_for_history(const LaunchParams& params);

    DeviceFrameBuffers device_history_{};
    int history_width_ = 0;
    int history_height_ = 0;
    int history_length_ = 0;
    double prev_origin_[3] {};
    double prev_basis_x_[3] {};
    double prev_basis_y_[3] {};
    double prev_basis_z_[3] {};
```

- [ ] **Step 2: Implement `allocate_history_buffers`**

In `src/realtime/gpu/optix_renderer.cpp`, add after the `allocate_frame_buffers` function or near the existing buffer management blocks:

```cpp
void OptixRenderer::allocate_history_buffers(int width, int height) {
    if (width == history_width_ && height == history_height_
        && device_history_.beauty != nullptr) {
        return;
    }
    free_history_buffers();

    const std::size_t float4_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    RT_CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&device_history_.beauty), float4_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&device_history_.normal), float4_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&device_history_.albedo), float4_count * sizeof(float4)));
    RT_CUDA_CHECK(cudaMalloc(
        reinterpret_cast<void**>(&device_history_.depth), float4_count * sizeof(float)));

    history_width_ = width;
    history_height_ = height;
    history_length_ = 0;
}
```

- [ ] **Step 3: Implement `free_history_buffers`**

```cpp
void OptixRenderer::free_history_buffers() {
    if (device_history_.beauty != nullptr) {
        cudaFree(device_history_.beauty);
        device_history_.beauty = nullptr;
    }
    if (device_history_.normal != nullptr) {
        cudaFree(device_history_.normal);
        device_history_.normal = nullptr;
    }
    if (device_history_.albedo != nullptr) {
        cudaFree(device_history_.albedo);
        device_history_.albedo = nullptr;
    }
    if (device_history_.depth != nullptr) {
        cudaFree(device_history_.depth);
        device_history_.depth = nullptr;
    }
    history_width_ = 0;
    history_height_ = 0;
    history_length_ = 0;
}
```

- [ ] **Step 4: Implement `swap_history_buffers` — DeviceToDevice copy**

```cpp
void OptixRenderer::swap_history_buffers() {
    const std::size_t float4_count =
        static_cast<std::size_t>(history_width_) * static_cast<std::size_t>(history_height_);
    RT_CUDA_CHECK(cudaMemcpyAsync(
        device_history_.beauty, device_frame_.beauty,
        float4_count * sizeof(float4), cudaMemcpyDeviceToDevice, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(
        device_history_.normal, device_frame_.normal,
        float4_count * sizeof(float4), cudaMemcpyDeviceToDevice, stream_));
    RT_CUDA_CHECK(cudaMemcpyAsync(
        device_history_.depth, device_frame_.depth,
        float4_count * sizeof(float), cudaMemcpyDeviceToDevice, stream_));
}
```

- [ ] **Step 5: Implement `populate_launch_history`**

```cpp
void OptixRenderer::populate_launch_history(LaunchParams& params) {
    params.history = device_history_;
    params.history_length = history_length_;
    params.prev_origin[0] = prev_origin_[0];
    params.prev_origin[1] = prev_origin_[1];
    params.prev_origin[2] = prev_origin_[2];
    params.prev_basis_x[0] = prev_basis_x_[0];
    params.prev_basis_x[1] = prev_basis_x_[1];
    params.prev_basis_x[2] = prev_basis_x_[2];
    params.prev_basis_y[0] = prev_basis_y_[0];
    params.prev_basis_y[1] = prev_basis_y_[1];
    params.prev_basis_y[2] = prev_basis_y_[2];
    params.prev_basis_z[0] = prev_basis_z_[0];
    params.prev_basis_z[1] = prev_basis_z_[1];
    params.prev_basis_z[2] = prev_basis_z_[2];
}
```

- [ ] **Step 6: Implement `snapshot_camera_for_history`**

```cpp
void OptixRenderer::snapshot_camera_for_history(const LaunchParams& params) {
    const DeviceActiveCamera& cam = params.active_camera;
    prev_origin_[0] = cam.origin[0];
    prev_origin_[1] = cam.origin[1];
    prev_origin_[2] = cam.origin[2];
    prev_basis_x_[0] = cam.basis_x[0];
    prev_basis_x_[1] = cam.basis_x[1];
    prev_basis_x_[2] = cam.basis_x[2];
    prev_basis_y_[0] = cam.basis_y[0];
    prev_basis_y_[1] = cam.basis_y[1];
    prev_basis_y_[2] = cam.basis_y[2];
    prev_basis_z_[0] = cam.basis_z[0];
    prev_basis_z_[1] = cam.basis_z[1];
    prev_basis_z_[2] = cam.basis_z[2];
}
```

- [ ] **Step 7: Implement `reset_accumulation`**

```cpp
void OptixRenderer::reset_accumulation() {
    history_length_ = 0;
}
```

- [ ] **Step 8: Update destructor (~OptixRenderer)**

The destructor is already defined. Add `free_history_buffers();` before `free_device_resources();` in the destructor body. Read the destructor in `optix_renderer.cpp` first, then insert at the appropriate location.

- [ ] **Step 9: Build to verify compilation**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: Build succeeds.

- [ ] **Step 10: Commit**

```bash
git add src/realtime/gpu/optix_renderer.h src/realtime/gpu/optix_renderer.cpp
git commit -m "feat: add GPU history buffer management to OptixRenderer"
```

---

### Task 5: Wire up resolve kernel in launch pipeline

**Files:**
- Modify: `src/realtime/gpu/optix_renderer.cpp`
- Modify: `src/realtime/gpu/renderer_pool.h`
- Modify: `src/realtime/gpu/renderer_pool.cpp`

- [ ] **Step 1: Add `reset_accumulation` to RendererPool**

In `src/realtime/gpu/renderer_pool.h`, in the public section (after `prepare_scene` declaration):

```cpp
    void reset_accumulation();
```

In `src/realtime/gpu/renderer_pool.cpp`, add the body (after `prepare_scene` definition):

```cpp
void RendererPool::reset_accumulation() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (OptixRenderer& renderer : renderers_) {
        renderer.reset_accumulation();
    }
}
```

- [ ] **Step 2: Insert resolve kernel launch after radiance kernel**

In `src/realtime/gpu/optix_renderer.cpp`, function `launch_radiance_pipeline`. There are two code paths (with and without timing). Both need the same resolve block inserted.

**Non-timed path** (around line 614, after `launch_radiance_kernel(params, stream_);`):

Replace:
```cpp
        launch_radiance_kernel(params, stream_);
```

With:
```cpp
        launch_radiance_kernel(params, stream_);
        allocate_history_buffers(params.width, params.height);
        populate_launch_history(params);
        launch_resolve_kernel(params, stream_);
        snapshot_camera_for_history(params);
        if (history_length_ == 0) {
            history_length_ = 1;
        } else {
            ++history_length_;
        }
        swap_history_buffers();
```

**Timed path** (around line 626, after `launch_radiance_kernel(params, stream_);`):

Apply the exact same replacement — add the same 9 lines after `launch_radiance_kernel(params, stream_);`.

- [ ] **Step 3: Build and run smoke test**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
./bin/render_realtime --scene final_room --camera-count 1 --frames 1 --profile quality --output-dir build/smoke-test
```

Expected: Build succeeds. No crash. PNG produced.

- [ ] **Step 4: Commit**

```bash
git add src/realtime/gpu/optix_renderer.cpp src/realtime/gpu/renderer_pool.h src/realtime/gpu/renderer_pool.cpp
git commit -m "feat: wire up resolve kernel after radiance kernel in launch pipeline"
```

---

### Task 6: Simplify CPU-side ViewerQualityController

**Files:**
- Modify: `src/realtime/viewer/viewer_quality_controller.h`
- Modify: `src/realtime/viewer/viewer_quality_controller.cpp`
- Modify: `utils/render_realtime_viewer.cpp`

- [ ] **Step 1: Simplify `CameraHistory` struct**

In `src/realtime/viewer/viewer_quality_controller.h`, replace the `CameraHistory` struct:

Old:
```cpp
    struct CameraHistory {
        int width = 0;
        int height = 0;
        int history_length = 0;
        std::vector<float> beauty_rgba;
    };
```

New:
```cpp
    struct CameraHistory {
        int width = 0;
        int height = 0;
    };
```

- [ ] **Step 2: Remove `pose_exceeded_reset_threshold` declaration**

In `src/realtime/viewer/viewer_quality_controller.h`, remove the line:
```cpp
    bool pose_exceeded_reset_threshold(const BodyPose& pose) const;
```

- [ ] **Step 3: Simplify `history_length` accessor**

Since GPU manages history_length now, change the public `history_length()` method to always return 0 (or remove it — but keep a stub for any callers):

In `.h`, keep the declaration. In `.cpp`, change:

```cpp
int ViewerQualityController::history_length(int camera_index) const {
    (void)camera_index;
    return 0;
}
```

- [ ] **Step 4: Simplify `resolve_beauty_view` to pass-through**

In `src/realtime/viewer/viewer_quality_controller.cpp`, replace the entire body of `resolve_beauty_view`:

```cpp
ResolvedBeautyFrameView ViewerQualityController::resolve_beauty_view(
    int camera_index, const RadianceFrame& raw_frame) {
    (void)camera_index;
    return ResolvedBeautyFrameView {
        .width = raw_frame.width,
        .height = raw_frame.height,
        .average_luminance = compute_average_luminance(raw_frame.beauty_rgba),
        .beauty_rgba = raw_frame.beauty_rgba,
    };
}
```

- [ ] **Step 5: Remove unused helpers from anonymous namespace**

In `src/realtime/viewer/viewer_quality_controller.cpp`, remove these constants and functions from the anonymous namespace:

Remove:
```cpp
constexpr float kMaxSanitizedBeautyValue = 64.0f;
constexpr float kHistoryClampMultiplier = 1.5f;
constexpr float kDarkHistoryClampMultiplier = 4.0f;
constexpr float kDarkHistoryThreshold = 0.5f;
constexpr float kHistoryClampFloor = 0.75f;
```

Remove `pose_translation_delta()`, `pose_rotation_delta_deg()`, `is_valid_beauty_value()`, `sanitized_value()`, `bounded_history_value()`.

Also remove `pose_exceeded_reset_threshold()` definition.

`compute_average_luminance()` — KEEP this function, it's still used in `resolve_beauty_view`.

- [ ] **Step 6: Simplify `begin_frame` — only clear on scene change or first frame**

Replace `begin_frame`:

```cpp
void ViewerQualityController::begin_frame(std::string_view scene_id, const BodyPose& pose) {
    const bool scene_changed = current_scene_id_ != scene_id;

    if (!has_last_pose_ || scene_changed) {
        clear_histories();
        stable_frame_count_ = 0;
        active_mode_ = ViewerQualityMode::preview;
    } else {
        ++stable_frame_count_;
        active_mode_ = stable_frame_count_ >= 1 ? ViewerQualityMode::converge : ViewerQualityMode::preview;
    }

    current_scene_id_ = std::string(scene_id);
    last_pose_ = pose;
    has_last_pose_ = true;
}
```

- [ ] **Step 7: Wire `pool.reset_accumulation()` call in viewer loop**

In `utils/render_realtime_viewer.cpp`, after `quality_controller.begin_frame(...)` (around line 464), add:

```cpp
        if (quality_controller.active_mode() == rt::viewer::ViewerQualityMode::preview) {
            pool.reset_accumulation();
        }
```

This clears GPU history when the quality controller enters preview mode (first frame, scene change).

- [ ] **Step 8: Build and smoke test**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
./bin/render_realtime --scene final_room --camera-count 1 --frames 3 --profile quality --output-dir build/cpu-simplify-test
```

Expected: Build succeeds. No crash. Multi-frame output works.

- [ ] **Step 9: Commit**

```bash
git add src/realtime/viewer/viewer_quality_controller.h src/realtime/viewer/viewer_quality_controller.cpp utils/render_realtime_viewer.cpp
git commit -m "refactor: simplify ViewerQualityController to pass-through, blending now on GPU"
```

---

### Task 7: Build viewer target and validation test

**Files:** No changes — verification only.

- [ ] **Step 1: Build viewer target**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j
```

Expected: Build succeeds.

- [ ] **Step 2: Static scene multi-frame convergence test**

```bash
./bin/render_realtime --scene final_room --camera-count 4 --frames 10 --profile quality --output-dir build/validation-static --skip-image-write
```

Expected: All 4 cameras complete. Check `benchmark_summary.json` — no NaN/inf values.

- [ ] **Step 3: Scene switch test**

```bash
./bin/render_realtime --scene cornell_box --camera-count 4 --frames 5 --profile realtime --output-dir build/validation-switch --skip-image-write
```

Expected: No crash. No cross-scene contamination.

- [ ] **Step 4: Performance baseline**

```bash
./bin/render_realtime --camera-count 4 --frames 5 --profile realtime --output-dir build/perf-test
python3 -c "import json; d=json.load(open('build/perf-test/benchmark_summary.json')); print(f'{d[\"fps\"]:.1f} fps')"
```

Expected: FPS within ~5% of pre-change baseline. Resolve kernel overhead should be < 1ms.

- [ ] **Step 5: Commit (if any build fixes needed)**

```bash
git add -u
git commit -m "test: validate temporal reprojection correctness and performance"
```
