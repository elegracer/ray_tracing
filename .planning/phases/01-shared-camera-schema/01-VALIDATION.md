---
phase: 1
slug: shared-camera-schema
status: ready
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-19
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake + CTest standalone C++ executables |
| **Config file** | `CMakeLists.txt` |
| **Quick run command** | `ctest --test-dir build -R 'test_camera_rig|test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory' --output-on-failure` |
| **Full suite command** | `ctest --test-dir build --output-on-failure` |
| **Estimated runtime** | ~2 seconds for the quick slice, longer for the full suite |

---

## Sampling Rate

- **After every task commit:** Run `ctest --test-dir build -R 'test_camera_rig|test_scene_definition|test_yaml_scene_loader|test_scene_file_catalog|test_shared_scene_regression|test_offline_shared_scene_renderer|test_realtime_scene_factory' --output-on-failure`
- **After every plan wave:** Run `ctest --test-dir build --output-on-failure`
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 1-01-01 | 01 | 1 | CAM-01, CAM-05 | T-01-01-01 / T-01-01-02 | `CameraSpec` carries canonical model/intrinsics/extrinsics without derived runtime state | unit | `cmake --build build --target test_camera_rig -j4 && ctest --test-dir build -R '^test_camera_rig$' --output-on-failure` | ✅ | ⬜ pending |
| 1-01-02 | 01 | 1 | CAM-01 | T-01-01-01 | `CameraRig` adapts from `CameraSpec` and preserves mixed-model packing semantics | integration | `cmake --build build --target test_camera_rig -j4 && ctest --test-dir build -R '^test_camera_rig$' --output-on-failure` | ✅ | ⬜ pending |
| 1-02-01 | 02 | 2 | CAM-01, CAM-05, DEF-01 | T-01-02-02 | Shared preset structs own `CameraSpec`, and realtime/offline consumers compile against the new boundary without semantic creep | integration | `cmake --build build --target test_realtime_scene_factory test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_offline_shared_scene_renderer)$' --output-on-failure` | ✅ | ⬜ pending |
| 1-02-02 | 02 | 2 | DEF-01 | T-01-02-01 | Builtin scene declarations explicitly set `model` and shared intrinsics | integration | `cmake --build build --target test_realtime_scene_factory test_offline_shared_scene_renderer -j4 && ctest --test-dir build -R '^(test_realtime_scene_factory|test_offline_shared_scene_renderer)$' --output-on-failure` | ✅ | ⬜ pending |
| 1-03-01 | 03 | 3 | CAM-01, CAM-05, DEF-01 | T-01-03-01 | YAML loader rejects missing/legacy camera declarations and parses `CameraSpec` correctly | integration | `cmake --build build --target test_yaml_scene_loader -j4 && ctest --test-dir build -R '^test_yaml_scene_loader$' --output-on-failure` | ✅ | ⬜ pending |
| 1-03-02 | 03 | 3 | DEF-01 | T-01-03-02 / T-01-03-03 | Repo-owned YAML scenes and builtin scenes preserve the same camera model/intrinsics through catalog overlay | integration | `cmake --build build --target test_shared_scene_regression test_scene_file_catalog test_yaml_scene_loader -j4 && ctest --test-dir build -R '^(test_shared_scene_regression|test_scene_file_catalog|test_yaml_scene_loader)$' --output-on-failure` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing infrastructure covers all phase requirements.

---

## Manual-Only Verifications

All phase behaviors have automated verification.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-04-19
