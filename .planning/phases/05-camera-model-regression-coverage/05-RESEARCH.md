# Phase 5: Camera Model Regression Coverage - Research

**Researched:** 2026-04-22
**Domain:** Reference-backed camera-model parity and cross-path regression closure after fisheye became the default [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`, `tests/test_camera_models.cpp`, `tests/test_optix_direction.cpp`, `tests/test_reference_vs_realtime.cpp`, `tests/test_viewer_quality_reference.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`, `tests/test_realtime_scene_factory.cpp`, `tests/test_optix_equi_path_trace.cpp`]
**Confidence:** HIGH [VERIFIED: relevant code paths and the widened regression slice were inspected locally; `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure` passed on 2026-04-22]

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Phase 5 must verify `pinhole32` and `equi62_lut1d` against the reference headers at the math and direction level rather than relying only on behavior-level roundtrips. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-02:** Representative samples and explicit tolerances are preferred over a large exhaustive numerical sweep. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-03:** `pinhole32` may use tighter numeric tolerances than `equi62_lut1d`, because the latter is LUT-backed and may include small interpolation error. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-04:** Offline CPU and realtime GPU consistency is judged primarily by shared camera-contract preservation: same packed camera, same pixel center, same world-space ray. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-05:** Image-level render comparisons may remain smoke coverage, but they are not the main acceptance proof for this phase. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-06:** The coverage matrix must include explicit `pinhole32`, explicit `equi62_lut1d`, helper-derived default fisheye intrinsics, and regression proving explicitly authored pinhole scenes/presets stay pinhole after the default switch. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-07:** Phase 5 should use a widened mandatory regression slice that includes `test_shared_scene_regression`, `test_viewer_scene_reload`, and `test_optix_equi_path_trace`, not only the narrow camera-math tests. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- **D-08:** Prefer extending existing test binaries instead of creating many new executables. Production camera code should only change if stronger tests expose a real bug. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]

### the agent's Discretion
- The exact representative sample points and parameter fixtures used for direct reference parity. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- The exact distribution of new assertions across existing test files. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- Whether any small test-only adapters or CMake include-path changes are needed to call the reference headers directly. [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`, `CMakeLists.txt`]

### Deferred Ideas (OUT OF SCOPE)
- Exhaustive dense-grid or full-image sweeps against the reference headers. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- A broader image-quality benchmarking framework between offline and realtime. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
- New calibration-editing surfaces such as authored extrinsics editing, non-zero distortion tuning flows, or dynamic render-preset resolution controls. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| VER-01 | Automated tests verify `pinhole32` math against `docs/reference/src-cam/cam_pinhole32.h`. [VERIFIED: `.planning/REQUIREMENTS.md`] | `tests/test_camera_models.cpp` already exercises pinhole projection/unprojection, but today it only validates self-consistency and expected formulas. It does not call the reference header or prove point-for-point parity against the reference implementation. [VERIFIED: `tests/test_camera_models.cpp`, `docs/reference/src-cam/cam_pinhole32.h`] |
| VER-02 | Automated tests verify `equi62_lut1d` math against `docs/reference/src-cam/cam_equi62_lut1d.h`. [VERIFIED: `.planning/REQUIREMENTS.md`] | The same gap exists for fisheye: current tests prove roundtrip behavior and analytic zero-distortion behavior, but not direct parity with the LUT-backed reference header. [VERIFIED: `tests/test_camera_models.cpp`, `docs/reference/src-cam/cam_equi62_lut1d.h`] |
| VER-03 | Automated tests show offline and realtime paths honor the same selected camera model and default intrinsics. [VERIFIED: `.planning/REQUIREMENTS.md`] | `tests/test_offline_shared_scene_renderer.cpp` already proves the offline seam matches shared camera math, and `tests/test_optix_direction.cpp` already proves OptiX direction debug matches shared camera math, but there is no single test that explicitly compares both paths under the same packed camera contract. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_optix_direction.cpp`] |
| VER-04 | Existing relevant render, scene, and viewer tests continue to pass after fisheye becomes the default. [VERIFIED: `.planning/REQUIREMENTS.md`] | The widened Phase 5 slice is already green on the current baseline, which means the phase can treat it as a regression lock and only widen assertions where the default-switch contract is still implicit. [VERIFIED: `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure`] |
</phase_requirements>

## Project Constraints

- Surface assumptions and tradeoffs before implementation instead of silently choosing one. [VERIFIED: user-provided `AGENTS.md` content]
- Prefer the minimum code that solves the problem; avoid speculative abstractions and unrelated cleanups. [VERIFIED: user-provided `AGENTS.md` content]
- Keep edits surgical and directly traceable to regression coverage rather than feature expansion. [VERIFIED: user-provided `AGENTS.md` content]
- Define success through tests and explicit verification commands. [VERIFIED: user-provided `AGENTS.md` content]

## Summary

Phase 5 starts from a strong baseline: the widened regression slice already passes end-to-end, so this is not a bug-hunting phase by default. The real missing coverage is sharper proof. `tests/test_camera_models.cpp` knows the repo math is self-consistent, but it does not yet prove the math agrees with the reference headers in `docs/reference/src-cam/`. That is the clearest gap for `VER-01` and `VER-02`, and it is best closed in the existing unit-style camera-model test rather than by inventing a second dedicated parity binary. [VERIFIED: `tests/test_camera_models.cpp`, `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`]

The reference headers are not just prose. They contain callable camera implementations, but they are not currently part of the test target include path. That means the lowest-risk Phase 5 path is likely a small test-only integration step: either extend `test_camera_models` to include the reference headers directly, or add a tiny test-local adapter that translates the repo parameter fixtures into the reference-camera classes. This is still a surgical test-only change, but it should be planned explicitly rather than assumed. [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`, `CMakeLists.txt`]

