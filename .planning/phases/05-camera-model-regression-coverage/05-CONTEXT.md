# Phase 5: Camera Model Regression Coverage - Context

**Gathered:** 2026-04-21
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase closes the milestone by proving that the supported camera models remain numerically aligned with the reference math and contractually consistent across offline CPU and realtime GPU/viewer paths after fisheye became the default. It covers automated math-parity tests against the reference headers, cross-path direction-level contract checks for equivalent cameras, and a widened regression slice that keeps default-fisheye fallout visible across render, scene, viewer, and OptiX paths. It does not introduce new camera behavior, new calibration surfaces, or a broader image-quality evaluation framework beyond what is needed to prove the camera-model contract.

</domain>

<decisions>
## Implementation Decisions

### Reference parity strength
- **D-01:** Phase 5 should verify `pinhole32` and `equi62_lut1d` against the reference headers at the math and ray-direction level rather than relying only on roundtrip or qualitative behavior checks.
- **D-02:** The parity coverage should use stable authored parameter sets and representative pixel samples, with explicit tolerances chosen per model.
- **D-03:** `pinhole32` may use tighter tolerances than `equi62_lut1d`, which can allow a small amount of LUT interpolation error while still proving alignment to the reference implementation.

### Offline / realtime consistency contract
- **D-04:** Phase 5 should treat shared camera contract preservation as the primary acceptance target for `VER-03`.
- **D-05:** For equivalent `PackedCamera` inputs, offline CPU and realtime GPU paths should agree on the selected camera model, derived default intrinsics, and world-space ray directions at the same pixel centers.
- **D-06:** End-to-end render comparisons may remain as smoke coverage, but they are not the main proof target for this phase.

### Coverage matrix after fisheye default
- **D-07:** The required coverage matrix includes explicit `pinhole32`, explicit `equi62_lut1d`, helper-derived default `equi62_lut1d` intrinsics, and the regression that explicitly authored `pinhole32` scenes or presets remain pinhole after the project default switched to fisheye.
- **D-08:** Mixed-model four-camera rigs remain supported, but Phase 5 does not need to exhaustively re-prove the full mixed-model matrix if existing Phase 3 regressions already cover that contract.

### Final regression slice boundary
- **D-09:** Phase 5 should use a wider mandatory regression slice rather than a narrow camera-math-only slice.
- **D-10:** The phase-level regression slice should include at least `test_camera_models`, `test_offline_shared_scene_renderer`, `test_optix_direction`, `test_reference_vs_realtime`, `test_viewer_quality_reference`, `test_realtime_scene_factory`, `test_shared_scene_regression`, `test_viewer_scene_reload`, and `test_optix_equi_path_trace`.
- **D-11:** Additional nearby tests may be included if implementation naturally touches them, but the widened slice above is the minimum planning target for `VER-04`.

### Production-change boundary
- **D-12:** Phase 5 should prefer adding or tightening automated tests in existing test binaries over creating many new dedicated test executables.
- **D-13:** Production camera logic should only change if the new parity or regression coverage exposes a real contract bug; otherwise this phase remains verification-focused.

### the agent's Discretion
- The exact representative pixel samples and parameter sets used for reference-header parity coverage, as long as they are stable and materially prove the contract.
- The exact tolerance values for `pinhole32` versus `equi62_lut1d`, as long as the rationale is grounded in the underlying analytic versus LUT-based math.
- The exact distribution of new assertions across existing test files, as long as the phase stays verification-focused and the widened regression slice remains executable.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Reference camera math
- `docs/reference/src-cam/cam_pinhole32.h` — Canonical pinhole reference implementation and parameter slots to match.
- `docs/reference/src-cam/cam_equi62_lut1d.h` — Canonical equidistant fisheye LUT reference implementation and parameter slots to match.

