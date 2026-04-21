# Phase 5: Camera Model Regression Coverage - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-21
**Phase:** 05-camera-model-regression-coverage
**Areas discussed:** reference parity strength, offline/realtime consistency contract, default-fisheye coverage matrix, final regression slice boundary

---

## Reference parity strength

| Option | Description | Selected |
|--------|-------------|----------|
| A | Verify `pinhole32` and `equi62_lut1d` against the reference headers at the math and direction level using stable samples and explicit tolerances | ✓ |
| B | Use a much larger sweep or near-exhaustive parity grid | |
| C | Keep only existing roundtrip and behavior tests without direct reference anchoring | |
| D | Another parity contract | |

**User's choice:** `1A`
**Notes:** The user wants direct reference anchoring, but does not want Phase 5 to balloon into a full numerical audit.

---

## Offline / realtime consistency contract

| Option | Description | Selected |
|--------|-------------|----------|
| A | Use shared-camera contract preservation as the main proof target: same `PackedCamera`, same pixel center, same world-space ray, with render comparisons remaining smoke coverage | ✓ |
| B | Require broader rendered-image agreement between offline and realtime in this phase | |
| C | Only prove both paths can render each model without ray-level agreement | |
| D | Another consistency contract | |

**User's choice:** `2A`
**Notes:** The user wants Phase 5 to lock the stable camera contract rather than depending mainly on noisier image-level comparisons.

---

## Coverage matrix after fisheye default

| Option | Description | Selected |
|--------|-------------|----------|
| A | Cover explicit `pinhole32`, explicit `equi62_lut1d`, helper-derived default fisheye intrinsics, and explicit authored pinhole preservation after the default switch | ✓ |
| B | Add exhaustive mixed-model four-camera rig matrix coverage | |
| C | Only cover explicit pinhole and explicit fisheye, leaving default-switch regressions to legacy tests | |
| D | Another coverage matrix | |

**User's choice:** `3A`
**Notes:** The user wants Phase 5 to close the default-switch loop without reopening the full mixed-model matrix already handled in Phase 3.

---

## Final regression slice boundary

| Option | Description | Selected |
|--------|-------------|----------|
| A | Use a medium-width slice centered on core camera-contract tests | |
| B | Use a wider required slice that also includes broader shared-scene, viewer reload, and live OptiX fisheye regressions | ✓ |
| C | Use a very narrow slice focused only on newly added parity checks | |
| D | Another regression boundary | |

**User's choice:** `4B`
**Notes:** The user wants the final milestone phase to prove the whole rollout stayed green, not only the narrow seams touched by new assertions.

## Additional constraints

- Prefer extending existing test binaries over creating many new standalone test targets.
- Use model-specific numeric tolerances, with tighter tolerances for `pinhole32` and slightly looser tolerances for `equi62_lut1d` where LUT interpolation applies.
- Treat production code changes as bug fixes only if stronger tests expose a real contract mismatch.

## the agent's Discretion

- Exact sample points, parameter sets, and tolerance values for parity checks.
- Exact mapping of new assertions into existing test files.

## Deferred Ideas

- Exhaustive dense-grid parity sweeps against the reference headers.
- A broader image-quality benchmarking framework between offline and realtime.
- New calibration-editing surfaces beyond the already locked v1 defaults.
