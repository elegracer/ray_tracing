# Roadmap: Ray Tracing

## Archived Milestones

- [x] **v1.0** - Shipped 2026-04-22. Full-chain `pinhole32` and `equi62_lut1d` support across shared scene, offline CPU, realtime GPU, viewer defaults, and regression coverage, with fisheye-first defaults. Archive: [v1.0-ROADMAP.md](/home/huangkai/codes/ray_tracing/.planning/milestones/v1.0-ROADMAP.md)
- [x] **v2.0** - Completed 2026-07-21. Physically grounded realtime OpenUSD/OpenPBR product path with validated lighting, ReSTIR DI, GPU scheduling, public assets, and quality-normalized RTX 3090 evidence.

## Active Milestone

### v2.1 — OpenPBR Scalar Data Connections

- [x] Import official MaterialX `ND_constant_float` and `ND_image_float` nodes through OpenUSD with typed single-source validation.
- [x] Compile raw `base_metalness` and `specular_roughness` connections into compact shared CPU/GPU bindings.
- [x] Execute the scalar textures at the hit point in CPU and OptiX paths with a deterministic cross-backend reference gate.
- [x] Preserve the complete default 73-test and official OpenUSD v26.05/public 77-test product paths.

## Completed v2.0 Detail

### v2.0 — Physically Grounded Realtime USD Renderer

Requirements: `.planning/milestones/v2.0-REQUIREMENTS.md`
Research: `.planning/v2.0-REALTIME-MODERNIZATION-RESEARCH.md`
Baseline: `.planning/v2.0-BASELINE.json`
Performance audit: `.planning/v2.0-PERFORMANCE-CLAIMS-AUDIT.json`

- [x] **Phase 1: Truthful Benchmarking And GPU Reconstruction** — critical-path/work metrics, native OptiX temporal AOV denoising, self-contained benchmark provenance/artifacts, temporal reference gates, and GPU-resident CUDA/OpenGL viewer presentation are complete.
- [x] **Phase 2: SceneIR v2 And OpenUSD Semantics** — USD-01 identity/stage/hierarchy/xform, USD-02 geometry/primvar/subset, prototype/instance, camera, asset, UsdLux light, and full USD-05 legacy texture/material compatibility semantics are complete.
- [x] **Phase 3: OpenPBR Core And Physical BSDFs** — the official OpenPBR 1.1.1 authoring contract and shared CPU/GPU evaluator run through an opt-in SceneIR v2 production path with matched sampling, direct response, emission, four supported MaterialX color3 connections, explicit source-to-linear conversion, compact GPU sidecar storage, deterministic linear reference gates, documented legacy mappings, and five fixed-seed compatibility image comparisons.
- [x] **Phase 4: OpenUSD And MaterialX I/O** — all scoped OpenUSD requirements, deterministic round trips, supported connected MaterialX color3 graph import, and shared coat/fuzz/thin-film/dispersion/subsurface transport are complete; the final product path passes 77/77 tests against an SDK built from the official v26.05 tag.
- [x] **Phase 5: Scalable Lighting And GPU Scheduling** — explicit light distributions, environment sampling, MIS, a temporal ReSTIR DI candidate, persistent scheduling/launch data, and measured BVH rebuild/update/refit/instancing are implemented. Strict quality promotion of ReSTIR remains a Phase 6 acceptance item.
- [x] **Phase 6: Quality/Performance Closure And Advanced Reuse** — VAL-03/VAL-04 are closed, unpaired uplift claims are retired, NRD is rejected for the current CUDA/OptiX product path, and one previous-frame ReSTIR DI spatial donor passes strict quality plus 20/20 paired RTX 3090 speed trials.

## Ordering Rules

- Phase 1 was the entry gate because the original denoise and timing data were not trustworthy; its validated outputs now gate later optimization claims.
- Phase 2 fixes scene semantics before the USD SDK frontend, so YAML and USD compile into one tested contract.
- Phase 3 establishes correct BSDF probabilities before ReSTIR can reuse samples safely.
- Phase 6 techniques remain experimental until the preceding acceptance gates are green.

## Immediate Next Step

The v2.1 scalar-material slice is complete and has no unfinished delivery item. Start another slice only after selecting a new measured product bottleneck against the retained quality-normalized RTX 3090 and OpenUSD/OpenPBR acceptance baselines.