### Prior locked camera-contract decisions
- `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md` — Offline rectangular fisheye output and contract-preservation decisions.
- `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md` — Realtime/viewer contract preservation and mixed-model rig behavior.
- `.planning/phases/04-default-intrinsics-and-fisheye-defaults/04-CONTEXT.md` — Default fisheye switch and shared default-intrinsics derivation rules.

### Existing regression and parity anchors
- `tests/test_camera_models.cpp` — Current model-level projection, unprojection, and default-intrinsics coverage.
- `tests/test_offline_shared_scene_renderer.cpp` — Existing offline shared-scene camera seam and model-switching regressions.
- `tests/test_optix_direction.cpp` — Existing GPU direction-debug coverage aligned to shared camera math.
- `tests/test_reference_vs_realtime.cpp` — Existing coarse offline versus realtime comparison harness that may need stronger contract assertions.
- `tests/test_viewer_quality_reference.cpp` — Viewer CPU reference versus OptiX regression coverage.
- `tests/test_realtime_scene_factory.cpp` — Realtime preset and packed-camera contract coverage.
- `tests/test_shared_scene_regression.cpp` — Broader shared-scene regression coverage after the default switch.
- `tests/test_viewer_scene_reload.cpp` — File-backed viewer reload coverage that should stay green under the widened regression slice.
- `tests/test_optix_equi_path_trace.cpp` — Existing live OptiX fisheye rendering regression that should remain in the required Phase 5 slice.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tests/test_camera_models.cpp` already exercises both supported camera models and the shared default-intrinsics helper, making it the clearest place to anchor direct reference-header parity checks.
- `tests/test_optix_direction.cpp` already exposes realtime GPU ray directions in a deterministic way, so it is the most stable existing seam for CPU versus GPU world-ray agreement.
- `tests/test_offline_shared_scene_renderer.cpp` already proves the offline shared-scene path consumes canonical camera data, which makes it the natural offline side of the cross-path contract.

### Established Patterns
- The project already uses `PackedCamera` and shared camera-model helpers as the canonical seam between authored scene data and both render backends, so Phase 5 should keep proving that seam rather than inventing a second abstraction.
- Existing Phase 3 and Phase 4 work already covers mixed-model rigs and default-fisheye behavior at the contract level, so Phase 5 should extend those regressions selectively instead of re-solving earlier phases.
- Current image-level comparisons are intentionally coarse and noisy compared with direction-level checks, so the stable verification surface is still ray math and contract preservation.

### Integration Points
- `src/realtime/camera_models.cpp` is the implementation under direct parity scrutiny for `VER-01` and `VER-02`.
- `src/core/offline_shared_scene_renderer.cpp` and `src/common/camera.h` are the offline-side contract seam for ray generation.
- `src/realtime/gpu/programs.cu`, `src/realtime/gpu/launch_params.h`, and `tests/test_optix_direction.cpp` are the realtime-side contract seam for camera rays.
- `src/scene/shared_scene_builders.cpp`, `src/realtime/realtime_scene_factory.cpp`, and `src/realtime/viewer/four_camera_rig.cpp` remain relevant because the widened regression slice must keep default-fisheye and explicit-pinhole behavior intact.

</code_context>

<specifics>
## Specific Ideas

- Add direct reference-parity assertions around representative `project/unproject` samples rather than relying only on indirect end-to-end renders.
- Reuse existing deterministic direction-debug seams to prove offline and realtime agree on the same camera contract for both explicit and default-derived camera inputs.
- Keep the phase verification-heavy: strengthen the right existing tests, and only touch production logic if coverage reveals a real mismatch.

</specifics>

<deferred>
## Deferred Ideas

- Exhaustive dense-grid or full-image numerical sweeps against the reference headers are out of scope unless lighter representative parity checks reveal instability.
- A broader image-quality benchmarking framework between offline and realtime remains out of scope for this milestone.
- New calibration surfaces such as explicit distortion tuning, authored extrinsics editing, or dynamic resolution controls remain deferred to the calibration follow-up work.

</deferred>

---
*Phase: 05-camera-model-regression-coverage*
*Context gathered: 2026-04-21*
