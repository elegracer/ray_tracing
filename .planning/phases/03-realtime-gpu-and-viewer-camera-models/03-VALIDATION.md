---
phase: 3
slug: realtime-gpu-and-viewer-camera-models
status: ready
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-21
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + CTest standalone C++ executables |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace|test_viewer_scene_reload)$' --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | Quick slice is moderate because the OptiX tests allocate GPU resources; task-level commands should stay narrower where listed below |

---

## Sampling Rate

- **After every task commit:** Run the task-level command from the table below
- **After every plan wave:** Run `ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace|test_viewer_scene_reload)$' --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 3-01-01 | 01 | 1 | CAM-04 | T-03-01-01 / T-03-01-02 | Viewer default rigs derive model/intrinsics from authored camera specs instead of hardcoded pinhole params | unit/integration | `cmake --build build --target test_realtime_scene_factory test_viewer_four_camera_rig -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig)$' --output-on-failure` | ✅ | ⬜ pending |
| 3-01-02 | 01 | 1 | CAM-04 | T-03-01-02 / T-03-01-03 | Spec-driven viewer rigs preserve yaw offsets, pose, and model/intrinsics for authored equi and mixed-model camera specs | integration | `cmake --build build --target test_viewer_four_camera_rig test_realtime_scene_factory -j4 && ctest --test-dir build -R '^(test_viewer_four_camera_rig|test_realtime_scene_factory)$' --output-on-failure` | ✅ | ⬜ pending |
| 3-02-01 | 02 | 2 | CAM-03 | T-03-02-01 / T-03-02-02 | Packed runtime camera data reaches `DeviceActiveCamera` without pinhole coercion | unit/integration | `cmake --build build --target test_optix_direction -j4 && ctest --test-dir build -R '^test_optix_direction$' --output-on-failure` | ✅ | ⬜ pending |
| 3-02-02 | 02 | 2 | CAM-03 | T-03-02-02 / T-03-02-03 | Realtime equi and mixed-model active-camera renders produce non-black output through the live OptiX path | regression | `cmake --build build --target test_optix_direction test_optix_equi_path_trace -j4 && ctest --test-dir build -R '^(test_optix_direction|test_optix_equi_path_trace)$' --output-on-failure` | ✅ | ⬜ pending |
| 3-03-01 | 03 | 3 | CAM-04 | T-03-03-01 | File-backed realtime presets and scene reload preserve authored camera model data through the viewer/runtime chain | integration | `cmake --build build --target test_realtime_scene_factory test_viewer_scene_reload -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_scene_reload)$' --output-on-failure` | ✅ | ⬜ pending |
| 3-03-02 | 03 | 3 | CAM-03 / CAM-04 | T-03-03-02 | Mixed-model four-camera regressions stay green together across factory, viewer, reload, and GPU execution | regression | `cmake --build build --target test_realtime_scene_factory test_viewer_four_camera_rig test_optix_direction test_optix_equi_path_trace test_viewer_scene_reload -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_viewer_four_camera_rig|test_optix_direction|test_optix_equi_path_trace|test_viewer_scene_reload)$' --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing CMake/CTest infrastructure is sufficient for the planned validation slice.

---

## Manual-Only Verifications

All Phase 3 behaviors are expected to have automated verification.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verification
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all test infrastructure needs
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** local-orchestrator approved 2026-04-21