For `VER-03`, the project already has the two right seams: `tests/test_offline_shared_scene_renderer.cpp` proves the offline CPU seam emits the same rays as shared camera math through `Camera::debug_primary_ray(...)`, and `tests/test_optix_direction.cpp` proves the GPU direction-debug path emits the same rays as shared camera math. The missing proof is the join between them. Because both tests already use `PackedCamera` and pixel-center sampling, the safest Phase 5 move is to connect these existing seams with the same fixtures instead of adding new production debugging hooks. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_optix_direction.cpp`, `src/core/offline_shared_scene_renderer.cpp`, `src/common/camera.h`]

`tests/test_reference_vs_realtime.cpp` is the weakest current part of the widened slice. It still uses a legacy direct `Camera` setup and only compares one pinhole scene by mean luminance. That is useful smoke coverage, but it does not prove anything about the new per-camera model contract or the default fisheye helper. Phase 5 should upgrade this file into a contract-following smoke test that exercises at least one explicit pinhole path and one helper-derived fisheye path while keeping the image-level assertions coarse. [VERIFIED: `tests/test_reference_vs_realtime.cpp`, `src/realtime/camera_models.cpp`, `.planning/phases/04-default-intrinsics-and-fisheye-defaults/04-CONTEXT.md`]

The default-switch aftermath is already spread across several strong regression anchors. `tests/test_realtime_scene_factory.cpp` checks that helper-generated defaults such as `earth_sphere` are fisheye while explicitly authored presets such as `final_room` remain pinhole. `tests/test_shared_scene_regression.cpp` checks file-backed equi presets and repo-owned YAML preservation. `tests/test_viewer_scene_reload.cpp` checks that file-backed equi scene reload and rescan stay equi after runtime refresh. `tests/test_viewer_quality_reference.cpp` already spans default equi viewer rigs and explicit pinhole overrides. Phase 5 should use these tests as the widened regression lock, only adding assertions where the authored-vs-default distinction is still too implicit. [VERIFIED: `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`, `tests/test_viewer_quality_reference.cpp`]

**Primary recommendation:** Keep Phase 5 verification-heavy. First, anchor `test_camera_models` directly to the reference headers for representative pinhole and equi samples. Second, strengthen the offline/GPU contract proof by reusing the existing offline seam and OptiX direction-debug seam with the same packed-camera fixtures, plus one upgraded cross-path smoke test around helper-derived default fisheye. Third, lock the milestone with the widened regression slice and a few sharper authored-vs-default assertions instead of broad new infrastructure. [VERIFIED: `tests/test_camera_models.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_optix_direction.cpp`, `tests/test_reference_vs_realtime.cpp`, `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`]

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Direct parity to `cam_pinhole32.h` / `cam_equi62_lut1d.h` | `tests/test_camera_models.cpp` | `CMakeLists.txt` | The phase should extend the existing model-level regression target, only adding the minimum build plumbing needed to include or adapt the reference headers. [VERIFIED: `tests/test_camera_models.cpp`, `CMakeLists.txt`] |
| Offline CPU ray contract | `tests/test_offline_shared_scene_renderer.cpp` | `src/common/camera.h` | This test already drives `Camera::debug_primary_ray(...)` through canonical `PackedCamera`-backed state, making it the cleanest offline-side ray seam. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`, `src/common/camera.h`] |
| Realtime GPU ray contract | `tests/test_optix_direction.cpp` | `src/realtime/gpu/programs.cu` | Direction-debug is deterministic and already keyed by `camera_index`, so it is the stable GPU-side comparison seam. [VERIFIED: `tests/test_optix_direction.cpp`, `src/realtime/gpu/programs.cu`] |
| Cross-path image-level smoke | `tests/test_reference_vs_realtime.cpp` | `tests/test_viewer_quality_reference.cpp` | `test_reference_vs_realtime` is the natural place for a lightweight offline-vs-realtime smoke upgrade, while `test_viewer_quality_reference` already covers the heavier viewer accumulation path. [VERIFIED: `tests/test_reference_vs_realtime.cpp`, `tests/test_viewer_quality_reference.cpp`] |
| Default-vs-authored regression lock | `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp` | `tests/test_optix_equi_path_trace.cpp` | These tests already span helper-generated defaults, explicit authored pinhole presets, file-backed equi reload, and live OptiX equi rendering. Phase 5 should widen this existing lattice instead of replacing it. [VERIFIED: `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`, `tests/test_optix_equi_path_trace.cpp`] |

