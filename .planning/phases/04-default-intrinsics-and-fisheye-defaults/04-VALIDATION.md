---
phase: 4
slug: default-intrinsics-and-fisheye-defaults
status: ready
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-21
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + CTest standalone C++ executables |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build -R '^(test_camera_models|test_shared_scene_builders|test_realtime_scene_factory|test_viewer_four_camera_rig|test_viewer_quality_reference)$' --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | Quick slice is moderate; `test_viewer_quality_reference` allocates OptiX/GPU resources, so keep earlier task checks narrower where possible |

---

## Sampling Rate

- **After every task commit:** Run the task-level command from the table below
- **After every plan wave:** Run `ctest --test-dir build -R '^(test_camera_models|test_shared_scene_builders|test_realtime_scene_factory|test_viewer_four_camera_rig|test_viewer_quality_reference)$' --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite should be green
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 4-01-01 | 01 | 1 | DEF-04 / DEF-05 / DEF-06 | T-04-01-01 / T-04-01-02 | One shared helper derives default intrinsics for both models and the CLI wrapper reports the same values | unit/smoke | `cmake --build build --target derive_default_camera_intrinsics test_camera_models -j4 && ./bin/derive_default_camera_intrinsics --model pinhole32 --width 640 --height 480 --hfov-deg 90 && ./bin/derive_default_camera_intrinsics --model equi62_lut1d --width 640 --height 480 --hfov-deg 120 && ctest --test-dir build -R '^test_camera_models$' --output-on-failure` | ✅ | ⬜ pending |
| 4-02-01 | 02 | 2 | DEF-02 / DEF-03 | T-04-02-01 / T-04-02-02 | Builtin helper-generated default CPU and realtime presets flip to fisheye while explicit pinhole presets remain pinhole | unit/integration | `cmake --build build --target test_shared_scene_builders test_realtime_scene_factory -j4 && ctest --test-dir build -R '^(test_shared_scene_builders|test_realtime_scene_factory)$' --output-on-failure` | ✅ | ⬜ pending |
| 4-02-02 | 02 | 2 | DEF-02 / DEF-03 | T-04-02-02 / T-04-02-03 | Repo-owned builtin defaults and shared-scene regressions preserve authored pinhole cases while default-generated cases switch models | regression | `cmake --build build --target test_shared_scene_builders test_realtime_scene_factory -j4 && ctest --test-dir build -R '^(test_shared_scene_builders|test_realtime_scene_factory)$' --output-on-failure` | ✅ | ⬜ pending |
| 4-03-01 | 03 | 3 | DEF-02 / DEF-03 | T-04-03-01 / T-04-03-02 | No-arg viewer rigs use fisheye defaults, but explicit pinhole/equi authored viewer cameras still pack and resize correctly | integration | `cmake --build build --target test_viewer_four_camera_rig test_realtime_scene_factory -j4 && ctest --test-dir build -R '^(test_viewer_four_camera_rig|test_realtime_scene_factory)$' --output-on-failure` | ✅ | ⬜ pending |
| 4-03-02 | 03 | 3 | DEF-02 / DEF-03 / DEF-04 | T-04-03-02 / T-04-03-03 | Viewer CPU reference and default rig regressions remain green after the default switch, proving the fallback/default path still produces valid cameras | regression | `cmake --build build --target test_viewer_four_camera_rig test_viewer_quality_reference test_realtime_scene_factory -j4 && ctest --test-dir build -R '^(test_viewer_four_camera_rig|test_viewer_quality_reference|test_realtime_scene_factory)$' --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing CMake/CTest infrastructure is sufficient for the planned validation slice.

---

## Manual-Only Verifications

All Phase 4 behaviors are expected to have automated verification plus one CLI smoke invocation of the intrinsics utility.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verification
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all test infrastructure needs
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** local-orchestrator approved 2026-04-21
