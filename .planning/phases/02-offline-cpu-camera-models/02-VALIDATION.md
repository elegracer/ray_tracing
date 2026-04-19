---
phase: 2
slug: offline-cpu-camera-models
status: ready
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-20
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + CTest standalone C++ executables |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_shared_scene_regression|test_viewer_quality_reference)$' --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | Quick slice is moderate because `test_viewer_quality_reference` accumulates GPU frames; per-task runs should prefer narrower commands where listed below |

---

## Sampling Rate

- **After every task commit:** Run the task-level command from the table below
- **After every plan wave:** Run `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_shared_scene_regression|test_viewer_quality_reference)$' --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 2-01-01 | 01 | 1 | CAM-02 | T-02-01-01 / T-02-01-02 | Offline primary rays are emitted from shared camera math for both models without duplicating the tracer loop | unit/integration | `cmake --build build --target test_camera_models test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer)$' --output-on-failure` | ✅ | ⬜ pending |
| 2-01-02 | 01 | 1 | CAM-02 | T-02-01-02 | Pinhole keeps defocus behavior while equi stays rectangular and no-defocus | integration | `cmake --build build --target test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^test_offline_shared_scene_renderer$' --output-on-failure` | ✅ | ⬜ pending |
| 2-02-01 | 02 | 2 | CAM-02 | T-02-02-01 | `render_shared_scene(...)` adapts preset camera model/intrinsics instead of ignoring `camera.model` | integration | `cmake --build build --target test_offline_shared_scene_renderer test_shared_scene_regression -j4 && ctest --test-dir build -R '^(test_offline_shared_scene_renderer|test_shared_scene_regression)$' --output-on-failure` | ✅ | ⬜ pending |
| 2-02-02 | 02 | 2 | CAM-02 | T-02-02-02 / T-02-02-03 | File-backed or temp-catalog preset-path coverage proves offline fisheye uses the same shared-scene route | integration | `cmake --build build --target test_offline_shared_scene_renderer test_scene_file_catalog -j4 && ctest --test-dir build -R '^(test_offline_shared_scene_renderer|test_scene_file_catalog)$' --output-on-failure` | ✅ | ⬜ pending |
| 2-03-01 | 03 | 3 | CAM-02 | T-02-03-01 | Explicit packed-camera offline reference rendering stays usable for viewer-quality checks after model-aware rollout | regression | `cmake --build build --target test_viewer_quality_reference -j4 && ctest --test-dir build -R '^test_viewer_quality_reference$' --output-on-failure` | ✅ | ⬜ pending |
| 2-03-02 | 03 | 3 | CAM-02 | T-02-03-02 | Model-switch and low-level shared-math parity regressions stay green together | regression | `cmake --build build --target test_camera_models test_offline_shared_scene_renderer test_viewer_quality_reference -j4 && ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_viewer_quality_reference)$' --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing CMake/CTest infrastructure is sufficient for the planned validation slice.

---

## Manual-Only Verifications

All Phase 2 behaviors are expected to have automated verification.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verification
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all test infrastructure needs
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** local-orchestrator approved 2026-04-20
