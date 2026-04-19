# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-19)

**Core value:** Camera behavior must stay consistent across offline and realtime rendering so the same scene and rig produce the intended image no matter which path is used.
**Current focus:** Phase 2 - Offline CPU Camera Models

## Current Position

Phase: 2 of 5 (Offline CPU Camera Models)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-04-20 - Phase 2 discussion completed and context captured

Progress: [██░░░░░░░░] 20%

## Performance Metrics

**Velocity:**
- Total plans completed: 3
- Average duration: 27 min
- Total execution time: 1.3 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Shared Camera Schema | 3 | 80 min | 27 min |

**Recent Trend:**
- Last 5 plans: 01-01, 01-02, 01-03
- Trend: Active

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Treat `pinhole32` and `equi62_lut1d` as first-class per-camera types across shared scene, offline, realtime, and viewer paths.
- Make `equi62_lut1d` the default camera model while preserving explicit pinhole support everywhere.
- Derive v1 default `fx`, `fy`, `cx`, and `cy` from resolution plus horizontal FOV, using `90` degrees for pinhole and `120` degrees for fisheye.
- Lock Phase 1 to a single canonical shared camera schema with explicit `model`, pre-allocated `T_bc`, and model-specific parameter slots.
- Migrate repo-owned builtin and YAML scene data directly to the new schema rather than maintaining project-owned old-format compatibility.

### Pending Todos

None yet.

### Blockers/Concerns

- Brownfield camera changes will touch shared scene, offline, realtime, and viewer code paths, so regression coverage is part of the milestone rather than follow-up polish.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Calibration | Dynamic render-preset-controlled resolution selection | Deferred to v2 | 2026-04-19 |
| Calibration | Explicit per-camera intrinsics instead of derived defaults | Deferred to v2 | 2026-04-19 |
| Calibration | Model-specific distortion coefficients and SE3 extrinsics | Deferred to v2 | 2026-04-19 |

## Session Continuity

Last session: 2026-04-20 00:00
Stopped at: Phase 2 context captured; next step is plan Phase 2
Resume file: .planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md
