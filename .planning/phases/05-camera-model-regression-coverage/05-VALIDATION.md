---
phase: 5
slug: camera-model-regression-coverage
status: ready
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-22
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + CTest standalone C++ executables |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload)$' --output-on-failure` |
| **Full phase slice** | `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | Phase slice is short on the current baseline, but `test_viewer_quality_reference` and OptiX tests still allocate GPU resources, so earlier task checks should stay narrower where possible |

---

## Sampling Rate

- **After every task commit:** Run the task-level command from the table below
- **After every plan wave:** Run the full phase slice
- **Before `$gsd-verify-work`:** Full suite should be green
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 5-01-01 | 01 | 1 | VER-01 / VER-02 | T-05-01-01 / T-05-01-02 | `test_camera_models` directly anchors representative pinhole and equi samples to the reference headers with model-appropriate tolerances | unit | `cmake --build build --target test_camera_models -j4 && ctest --test-dir build -R '^test_camera_models$' --output-on-failure` | ✅ | ⬜ pending |
| 5-02-01 | 02 | 2 | VER-03 | T-05-02-01 / T-05-02-02 | Offline CPU primary rays and realtime GPU direction-debug encode the same world-space rays for the same packed cameras and pixel centers | integration | `cmake --build build --target test_offline_shared_scene_renderer test_optix_direction -j4 && ctest --test-dir build -R '^(test_offline_shared_scene_renderer|test_optix_direction)$' --output-on-failure` | ✅ | ⬜ pending |
| 5-02-02 | 02 | 2 | VER-03 | T-05-02-02 / T-05-02-03 | Cross-path smoke coverage exercises explicit pinhole plus helper-derived fisheye defaults without treating image-level similarity as the source of truth | regression | `cmake --build build --target test_reference_vs_realtime test_offline_shared_scene_renderer test_optix_direction -j4 && ctest --test-dir build -R '^(test_reference_vs_realtime|test_offline_shared_scene_renderer|test_optix_direction)$' --output-on-failure` | ✅ | ⬜ pending |
| 5-03-01 | 03 | 3 | VER-04 | T-05-03-01 / T-05-03-02 | Helper-generated defaults stay fisheye while explicitly authored pinhole scenes and presets remain pinhole across scene factory and shared-scene regressions | regression | `cmake --build build --target test_realtime_scene_factory test_shared_scene_regression test_viewer_scene_reload -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload)$' --output-on-failure` | ✅ | ⬜ pending |
| 5-03-02 | 03 | 3 | VER-04 | T-05-03-02 / T-05-03-03 | Viewer and live OptiX equi regressions stay green under the widened final slice after stronger parity assertions land | regression | `cmake --build build --target test_viewer_quality_reference test_optix_equi_path_trace test_camera_models test_offline_shared_scene_renderer test_optix_direction test_reference_vs_realtime test_realtime_scene_factory test_shared_scene_regression test_viewer_scene_reload -j4 && ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing CMake/CTest infrastructure is sufficient for the planned validation slice.
- [x] The widened mandatory Phase 5 slice is already executable on the current baseline.

---

## Manual-Only Verifications

All Phase 5 behaviors are expected to have automated verification. Manual verification is only needed if reference-header integration exposes a build-system issue that cannot be proven through `ctest`.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verification
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all test infrastructure needs
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** local-orchestrator approved 2026-04-22