## Standard Stack

### Core

| Item | Role | Why Standard |
|------|------|--------------|
| `docs/reference/src-cam/cam_pinhole32.h` | Canonical pinhole math reference. [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`] | The milestone explicitly promises parity to this header, so Phase 5 should call or mirror it in tests directly. |
| `docs/reference/src-cam/cam_equi62_lut1d.h` | Canonical fisheye math reference. [VERIFIED: `docs/reference/src-cam/cam_equi62_lut1d.h`] | Same reason: direct parity is the missing proof for the equi model. |
| `tests/test_camera_models.cpp` | Existing projection/unprojection regression target. [VERIFIED: `tests/test_camera_models.cpp`] | Already owns low-level camera math coverage and default-intrinsics checks. |
| `tests/test_offline_shared_scene_renderer.cpp` | Existing offline camera-contract target. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`] | Already has `PackedCamera` fixtures and offline primary-ray debug checks. |
| `tests/test_optix_direction.cpp` | Existing realtime direction-contract target. [VERIFIED: `tests/test_optix_direction.cpp`] | Already reduces GPU behavior to deterministic per-pixel direction output. |

### Supporting

| Item | Role | When to Use |
|------|------|-------------|
| `tests/test_reference_vs_realtime.cpp` | Coarse offline-vs-realtime smoke harness. [VERIFIED: `tests/test_reference_vs_realtime.cpp`] | Use it after ray-level parity is locked, not as the primary contract proof. |
| `tests/test_realtime_scene_factory.cpp` | Default-helper versus authored-preset contract coverage. [VERIFIED: `tests/test_realtime_scene_factory.cpp`] | Use it to prove implicit fisheye defaults and explicit pinhole authored presets still coexist correctly. |
| `tests/test_shared_scene_regression.cpp` | Shared-scene and file-backed preset preservation coverage. [VERIFIED: `tests/test_shared_scene_regression.cpp`] | Use it to lock file-backed/default switch regressions without mutating repo-owned assets. |
| `tests/test_viewer_scene_reload.cpp` | File-backed equi reload/rescan coverage. [VERIFIED: `tests/test_viewer_scene_reload.cpp`] | Use it to keep the widened slice honest when defaults and file-backed equi scenes interact. |
| `tests/test_optix_equi_path_trace.cpp` | Live OptiX equi render smoke. [VERIFIED: `tests/test_optix_equi_path_trace.cpp`] | Keep it in the mandatory slice so fisheye remains live, not just direction-debug-correct. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extend existing test binaries | Create several new dedicated Phase 5 test executables | This would add build and maintenance overhead for a phase that is supposed to sharpen existing regression proof, not expand infrastructure. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`] |
| Compare offline and GPU at the ray/direction seam | Rely mainly on whole-image render comparisons | Whole-image comparisons are noisier and already intentionally coarse in the current codebase, so they are the wrong main contract for this phase. [VERIFIED: `tests/test_reference_vs_realtime.cpp`, `tests/test_viewer_quality_reference.cpp`] |
| Integrate the reference headers into `test_camera_models` | Re-derive the reference formulas by hand inside repo tests | That would weaken the meaning of VER-01/VER-02 and create a second copy of the reference math rather than proving parity to the actual headers. [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`] |

