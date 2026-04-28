# Temporal Reprojection Artifact Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix white overexposed dots and surface-ghosting artifacts during camera translation by tightening temporal reprojection validation, fixing sampling bugs, and wiring up camera-motion-driven accumulation reset.

**Architecture:** Five targeted fixes in the resolve kernel and quality controller. No new files or buffers. Each fix addresses a specific, identified causal chain: loose thresholds allow incorrect history matches; nearest-neighbor truncation accepts negative coords; unblended normal/depth let fireflies anchor in history; missing pose tracking means accumulation never resets on camera motion; unclamped beauty values let extreme fireflies enter the EMA.

**Tech Stack:** CUDA, OptiX, C++17

---

### Task 1: Tighten validation thresholds and fix negative coordinate truncation

**Files:**
- Modify: `src/realtime/gpu/programs.cu:951-952,965`

- [ ] **Step 1: Fix negative coordinate truncation**

In `resolve_reprojection_kernel`, replace `static_cast<int>()` with `floorf` for pixel coordinate conversion. Currently `static_cast<int>(-0.4f)` yields `0` (truncation toward zero), passing the subsequent bounds check but indexing the wrong pixel.

Change `programs.cu:951-952` from:
```cuda
const int prev_x = static_cast<int>(prev_pixel.x);
const int prev_y = static_cast<int>(prev_pixel.y);
```
to:
```cuda
const int prev_x = static_cast<int>(floorf(prev_pixel.x));
const int prev_y = static_cast<int>(floorf(prev_pixel.y));
```

- [ ] **Step 2: Tighten validation thresholds**

Change `programs.cu:965` from:
```cuda
if (normal_dot > 0.85f && depth_ratio > 0.9f && depth_ratio < 1.1f) {
```
to:
```cuda
if (normal_dot > 0.95f && depth_ratio > 0.95f && depth_ratio < 1.05f) {
```

- [ ] **Step 3: Build and run existing tests**

```bash
cmake --build build-clang-vcpkg-settings --target test_viewer_quality_controller -j
./bin/test_viewer_quality_controller
```

Expected: all tests pass. These changes don't affect the quality controller logic.

---

### Task 2: Blend normal and depth in resolve kernel

**Files:**
- Modify: `src/realtime/gpu/programs.cu:975-979`

- [ ] **Step 1: Add normal/depth blending**

In `resolve_reprojection_kernel`, inside the `if (valid)` block, extend the EMA blend to normal and depth. Currently only `frame.beauty` is updated; `frame.normal` and `frame.depth` retain raw single-sample values, and `swap_history_buffers` copies these noisy values into next frame's validation data.

Change `programs.cu:970-980` from:
```cuda
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
```
to:
```cuda
if (valid) {
    const int prev_idx = prev_y * params.width + prev_x;
    const float4 h_beauty = params.history.beauty[prev_idx];
    const float4 h_normal = params.history.normal[prev_idx];
    const float h_depth = params.history.depth[prev_idx];
    const int next_len = params.history_length + 1;
    const float blend = 1.0f / static_cast<float>(next_len);

    params.frame.beauty[pixel_index] = make_float4(
        h_beauty.x + (current_beauty.x - h_beauty.x) * blend,
        h_beauty.y + (current_beauty.y - h_beauty.y) * blend,
        h_beauty.z + (current_beauty.z - h_beauty.z) * blend,
        1.0f);

    params.frame.normal[pixel_index] = make_float4(
        h_normal.x + (current_normal.x - h_normal.x) * blend,
        h_normal.y + (current_normal.y - h_normal.y) * blend,
        h_normal.z + (current_normal.z - h_normal.z) * blend,
        h_normal.w + (current_normal.w - h_normal.w) * blend);

    params.frame.depth[pixel_index] =
        h_depth + (current_depth - h_depth) * blend;
}
```

- [ ] **Step 2: Build CUDA target**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: compile succeeds with no warnings.

---

### Task 3: Add firefly clamping before blending

**Files:**
- Modify: `src/realtime/gpu/programs.cu:973-975`

- [ ] **Step 1: Clamp current beauty before blend**

In the valid block of `resolve_reprojection_kernel`, clamp the current frame's beauty RGB components before blending them into history. This prevents a single-sample firefly from entering the exponentially-weighted history.

Add before the blend computation (after `const float blend = ...`):

