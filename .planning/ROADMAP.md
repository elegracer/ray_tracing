# Roadmap: Ray Tracing

## Archived Milestones

- [x] **v1.0** - Shipped 2026-04-22. Full-chain `pinhole32` and `equi62_lut1d` support across shared scene, offline CPU, realtime GPU, viewer defaults, and regression coverage, with fisheye-first defaults. Archive: [v1.0-ROADMAP.md](/home/huangkai/codes/ray_tracing/.planning/milestones/v1.0-ROADMAP.md)

## Active Milestone

### v2.0 — Physically Grounded Realtime USD Renderer

Requirements: `.planning/milestones/v2.0-REQUIREMENTS.md`
Research: `.planning/v2.0-REALTIME-MODERNIZATION-RESEARCH.md`
Baseline: `.planning/v2.0-BASELINE.json`

- [x] **Phase 1: Truthful Benchmarking And GPU Reconstruction** — critical-path/work metrics, native OptiX temporal AOV denoising, self-contained benchmark provenance/artifacts, temporal reference gates, and GPU-resident CUDA/OpenGL viewer presentation are complete.
- [x] **Phase 2: SceneIR v2 And OpenUSD Semantics** — USD-01 identity/stage/hierarchy/xform, USD-02 geometry/primvar/subset, prototype/instance, camera, asset, UsdLux light, and full USD-05 legacy texture/material compatibility semantics are complete.
- [x] **Phase 3: OpenPBR Core And Physical BSDFs** — the official OpenPBR 1.1.1 authoring contract and shared CPU/GPU evaluator run through an opt-in SceneIR v2 production path with matched sampling, direct response, emission, four supported MaterialX color3 connections, explicit source-to-linear conversion, compact GPU sidecar storage, deterministic linear reference gates, documented legacy mappings, and five fixed-seed compatibility image comparisons.
- [ ] **Phase 4: OpenUSD And MaterialX I/O** — USD-03 optional OpenUSD `v26.05` SDK integration and USD-04 deterministic supported-subset export/semantic round trips are complete; connected shader import and advanced OpenPBR lobes remain.
- [ ] **Phase 5: Scalable Lighting And GPU Scheduling** — explicit light distributions, environment sampling, MIS, ReSTIR DI, persistent scheduling/launch data, and AS update/refit/instancing.
- [ ] **Phase 6: Quality/Performance Closure And Advanced Reuse** — reference corpus, physical/temporal/image gates, NRD comparison, capability-gated ReSTIR GI/PT and neural cache experiments.

## Ordering Rules

- Phase 1 was the entry gate because the original denoise and timing data were not trustworthy; its validated outputs now gate later optimization claims.
- Phase 2 fixes scene semantics before the USD SDK frontend, so YAML and USD compile into one tested contract.
- Phase 3 establishes correct BSDF probabilities before ReSTIR can reuse samples safely.
- Phase 6 techniques remain experimental until the preceding acceptance gates are green.

## Immediate Next Step

Compile supported UsdShade/MaterialX texture graphs into the existing SceneIR v2 connections without weakening fail-closed handling for unsupported nodes or changing the legacy default execution path. Keep advanced OpenPBR lobes as an independent physical-validation gate.