**Installation:** None. The baseline CMake/CTest stack is already sufficient for the current widened slice; Phase 5 only needs targeted test extensions and possibly a test-target include-path adjustment for the reference headers. [VERIFIED: `CMakeLists.txt`, `ctest --test-dir build -R '^(test_camera_models|test_offline_shared_scene_renderer|test_optix_direction|test_reference_vs_realtime|test_viewer_quality_reference|test_realtime_scene_factory|test_shared_scene_regression|test_viewer_scene_reload|test_optix_equi_path_trace)$' --output-on-failure`]

**Version verification:** N/A for the recommended core pieces because they are repo-local tests and reference headers. [VERIFIED: `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`, `tests/test_camera_models.cpp`]

## Architecture Patterns

### System Architecture Diagram

```text
reference headers
`cam_pinhole32.h` / `cam_equi62_lut1d.h`
                 |
                 v
      `test_camera_models.cpp`
      direct parity at project/unproject seam

offline seam (`Camera::debug_primary_ray`)
                 |
                 v
`test_offline_shared_scene_renderer.cpp`
                 |
                 v
same `PackedCamera` fixtures / pixel centers
                 |
                 v
`test_optix_direction.cpp`
                 |
                 v
realtime GPU direction-debug contract

default/authored regression lattice
`test_realtime_scene_factory` + `test_shared_scene_regression`
    + `test_viewer_scene_reload` + `test_viewer_quality_reference`
    + `test_optix_equi_path_trace`
```

The phase should sharpen proof at each existing seam instead of adding a new cross-cutting abstraction. [VERIFIED: `tests/test_camera_models.cpp`, `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_optix_direction.cpp`, `tests/test_realtime_scene_factory.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_viewer_scene_reload.cpp`]

### Pattern 1: Reference-header parity inside the existing math test

**What:** Keep low-level parity in `tests/test_camera_models.cpp`, augmenting it with direct calls or a thin adapter around the reference headers. [VERIFIED: `tests/test_camera_models.cpp`, `docs/reference/src-cam/cam_pinhole32.h`, `docs/reference/src-cam/cam_equi62_lut1d.h`]

**When to use:** Use this because the phase needs stronger proof at the math seam, not another standalone parity harness. [VERIFIED: `.planning/ROADMAP.md`, `.planning/REQUIREMENTS.md`]

### Pattern 2: Join existing offline and GPU seams with shared fixtures

**What:** Reuse `PackedCamera` fixtures and pixel-center samples across `tests/test_offline_shared_scene_renderer.cpp` and `tests/test_optix_direction.cpp` so both paths prove the same world-ray contract. [VERIFIED: `tests/test_offline_shared_scene_renderer.cpp`, `tests/test_optix_direction.cpp`]

**When to use:** Use this because both files already exercise the right seams; the missing work is coordination, not infrastructure. [VERIFIED: `src/common/camera.h`, `src/realtime/gpu/programs.cu`]

### Pattern 3: Treat image comparisons as smoke, not the source of truth

**What:** Keep `tests/test_reference_vs_realtime.cpp` and `tests/test_viewer_quality_reference.cpp` coarse and scene-level, using them to prove the stronger contracts still produce sane images rather than to prove mathematical parity. [VERIFIED: `tests/test_reference_vs_realtime.cpp`, `tests/test_viewer_quality_reference.cpp`]

**When to use:** Use this because the user explicitly locked ray-level consistency as the main acceptance contract. [VERIFIED: `.planning/phases/05-camera-model-regression-coverage/05-CONTEXT.md`]

---
## RESEARCH COMPLETE
