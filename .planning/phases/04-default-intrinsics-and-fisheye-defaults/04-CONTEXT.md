# Phase 4: Default Intrinsics And Fisheye Defaults - Context

**Gathered:** 2026-04-21
**Status:** Ready for planning

<domain>
## Phase Boundary

This phase switches default camera construction to `equi62_lut1d` wherever the codebase currently relies on implicit camera defaults, and introduces a deterministic default-intrinsics derivation path based on image resolution plus horizontal FOV. It covers the shared helper(s) and script needed to derive `fx`, `fy`, `cx`, and `cy`, updates default construction paths to use those helpers, and preserves explicit `pinhole32` authored presets wherever they are already intentionally declared. It does not expand the calibration surface beyond v1 defaults, and it does not recompose all existing authored scene framing.

</domain>

<decisions>
## Implementation Decisions

### Default-switch boundary
- **D-01:** Every default construction path that does not already explicitly author a camera model should switch to `equi62_lut1d`.
- **D-02:** The default-switch boundary includes no-argument viewer default rigs, builtin scene helper-generated default camera presets, CLI or utility code paths that rely on default scene cameras, and future default camera helper entrypoints.
- **D-03:** Existing authored presets or YAML scenes that already explicitly specify `model: pinhole32` should remain pinhole and should not be silently migrated just because project defaults switch.

### Intrinsics derivation utility shape
- **D-04:** Phase 4 should provide a single source of truth for default intrinsics derivation: a reusable C++ helper plus a thin script wrapper.
- **D-05:** The script and the runtime code must use the same derivation formula and must not drift into separate implementations.

### Legacy helper migration strategy
- **D-06:** Existing default helper paths that currently derive pinhole intrinsics from `vfov` should be refactored toward a shared `model + resolution + hfov -> fx/fy/cx/cy` derivation path.
- **D-07:** Compatibility wrappers may remain where brownfield code still needs explicit pinhole behavior, but internal default construction should converge on the new horizontal-FOV-driven helper rather than keeping two long-lived default systems.

### Existing explicit pinhole preset treatment
- **D-08:** Repo-owned presets or scenes that already explicitly author `pinhole32` should preserve their current pinhole framing and remain valid after the default switch.
- **D-09:** Phase 4 should change implicit defaults, not rewrite all current authored scene content into fisheye.

### Default intrinsics contract
- **D-10:** The default pinhole horizontal FOV is `90` degrees.
- **D-11:** The default fisheye horizontal FOV is `120` degrees.
- **D-12:** v1 default intrinsics continue to use `cx = width / 2` and `cy = height / 2`.
- **D-13:** v1 default derivation continues to assume zero distortion coefficients for both supported camera models.

### the agent's Discretion
- The exact API boundary between the reusable C++ intrinsics helper and the script wrapper, as long as they share one derivation implementation.
- The exact brownfield compatibility shape for pinhole-specific wrappers while default helper paths converge on the new horizontal-FOV-driven path.
- The exact locations where the new default helper is threaded into builtin scene builders, viewer defaults, and utility entrypoints.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Camera model math and parameter references
- `docs/reference/src-cam/cam_pinhole32.h` — Reference parameterization for pinhole defaults and supported intrinsic/distortion slots.
- `docs/reference/src-cam/cam_equi62_lut1d.h` — Reference parameterization for equidistant fisheye defaults and supported intrinsic/distortion slots.

### Prior locked decisions
- `.planning/phases/01-shared-camera-schema/01-CONTEXT.md` — Canonical shared camera schema and explicit model declaration rules.
- `.planning/phases/02-offline-cpu-camera-models/02-CONTEXT.md` — Offline camera contract and the locked rectangular fisheye output behavior.
- `.planning/phases/03-realtime-gpu-and-viewer-camera-models/03-CONTEXT.md` — Realtime/viewer contract preservation and mixed-model viewer rig behavior.

### Current default-construction code
- `src/scene/shared_scene_builders.cpp` — Current builtin CPU/realtime preset helpers that still derive pinhole defaults from `vfov`.
- `src/realtime/viewer/four_camera_rig.cpp` — Current no-argument viewer default rig path that still fabricates a pinhole default camera spec.
- `src/realtime/realtime_scene_factory.cpp` — Current default scene camera rig construction path that consumes authored presets and runtime resizing.
- `src/realtime/camera_models.cpp` — Existing camera-model math that should stay aligned with the new default intrinsics derivation helper.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/realtime/camera_models.cpp`: Already centralizes pinhole and equi projection/unprojection math, so Phase 4 should add default-intrinsics derivation near this camera-model seam rather than scattering new formulas.
- `src/scene/shared_scene_builders.cpp`: Already owns builtin CPU/realtime preset generation, making it the primary built-in default migration point.
- `src/realtime/viewer/four_camera_rig.cpp`: Already has a local default-viewer camera spec helper, which is the clearest current default pinhole assumption on the viewer side.

### Established Patterns
- File-backed and builtin authored scenes already use explicit `model + fx/fy/cx/cy`, so changing defaults should target implicit helper paths rather than rewriting existing authored scene files by default.
- Runtime resizing in realtime and viewer paths already scales `fx/fy/cx/cy` from authored calibration dimensions, so Phase 4 can treat default derivation as the source for authored base intrinsics rather than inventing another resizing layer.
- CPU and realtime phases have already locked the rectangular fisheye contract and explicit per-camera model selection, so Phase 4 should stay focused on default generation rather than revisiting camera semantics.

### Integration Points
- Builtin default CPU and realtime preset factories in `src/scene/shared_scene_builders.cpp` are where implicit pinhole defaults currently enter repo-owned scene definitions.
- `default_pinhole_viewer_camera_spec(...)` in `src/realtime/viewer/four_camera_rig.cpp` is the clearest viewer-side entrypoint that must flip to fisheye default behavior.
- Utility/CLI entrypoints such as `utils/render_realtime.cpp` inherit scene defaults through `default_camera_rig_for_scene(...)`, so authored default helper migration will naturally affect them once the source helpers are switched.

</code_context>

<specifics>
## Specific Ideas

- Keep the default switch targeted at implicit construction paths, not explicit authored scenes.
- Make the intrinsics script useful for ad hoc numeric checks, but ensure the runtime and the script are mathematically identical.
- Preserve brownfield pinhole scenes by requiring explicit opt-in to fisheye only where the code previously relied on default behavior.

</specifics>

<deferred>
## Deferred Ideas

- Dynamic render-preset-controlled resolution selection remains Phase 5 or later calibration work, not part of Phase 4.
- Explicit authored per-camera intrinsics/distortion/extrinsics editing remains deferred to the calibration follow-up milestone.
- Full migration of all existing authored pinhole scenes or scene framing adjustments into fisheye compositions remains out of scope for this phase.

</deferred>

---
*Phase: 04-default-intrinsics-and-fisheye-defaults*
*Context gathered: 2026-04-21*
