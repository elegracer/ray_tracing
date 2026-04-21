# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-22)

**Core value:** Camera behavior must stay consistent across offline and realtime rendering so the same scene and rig produce the intended image no matter which path is used.
**Current focus:** Next Milestone Definition

## Current Position

Phase: Milestone archived
Plan: None active
Status: Ready for next milestone
Last activity: 2026-04-22 - Archived v1.0 roadmap and requirements, updated project state, and created release tag

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**
- Total plans completed: 15
- Average duration: 21 min
- Total execution time: 2.1 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Shared Camera Schema | 3 | 80 min | 27 min |
| 2. Offline CPU Camera Models | 3 | 46 min | 15 min |
| 3. Realtime GPU And Viewer Camera Models | 3 | 0 min | 0 min |
| 4. Default Intrinsics And Fisheye Defaults | 3 | 0 min | 0 min |
| 5. Camera Model Regression Coverage | 3 | 0 min | 0 min |

**Recent Trend:**
- Last 5 plans: 05-01, 05-02, 05-03, 04-02, 04-03
- Trend: Milestone complete

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Treat `pinhole32` and `equi62_lut1d` as first-class per-camera types across shared scene, offline, realtime, and viewer paths.
- Make `equi62_lut1d` the default camera model while preserving explicit pinhole support everywhere.
- Derive v1 default `fx`, `fy`, `cx`, and `cy` from resolution plus horizontal FOV, using `90` degrees for pinhole and `120` degrees for fisheye.
- Keep implicit/default camera construction fisheye-first while preserving explicit authored pinhole scenes and viewer rigs.
- Lock Phase 1 to a single canonical shared camera schema with explicit `model`, pre-allocated `T_bc`, and model-specific parameter slots.
- Migrate repo-owned builtin and YAML scene data directly to the new schema rather than maintaining project-owned old-format compatibility.

### Pending Todos

No active todos captured for the next milestone yet.

### Blockers/Concerns

No active blockers. The next milestone needs fresh requirement definition before execution work starts.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Calibration | Dynamic render-preset-controlled resolution selection | Deferred to v2 | 2026-04-19 |
| Calibration | Explicit per-camera intrinsics instead of derived defaults | Deferred to v2 | 2026-04-19 |
| Calibration | Model-specific distortion coefficients and SE3 extrinsics | Deferred to v2 | 2026-04-19 |

## Session Continuity

Last session: 2026-04-22 00:00
Stopped at: v1.0 archived; next step is `$gsd-new-milestone`
Resume file: .planning/ROADMAP.md
