# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-07-19)

**Core value:** Composed scene, camera, light, and material meaning must stay consistent across offline and realtime rendering, while measured changes improve physical correctness and realtime performance together.
**Current focus:** v2.0 Phase 5 — Scalable Lighting And GPU Scheduling

## Current Position

Phase: 5 of 6 — Scalable Lighting And GPU Scheduling
Plan: Add explicit light distributions, solid-angle PDFs, MIS, ReSTIR DI, persistent launch data, and measured AS update/refit/instancing paths
Status: Active milestone, Phase 4 complete and Phase 5 starting
Last activity: 2026-07-20 - Completed shared CPU/GPU OpenPBR subsurface transport

Progress: [##########] Phase 4 complete; Phase 5 starts with an unbiased direct-lighting/MIS baseline

## v2.0 Phase 1 Evidence

| Profile | Baseline frame / FPS | GPU reconstruction frame / FPS | Change |
|---|---:|---:|---:|
| realtime | 42.8487 ms / 23.3380 | 29.9772 ms / 33.3587 | +42.9% FPS |
| balanced | 43.3536 ms / 23.0661 | 29.7147 ms / 33.6533 | +45.9% FPS |
| quality | 28.9618 ms / 34.5282 | 26.6274 ms / 37.5553 | +8.8% FPS |

Steady-state measurements discard frame zero across 99 measured frames at four cameras and 640x480 on RTX 3090. Realtime denoise critical path is 7.3764 ms while summed four-camera denoise work is 27.0912 ms; non-negative host residual averages 0.0087 ms.

The current built-in protocol uses eight unmeasured warmup frames followed by 100 measured frames with seed `20260718`; CSV frame zero is now a real measured sample and starts at sample stream `20260726`.

| Profile | Avg | P50 | P95 | P99 | FPS | Peak GPU delta |
|---|---:|---:|---:|---:|---:|---:|
| realtime | 31.0712 ms | 30.6497 ms | 34.8491 ms | 36.8067 ms | 32.1841 | 770 MiB |
| balanced | 31.3196 ms | 31.4011 ms | 35.9828 ms | 37.2362 ms | 31.9289 | 770 MiB |
| quality | 27.4086 ms | 27.4561 ms | 30.7860 ms | 32.3865 ms | 36.4849 | 322 MiB |

These final protocol runs validate provenance and artifact completeness, not a new performance uplift: seed, warmup policy, dirty source tree, and thermal/clock state differ from the earlier reconstruction checkpoint. Each profile emitted 100 raw frame rows, 400 per-camera records, a schema-v3 summary, and a manifest with non-empty file sizes and FNV-1a 64 digests.

The temporal reference gate uses a 64x48 `final_room` fixture, four history frames, a 0.03-unit camera translation, a 32-spp raw reference, and a world-space reprojection mask. Temporal motion MAE is `0.153471` versus `0.154568` for the same-seed cold start; across 40 genuinely disoccluded pixels it is `0.161560` versus `0.166292`. Resize, explicit camera-jump reset, and scene change each match a same-seed cold start within `1e-6`, while ViewerRenderSession verifies that a real pose jump requests reset before rendering.

Two identical RTX 3090 performance guard runs kept the benchmark protocol unchanged:

| Run | Avg | P95 | P99 | FPS | Denoise critical |
|---|---:|---:|---:|---:|---:|
| temporal gate A | 29.1951 ms | 33.0931 ms | 34.9069 ms | 34.2523 | 7.1981 ms |
| temporal gate B | 29.7210 ms | 33.5333 ms | 34.4544 ms | 33.6462 | 7.2671 ms |

These paired runs rule out a material regression against the prior protocol record (`31.0712 ms`, P99 `36.8067 ms`) but are not treated as a new uplift claim. The implementation now allocates or resets history before launch parameters capture it and validates history depth against the current world point's distance from the previous camera, rather than comparing depths measured from two different camera origins.

The viewer now exports the final OptiX/denoiser beauty image as a borrowed device view and tone-maps it into CUDA-registered OpenGL pixel buffers. The default path performs no host frame materialization; `--host-readback` is the explicit diagnostic fallback. Hybrid Intel/NVIDIA Linux sessions select the NVIDIA GLX render-offload before GLFW initialization so the OpenGL and CUDA devices match.

Two identical hidden-window product-path comparisons used `final_room`, four 640x480 cameras, the same preview/converge policy, no vsync, and 120 displayed frames. Times include process and renderer startup, so they are end-to-end viewer evidence rather than kernel microbenchmarks:

| Run | GPU interop elapsed / RSS | Host readback elapsed / RSS | Elapsed ratio |
|---|---:|---:|---:|
| viewer A | 2.71 s / 408472 KiB | 9.28 s / 633020 KiB | 3.42x |
| viewer B | 2.60 s / 408876 KiB | 9.32 s / 632596 KiB | 3.58x |

`test_cuda_gl_presenter` verifies the real registered-PBO path and exact RGBA8 display-transfer pixels through a hidden OpenGL context. `test_renderer_pool` verifies a CUDA device pointer, valid stream and resolution, and zero download timing for the borrowed device path. Both presentation modes complete a three-frame viewer smoke, and the full suite passes 56/56.

## v2.0 Phase 2 Evidence

`SceneIRv2` now owns stage-wide `meters_per_unit`, Y/Z up axis, right-handedness, time-code/frame rates, optional time range, interpolation policy, and default prim identity. Prim records use validated absolute paths, parent-derived hierarchy, full local-to-parent affine matrices, reset-stack semantics, inherited visibility/purpose, sorted transform samples, and stable path references for prototypes and materials.

The validator rejects malformed/duplicate paths, missing parents/default prims, non-finite or non-affine transforms, invalid stage/time metadata, bad references, and invalid volume density. Separate backend capability diagnostics report unsupported scale/shear, time samples, reset stacks, and proxy/guide purposes without silently flattening authored meaning.

The existing flat `SceneIR` remains the current CPU/GPU execution model. `compile_legacy_scene_ir_v2` deterministically projects its resources and instances under `/World`, explicitly declares legacy units as one meter per unit, and retains integer indices only as compatibility provenance. YAML loading now creates this v2 projection after includes/imports are composed. Focused semantic/rejection/compatibility tests and the fully rebuilt CUDA/OptiX suite pass 57/57.

`SceneMeshGeometry` now preserves polygon topology, right/left-handed orientation, `none`/Catmull-Clark/Loop/bilinear subdivision schemes, typed arbitrary primvars with OpenUSD interpolation domains and optional independent indices, plus non-overlapping or partitioned material subsets. The legacy compiler carries OBJ/YAML triangle normals, tangents, and UVs into indexed face-varying or vertex primvars, converts builtin quads and boxes without triangulating away authored topology, and reports unsupported backend capabilities explicitly. A full rebuild followed by CTest passes 57/57.

`SceneCamera` now owns the standard OpenUSD projection, filmback/aperture offsets, focal length, clipping range, f-stop, and focus distance contract. The definition-level compatibility compiler converts CPU look-at and realtime body/extrinsic poses from the renderer's +Z-forward/+Y-down frame to OpenUSD's fixed -Z-forward/+Y-up frame, preserves pixel intrinsics and `pinhole32`/`equi62_lut1d` distortion in an explicit capability-diagnosed extension, and marks deterministic fallback poses for legacy placeholder presets. YAML and every builtin definition now compile cameras only after presets are composed. The full rebuilt suite remains 57/57.

`SceneAssetReference` now separates authored, evaluated, and resolved paths without duplicating OpenUSD resolver behavior. `ImageTextureDesc` retains its existing execution `path` and additionally carries the authored token; YAML preserves relative authoring while storing its anchored normalized path, OBJ/MTL preserves `map_Kd` relative to the MTL while retaining the rebased path, and old builtins fall back to the execution path as authored identity. Image texture prims carry this contract with validation and backend capability diagnostics. A full rebuild and all 57 CTest cases pass.

`SceneLight` now preserves the common `UsdLuxLightAPI` color, nit-based intensity, stop-based exposure, size normalization, color-temperature, diffuse/specular, and material-sync semantics together with supported sphere/disk/rect/cylinder/distant/dome shape inputs. Validation rejects non-finite radiometry, out-of-range Kelvin values, invalid shape dimensions, incompatible payload ownership, and exposure overflow; capability diagnostics keep common, analytic, dome, geometry-light, normalization, and color-temperature support explicit. The legacy frontend applies a geometry-light payload to every emissive surface with `materialGlowTintsLight`, intensity 1, exposure 0, and normalization disabled, so the existing material emission remains authoritative and the v1 CPU/OptiX path is unchanged. The full build and all 57 CTest cases pass.

`SceneOpenPbrSurface` now pins the official OpenPBR 1.1.1 MaterialX nodedef names, types, and defaults across base, specular, transmission, subsurface, fuzz, coat, thin-film, emission, and geometry inputs. Typed connections resolve through validated MaterialX `constant`, `checkerboard`, `image`, and `noise3d` color3 nodedefs with explicit UV, wrap/filter, output, and color-space semantics; scalar/vector displacement remains an explicit companion shader and layered energy conservation is part of the authored contract. The compatibility compiler deterministically maps diffuse, metal, dielectric, emissive, and isotropic-volume materials plus every legacy texture variant, retains raw image-byte behavior explicitly, and leaves the v1 CPU/OptiX execution path unchanged. Focused defaults, projection, rejection, reference, HDR, color-space, and backend-capability tests pass within the full 57/57 suite. This closes USD-05 and PBR-01; PBR-02 evaluator behavior and PBR-05 compatibility images remain open.

## v2.0 Phase 3 Evidence

`OpenPbrCoreMaterial` now crosses the production boundary through explicit `adapt_to_cpu_openpbr` and `adapt_to_realtime_openpbr` entry points. Both consume the same SceneIR v2 compatibility material table; missing or duplicate identities, surface/volume mismatches, unsupported connections, displacement, and transmission scattering fail explicitly rather than rendering with ignored parameters. The legacy adapters and default scene factory remain unchanged.

The CPU material and GPU path tracer call the shared OpenPBR emission, evaluate, and sample/PDF implementation. GPU direct lighting evaluates the OpenPBR BSDF and recognizes OpenPBR emissive geometry. `OpenPbrCompiledMaterial` keeps constants plus base/specular/transmission/emission color bindings in an opt-in sidecar buffer while the common `MaterialSample` remains at most 24 bytes, preventing every legacy hit from carrying the OpenPBR block.

The shared host/device color path preserves raw and linear-sRGB values, decodes sRGB texture inputs with the standard piecewise transfer, and applies sampled values only to the four supported color3 inputs. The compiler resolves bindings through compatibility texture identities; ACEScg, non-RGB channels, missing identities, and other connected inputs fail explicitly. CPU proxy textures and OptiX packed textures evaluate the same binding at the hit point.

The deterministic 64x64 reference gate now hits and scatters a connected SceneIR v2 material on CPU, renders it through `OptixRenderer`, and bounds linear RGB disagreement to `5e-4`; separate scenes prove non-Lambertian direct response and OpenPBR emissive-light participation. Host/CUDA transfer tests cover source-to-linear parity, and raw/linear/sRGB plus unsupported cases are independently gated. The full suite passes 60/60 after this slice.

PBR-05 records the exact diffuse, metal, dielectric, emissive, and isotropic-volume translations together with stable warning codes for the three lossy assumptions. A fixed-seed OptiX gate renders the legacy and opt-in OpenPBR paths at 64x64, 16 samples per pixel, and four bounces, then compares the depth-masked subject in linear radiance. Six consecutive runs produced identical metrics: diffuse MAE `9.97737e-05`, metal `0.00866489`, dielectric `0.169882`, and exact emissive/volume agreement. Metal and dielectric P99 errors remain within their declared `0.32` and `1.20` bounds. The fully rebuilt suite passes 61/61, closing Phase 3 while leaving the legacy default path unchanged.

Two fixed-seed default `smoke` captures used four 640x480 cameras, eight warmup frames, 100 measured frames, realtime profile, and no image writes on RTX 3090. They measured `29.996 ms / 33.34 FPS` and `28.022 ms / 35.69 FPS`, compared with the preceding same-protocol `29.1951/29.7210 ms` records. This is no-regression evidence, not a new uplift claim; one run contained system-noise P99 spikes while the second P99 was `32.309 ms`.

## v2.0 Phase 4 Evidence

USD-03 adds an opt-in `RT_ENABLE_OPENUSD` frontend validated against the official OpenUSD `v26.05` monolithic SDK. The SDK-facing translation unit is isolated at OpenUSD's C++17 boundary, while the renderer and all public SceneIR v2 contracts remain C++23. The default SDK-disabled build requires no OpenUSD headers or libraries and fails import with an exact capability message, so YAML/builtin rendering remains the unchanged default.

The importer opens a composed `UsdStage`, preserves stage metadata, affine transforms, reset stacks, visibility, purpose, and transform samples, and traverses instance proxies without leaking unstable generated prototype paths. Sphere and mesh payloads compile into deterministic renderer-owned geometry prototypes; instance surfaces reuse shared prototypes and resolve inherited bindings through `UsdShadeMaterialBindingAPI::ComputeBoundMaterial`. `UsdGeomCamera`, sphere/disk/rect/cylinder/distant/dome `UsdLux` lights, light texture assets, and constant `ND_open_pbr_surface_surfaceshader` inputs compile into their existing SceneIR v2 payloads. Unsupported prim schemas, surface shader ids, authored inputs, and connected OpenPBR inputs fail explicitly.

The curated fixture composes a referenced layer, two instanceable references, a camera, all six supported light schemas, texture assets, and an inherited OpenPBR material binding. The OpenUSD SDK ON targeted CTest passes against `v26.05`; a clean default SDK OFF rebuild and the full CUDA/OptiX suite passed 62/62 at the USD-03 boundary. Implementation commit `592c73d` and fixture-gate commit `4126ecc` provide its review boundary.

USD-04 adds deterministic `.usda` export for the curated SceneIR v2 subset. It authors stage/time metadata, hierarchy and affine samples, instanceable class-backed sphere/mesh prototypes, cameras, all six supported UsdLux schemas, authored light assets, resolved material bindings, and constant OpenPBR surface inputs. Compiler-internal SceneIR prims stay private, and unsupported connected inputs, displacement, volumes, geometry lights, camera calibration extensions, mesh primvars/subsets, and non-USDA destinations fail explicitly.

The round-trip gate exports the composed fixture twice and requires byte-identical output, reimports it and compares stage metadata plus every prim/payload semantically, then requires the canonical re-export to remain byte-identical. The fixture now covers left-handed mesh topology in addition to sphere instances, hierarchy, sampled transforms, camera, six light types, assets, and constant OpenPBR. Official SDK ON and default SDK OFF full builds and CTest both pass 63/63. Implementation commit `65cc2f5` and fixture-gate commit `e853ebf` provide the USD-04 review boundary.

The UsdShade graph compiler uses OpenUSD v26.05 `GetConnectedSources` rather than the deprecated single-source query, rejects invalid/multiple/cyclic sources, and assigns renderer-owned stable texture identities without persisting USD implementation paths. Direct `ND_constant_color3`, `ND_image_color3`, `ND_checkerboard_color3`, and default `ND_noise3d_color3` nodes compile into the existing typed SceneIR v2 texture/connection payloads. Image assets preserve authored/evaluated/resolved identity plus explicit `colorSpace`, address, filter, and fallback values; checker literals become stable constant nodes and shared nodes remain shared.

The connected fixture follows the official MaterialX 1.39.5 nodedef inputs and gates four OpenPBR color connections, constant reuse, checker connected/literal inputs, uniform tiling, explicit sRGB image assets, and default noise. Unknown nodes, NodeGraph interfaces, scalar/vector connections, multiple sources, connected fallbacks, non-uniform checker transforms, missing image color space, and MaterialX inputs that SceneIR cannot preserve remain fail-closed. Official SDK ON and default SDK OFF full builds and CTest both pass 63/63. Implementation commit `6436817` and fixture-gate commit `0056595` provide the review boundary.

The PBR-03 coat/fuzz slice maps the existing SceneIR v2 constants into the compact production sidecar without changing the authored contract or enabling unsupported connected inputs. Coat uses its own anisotropic GGX/VNDF distribution, dielectric IOR Fresnel, OpenPBR roughening of the substrate, absorption on both coat passages, and modulated internal-reflection darkening. Fuzz uses the MaterialX Zeltner LTC rational fits for directional albedo, evaluation, importance sampling, and PDF; both outer layers attenuate the substrate on the incoming and outgoing paths before their own responses are added. Zero layer weights remain exactly compatible with the previous evaluator.

The physical gates pin a MaterialX Zeltner reference point, require pure-fuzz PDF normalization and fitted directional-albedo integration, exercise pure smooth-coat delta transport, match mixed evaluate/sample/PDF values, bound a full-weight coat/fuzz white furnace, verify coat absorption, and run the new cases through a real CUDA host/device comparison. Default SDK OFF and Clang/OpenUSD v26.05 SDK ON builds both pass 63/63 CTest. The stale GCC 13 `build-openusd` directory still rejects the repository's pre-existing C++23 explicit-object member syntax; the supported Clang SDK ON build is green. Implementation commit `e29b14b` and gate commit `02a9a75` provide the review boundary.

The PBR-03 thin-film slice maps the existing OpenPBR weight, micrometer thickness, and IOR inputs into the same compact production sidecar. A shared C++/CUDA Belcour/Barla Airy implementation matches MaterialX 1.39.5's default two-order RGB reference, accounts for coat/base media at the film interfaces, and modulates dielectric and generalized-Schlick metal Fresnel without adding a separate lobe. Reflection probabilities and per-channel diffuse/transmission complements share that result; the thin-walled window combines both interfaces, while the independent coat lobe remains unchanged and attenuates the film-modified base. Zero weight remains exactly compatible.

The physical gates pin MaterialX numeric responses at 300 and 600 nanometers, normalize the mixed thin-film PDF, match sampled/evaluated values, bound film-only and coat-plus-film white furnaces, verify thin-walled reflection/transmission closure, and compare the new case on a real CUDA device. Eight focused SceneIR/OpenPBR/GPU/render-path tests pass. Implementation commit `8c54a6c` and gate commit `59c044f` provide the review boundary.

The PBR-03 dispersion slice maps the existing OpenPBR scale and Abbe-number inputs into the compact production sidecar and removes their former fail-closed boundary. The shared C++/CUDA core derives C/d/F IORs with a two-term Cauchy model, keeps the authored IOR at the 587.6 nm d line, selects refracted RGB channels from path throughput, and uses a channel-mixture PDF for rough transmission. Smooth transmission carries an unbiased one-channel delta weight. GPU path state retains one wavelength context across all bounces and direct-light evaluation, while zero scale executes the previous scalar-IOR path exactly.

The physical gates reconstruct `V_d=20` from the resulting IORs, verify RGB refraction ordering, path-throughput channel probabilities, mixed-PDF normalization, sample/evaluate agreement, bounded white-furnace energy, exact zero-scale compatibility, authored SceneIR mapping, and active-dispersion parity on a real CUDA device. The full default suite passes 63/63. Implementation commit `d28ec14` and gate commit `f55a254` provide the review boundary.

The final PBR-03 subsurface slice maps the existing OpenPBR weight, observed color, scalar radius, RGB radius scale, and anisotropy inputs into the compact production sidecar. Non-thin-walled closed geometry uses bounded volumetric random walks: reciprocal RGB mean free paths, the OpenPBR van de Hulst color-to-albedo inversion, throughput-aware RGB free-flight sampling, Henyey-Greenstein scattering, and dielectric entry/internal-reflection/exit events all share one host/device implementation. CPU rays retain a material-owner token and GPU paths retain the material index so mismatched boundaries terminate explicitly. Zero weight stays on the previous evaluator path and zero radius takes the infinite-density diffuse limit.

Thin-walled subsurface follows the OpenPBR 1.1.1 sheet model instead of starting or rejecting a volume: radius inputs are ignored, while anisotropy splits the observed color between matched diffuse reflection and transmission lobes. Physical gates reconstruct the observed-color mapping, verify RGB free-flight expectation and HG mean cosine, match boundary sampling/PDFs, exercise CPU and OptiX production random walks, bound a white furnace, prove radius-invariant thin-wall behavior, and compare both volume and sheet cases on a real CUDA device. Default SDK-OFF full build and CTest pass 63/63. Implementation commit `4bb164f` and gate commit `fe16ae2` provide the review boundary.

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
- Compile composed OpenUSD stages into a renderer-owned `SceneIR v2`; do not reproduce USD composition locally.
- Use the official MaterialX OpenPBR node definition as the authored material contract.
- Replace the measured CPU clamp with native OptiX temporal AOV denoising before evaluating NRD.
- Fix critical-path/work accounting before accepting any optimization claim.
- Keep raw accumulation/history separate from denoised display output; temporal AOV consumes camera-space normals plus depth-derived flow/trust guides.
- Give each camera an independent renderer, CUDA stream, and denoiser history.
- Keep OpenGL ownership in the viewer layer; expose only a lifetime-bounded CUDA device frame from the renderer and retain host readback as an explicit consumer choice.
- On hybrid-GPU Linux, choose NVIDIA GLX render-offload before GLFW initialization unless the user already supplied an explicit PRIME/GLX route.
- Reset benchmark sample streams and temporal history together so a fixed seed and warmup interval define a reproducible measurement boundary.
- Keep GPU memory explicitly labeled as CUDA device-global observation; report both the high-water mark and peak-minus-baseline delta outside measured frame timing.
- Require correct light/BSDF PDFs and MIS before adding ReSTIR DI; keep GI/PT and neural caching evidence-gated.
- Use bounded random walks for non-thin-walled OpenPBR subsurface and the specification's diffuse sheet limit for thin-walled materials.

### Pending Todos

- Build explicit emissive-geometry and environment sampling distributions with solid-angle PDFs, visibility, and matched light/BSDF MIS as the unbiased Phase 5 baseline.

### Blockers/Concerns

- OptiX temporal AOV still costs about 7.20-7.27 ms on the four-camera critical path, leaving realtime slower than the no-denoise quality profile.
- The benchmark/image-output CLI still downloads beauty, normal, albedo, and depth by design; only the interactive viewer has a no-readback default path.
- CUDA/OpenGL interop requires the OpenGL context and CUDA allocation to use the same NVIDIA GPU; the viewer configures PRIME render-offload on Linux, while unsupported display stacks must use `--host-readback`.
- The first temporal reference fixture covers final_room motion/disocclusion plus exact reset parity; broader multi-scene perceptual coverage remains part of VAL-02 rather than Phase 1 reconstruction closure.
- Single-run P99 varied materially across repeated captures, so future speed claims need repeated identical runs in addition to the now-recorded workload and environment.
- CUDA memory telemetry is device-global rather than per-process; concurrent GPU workloads can perturb baseline and peak values.
- OpenUSD is an optional system SDK dependency with verified OFF/ON paths; supported direct MaterialX color3 graphs compile, coat/fuzz/thin-film/dispersion/subsurface constants execute, and connected coat/fuzz/subsurface colors, NodeGraph interfaces, and fields outside the declared SceneIR subset remain fail-closed.
- Dispersion currently uses fixed C/d/F RGB anchors. Stochastic wavelengths from measured sensor sensitivity curves remain a later spectral-quality extension; the current model is path-consistent but three-band.
- Direct-light sampling still uses the existing center/nearest-point heuristic without solid-angle/area PDFs or MIS. OpenPBR surfaces now use the shared BSDF response and emissive materials participate, but unbiased light transport remains a Phase 5 gate before ReSTIR DI.
- The current `gpt-5.6-*` Codex IDE route still returns HTTP 404 from the built-in web tool. This is not a renderer delivery blocker: bounded public research uses direct read-only HTTP against official primary sources and records the pinned source revision. Do not spend renderer goal turns repairing host search unless the user explicitly reopens that task.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Calibration | Dynamic render-preset-controlled resolution selection | Deferred to v2 | 2026-04-19 |
| Calibration | Explicit per-camera intrinsics instead of derived defaults | Deferred to v2 | 2026-04-19 |
| Calibration | Model-specific distortion coefficients and SE3 extrinsics | Deferred to v2 | 2026-04-19 |
| Advanced reuse | ReSTIR GI/PT and neural radiance caching | Evidence-gated Phase 6 | 2026-07-18 |
| Alternate denoiser | NRD integration | Compare after native OptiX temporal path | 2026-07-18 |
| Hardware-specific | SER and post-Ampere-only paths | Capability-gated; not RTX 3090 acceptance work | 2026-07-18 |

## Session Continuity

Last session: 2026-07-20
Stopped at: Phase 4 is closed with scoped OpenUSD/MaterialX I/O and shared advanced OpenPBR transport; Phase 5 starts with explicit light distributions, solid-angle PDFs, and MIS
Resume file: .planning/milestones/v2.0-REQUIREMENTS.md