```cuda
const float4 clamped_current = make_float4(
    fminf(fmaxf(current_beauty.x, 0.0f), 10.0f),
    fminf(fmaxf(current_beauty.y, 0.0f), 10.0f),
    fminf(fmaxf(current_beauty.z, 0.0f), 10.0f),
    1.0f);
```

Then change the blend to use `clamped_current` instead of `current_beauty`:
```cuda
params.frame.beauty[pixel_index] = make_float4(
    h_beauty.x + (clamped_current.x - h_beauty.x) * blend,
    h_beauty.y + (clamped_current.y - h_beauty.y) * blend,
    h_beauty.z + (clamped_current.z - h_beauty.z) * blend,
    1.0f);
```

> **Tuning note:** The clamp ceiling of `10.0` is a starting value. If bright light sources are clamped, raise it to `50.0` or `100.0`. If fireflies persist, lower it to `5.0`. The floor of `0.0` rejects negative radiance (physically impossible, can occur from numerical issues).

- [ ] **Step 2: Build CUDA target**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime -j
```

Expected: compile succeeds with no warnings.

---

### Task 4: Wire up camera motion to trigger accumulation reset

**Files:**
- Modify: `src/realtime/viewer/viewer_quality_controller.h:30,48-54`
- Modify: `src/realtime/viewer/viewer_quality_controller.cpp:26-41`

- [ ] **Step 1: Add previous pose tracking to header**

In `viewer_quality_controller.h`, add a `BodyPose prev_pose_` member and make the pose comparison logic accessible.

Add `#include <optional>` at the top. Then add to the private section:
```cpp
std::optional<BodyPose> prev_pose_;
```

After `int stable_frame_count_ = 0;` line.

- [ ] **Step 2: Implement camera motion detection in begin_frame**

In `viewer_quality_controller.cpp`, replace the current `begin_frame` body. The existing code ignores `pose` via `(void)pose`. The new code detects significant camera motion and resets to preview.

Replace `viewer_quality_controller.cpp:26-41`:
```cpp
void ViewerQualityController::begin_frame(std::string_view scene_id, const BodyPose& pose) {
    const bool scene_changed = current_scene_id_ != scene_id;
    bool pose_changed = false;

    if (prev_pose_.has_value()) {
        const double translation = (pose.position - prev_pose_->position).norm();
        const double yaw_delta = std::abs(pose.yaw_deg - prev_pose_->yaw_deg);
        const double pitch_delta = std::abs(pose.pitch_deg - prev_pose_->pitch_deg);
        const double rotation = std::max(yaw_delta, pitch_delta);

        const auto& profile = active_mode_ == ViewerQualityMode::converge
            ? converge_profile_ : preview_profile_;
        if (translation > profile.accumulation_reset_translation
            || rotation > profile.accumulation_reset_rotation_deg) {
            pose_changed = true;
        }
    }

    if (is_first_frame_ || scene_changed || pose_changed) {
        clear_histories();
        stable_frame_count_ = 0;
        active_mode_ = ViewerQualityMode::preview;
    } else {
        ++stable_frame_count_;
        active_mode_ = stable_frame_count_ >= 1 ? ViewerQualityMode::converge : ViewerQualityMode::preview;
    }

    current_scene_id_ = std::string(scene_id);
    is_first_frame_ = false;
    prev_pose_ = pose;
}
```

- [ ] **Step 3: Update reset_all to clear prev_pose_**

In `viewer_quality_controller.cpp`, in the `reset_all` method, add:
```cpp
prev_pose_.reset();
```
after the existing `stable_frame_count_ = 0;` line.

- [ ] **Step 4: Build and run viewer quality controller tests**

```bash
cmake --build build-clang-vcpkg-settings --target test_viewer_quality_controller -j && ./bin/test_viewer_quality_controller
```

Expected: existing tests pass (they use the same pose for consecutive frames, so no motion reset is triggered).

---

### Task 5: Add pose-change test for quality controller

**Files:**
- Modify: `tests/test_viewer_quality_controller.cpp`

- [ ] **Step 1: Add test case for pose-driven reset**

Add after the "First frame after reset starts in preview" test block (after line 143), before the `// --- materialize_frame` block:

```cpp
    // --- Significant camera motion triggers accumulation reset ---
    {
        // Converge first
        controller.reset_all();
        controller.begin_frame("scene_a", pose);
        controller.begin_frame("scene_a", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "converge after two stable frames");

        // Large translation resets to preview
        const rt::viewer::BodyPose moved_pose {
            .position = Eigen::Vector3d(1.0, 0.0, 0.0),
            .yaw_deg = 0.0,
            .pitch_deg = 0.0,
        };
        controller.begin_frame("scene_a", moved_pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "large translation resets to preview (1.0 > 0.25 threshold)");

        // Re-converge
        controller.begin_frame("scene_a", moved_pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "re-converge after stable frames at new pose");
    }

    // --- Small camera motion does NOT trigger reset ---
    {
        controller.reset_all();
        controller.begin_frame("scene_a", pose);
        controller.begin_frame("scene_a", pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "converge after two stable frames");

        const rt::viewer::BodyPose slightly_moved_pose {
            .position = Eigen::Vector3d(0.01, 0.0, 0.0),
            .yaw_deg = 0.5,
            .pitch_deg = 0.0,
        };
        controller.begin_frame("scene_a", slightly_moved_pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::converge,
            "small camera motion does NOT reset accumulation (0.01 < 0.25, 0.5 < 5.0)");
    }

    // --- Large rotation triggers reset ---
    {
        controller.reset_all();
        controller.begin_frame("scene_a", pose);
        controller.begin_frame("scene_a", pose);

        const rt::viewer::BodyPose rotated_pose {
            .position = Eigen::Vector3d::Zero(),
            .yaw_deg = 10.0,
            .pitch_deg = 0.0,
        };
        controller.begin_frame("scene_a", rotated_pose);
        expect_true(controller.active_mode() == rt::viewer::ViewerQualityMode::preview,
            "large rotation resets to preview (10.0 > 5.0 threshold)");
    }
```

- [ ] **Step 2: Build and run tests**

```bash
cmake --build build-clang-vcpkg-settings --target test_viewer_quality_controller -j && ./bin/test_viewer_quality_controller
```

Expected: all tests pass, including the 3 new pose-driven reset tests.

---

### Task 6: Visual validation build

- [ ] **Step 1: Full build**

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j
```

Expected: clean build, no warnings.

- [ ] **Step 2: Launch viewer for manual inspection**

```bash
./bin/render_realtime_viewer
```

Manual verification checklist:
1. Start viewer, let camera stabilize for 2 frames → converge mode engages
2. Rotate in place (mouse look) → minimal white dots, no smearing
3. Translate (WASD) → no white dots spreading across surfaces, edges stay clean
4. Rapid translation → accumulation resets if threshold exceeded, restarts smoothly
5. Long camera stillness → image converges cleanly without firefly persistence

> **Parameter tuning:** If white dots persist at edges during translation, tighten thresholds further (normal_dot > 0.98, depth_ratio [0.97, 1.03]). If too many pixels are rejected (flickering), loosen slightly (normal_dot > 0.92, depth_ratio [0.92, 1.08]). Adjust firefly clamp ceiling if bright sources appear dimmed.

---

### Task 7: Commit

- [ ] **Step 1: Stage and commit**

```bash
git add src/realtime/gpu/programs.cu src/realtime/viewer/viewer_quality_controller.cpp src/realtime/viewer/viewer_quality_controller.h tests/test_viewer_quality_controller.cpp
git commit -m "fix: tighten temporal reprojection validation and blend normal/depth

- Tighten reprojection validation: normal_dot 0.85→0.95, depth_ratio 0.9-1.1→0.95-1.05
- Fix negative pixel coordinate truncation (static_cast→floorf) for correct bounds rejection
- Blend normal and depth alongside beauty in resolve kernel so validation uses stable data
- Clamp current beauty to [0,10] before EMA blend to prevent firefly anchoring
- Wire up camera pose tracking to trigger accumulation reset on significant motion"
```

---

### Tuning Quick Reference

| Parameter | File:Line | Current | Purpose | Adjust if... |
|-----------|-----------|---------|---------|-------------|
| `normal_dot` min | `programs.cu:965` | `0.95f` | Surface match strictness | Too many rejects → lower; smearing → raise |
| `depth_ratio` range | `programs.cu:965` | `0.95f..1.05f` | Depth match strictness | Too many rejects → widen; ghosting → narrow |
| `clamped_current` max | `programs.cu` (Task 3) | `10.0f` | Firefly ceiling | Dim lights → raise; fireflies persist → lower |
| `accumulation_reset_translation` | `render_profile.h:11` | `0.05` | Translation reset threshold | Too frequent resets → raise; smearing → lower |
| `accumulation_reset_rotation_deg` | `render_profile.h:10` | `2.0` | Rotation reset threshold | Too frequent resets → raise; smearing → lower |
