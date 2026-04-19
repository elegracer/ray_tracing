# Requirements: Ray Tracing

**Defined:** 2026-04-19
**Core Value:** Camera behavior must stay consistent across offline and realtime rendering so the same scene and rig produce the intended image no matter which path is used.

## v1 Requirements

### Camera Models

- [ ] **CAM-01**: Developer can represent each camera in a rig as either `pinhole32` or `equi62_lut1d`.
- [ ] **CAM-02**: Offline CPU rendering uses the selected camera model for ray generation instead of assuming pinhole-only behavior.
- [ ] **CAM-03**: Realtime GPU rendering uses the selected camera model for ray generation instead of assuming pinhole-only behavior.
- [ ] **CAM-04**: Viewer camera rigs and default viewer scene setup preserve the selected camera model for each active camera.
- [ ] **CAM-05**: Both supported camera models accept `fx`, `fy`, `cx`, and `cy`, with v1 distortion coefficients defaulting to zero.

### Scene & Defaults

- [ ] **DEF-01**: Builtin scene definitions and YAML-backed scene configs can declare the intended camera model for each camera.
- [ ] **DEF-02**: Project defaults switch to `equi62_lut1d` for newly constructed/default camera setups.
- [ ] **DEF-03**: Project-wide pinhole defaults remain available, and each camera can still explicitly choose pinhole.
- [ ] **DEF-04**: A utility script can compute default `fx`, `fy`, `cx`, and `cy` from image resolution and horizontal FOV.
- [ ] **DEF-05**: The default pinhole intrinsics are derived from a `90` degree horizontal FOV.
- [ ] **DEF-06**: The default fisheye intrinsics are derived from a `120` degree horizontal FOV.

### Verification

- [ ] **VER-01**: Automated tests verify `pinhole32` math against `docs/reference/src-cam/cam_pinhole32.h`.
- [ ] **VER-02**: Automated tests verify `equi62_lut1d` math against `docs/reference/src-cam/cam_equi62_lut1d.h`.
- [ ] **VER-03**: Automated tests show offline and realtime paths honor the same selected camera model and default intrinsics.
- [ ] **VER-04**: Existing relevant render, scene, and viewer tests continue to pass after fisheye becomes the default.

## v2 Requirements

### Calibration & Presets

- **CAL-01**: Render presets can set output resolution dynamically instead of relying only on hardcoded defaults.
- **CAL-02**: Each camera can load or define explicit `fx`, `fy`, `cx`, and `cy` instead of only derived defaults.
- **CAL-03**: Each camera can configure model-specific distortion coefficients for `pinhole32` or `equi62_lut1d`.
- **CAL-04**: Each camera can configure its body-relative SE3 extrinsic transform through scene or preset data.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Additional camera models beyond `pinhole32` and `equi62_lut1d` | This milestone is specifically about promoting the two reference-backed models to first-class status |
| Runtime/editor UI for arbitrary camera calibration | The initial work is renderer and scene pipeline integration, not calibration tooling |
| Full dynamic camera calibration surface in v1 | Dynamic resolution, intrinsics, distortion, and extrinsics are explicitly deferred to follow-up phases |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CAM-01 | Phase 1 | Pending |
| CAM-02 | Phase 2 | Pending |
| CAM-03 | Phase 3 | Pending |
| CAM-04 | Phase 3 | Pending |
| CAM-05 | Phase 1 | Pending |
| DEF-01 | Phase 1 | Pending |
| DEF-02 | Phase 4 | Pending |
| DEF-03 | Phase 4 | Pending |
| DEF-04 | Phase 4 | Pending |
| DEF-05 | Phase 4 | Pending |
| DEF-06 | Phase 4 | Pending |
| VER-01 | Phase 5 | Pending |
| VER-02 | Phase 5 | Pending |
| VER-03 | Phase 5 | Pending |
| VER-04 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0

---
*Requirements defined: 2026-04-19*
*Last updated: 2026-04-19 after initialization*
