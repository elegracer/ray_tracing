# CUDA Realtime Multi-Camera Rendering Design

Date: 2026-04-16

## Goal

Add GPU rendering support to this repository so the system can scale toward realtime rendering on desktop RTX GPUs, with support for `1..4` cameras rigidly attached to a body frame.

Primary target:

- up to `4` active cameras
- each camera at `640x480`
- `30 FPS`
- scene is mostly static, with a small number of dynamic objects
- preserve direct lighting, reflections, refractions, and multi-bounce global illumination
- volumetric media remains an optional extension

The first supported camera models are:

- [`src/cam/cam_equi62_lut1d.h`](/home/huangkai/codes/ray_tracing/src/cam/cam_equi62_lut1d.h)
- [`src/cam/cam_pinhole32.h`](/home/huangkai/codes/ray_tracing/src/cam/cam_pinhole32.h)

Their current code is treated as the mathematical reference for projection and unprojection behavior, even if those headers are not ready to compile inside this repository today.

## Context

The current `master` branch is a compact CPU path tracer centered around the renderer in [`src/common/camera.h`](/home/huangkai/codes/ray_tracing/src/common/camera.h). The `dev/cuda` branch proves basic CUDA toolchain bring-up, but it is not an extension of the current renderer. It replaces most of the existing scene and material system with a minimal CUDA example.

Because of that, the CUDA work should not be designed as a small patch on top of the current CPU renderer internals. It should be designed as a new GPU runtime that shares scene intent and math definitions, while keeping the current CPU renderer as a reference path.

## Non-Goals

- Rewriting the existing CPU renderer into a single shared CPU/GPU implementation
- Making the first GPU version support arbitrary camera counts beyond `4`
- Solving mobile or embedded GPU deployment in the initial design
- Supporting volumes in the first realtime target
- Matching CPU and GPU outputs bit-for-bit

## Requirements

### Functional

- Support `1`, `2`, `3`, or `4` active cameras
- Each camera has:
  - its own model type
  - its own intrinsics
  - its own body-to-camera fixed extrinsic
  - its own output resolution and image buffers
- Support fisheye `equi62_lut1d` and `pinhole32`
- Camera projection and unprojection must remain customizable by model
- Respect SLAM camera convention:
  - camera frame: `x right`, `y down`, `z forward`
- Respect body convention:
  - body frame: `x up`, `y left`, `z back`
- Preserve direct lighting, reflective materials, refractive materials, and multi-bounce GI
- Allow a mostly static world with a small amount of dynamic object updates

### Performance

- Primary design point: desktop RTX 3090 / 4080 Super / 4090 class GPUs
- Use RTX hardware acceleration where it materially improves the path to the target
- Optimize around shared scene data for multiple cameras observing the same world

## Chosen Direction

Use an `OptiX + CUDA` GPU renderer as the realtime path, while keeping the current CPU renderer as a reference implementation.

This is preferred over a pure CUDA custom traversal path because:

- the target hardware includes desktop RTX GPUs
- the requirement is not merely “support CUDA”, but “reach realtime with high image quality”
- using RT cores is the most direct way to make multi-camera GI, reflection, and refraction viable
- `dev/cuda` is useful as a toolchain reference, but not as the future runtime architecture

## Architecture

### 1. Two execution paths

#### CPU reference path

Keep the current CPU renderer for:

- mathematical reference
- debugging and correctness comparison
- small-scene offline validation

It does not carry the realtime performance target.

#### GPU realtime path

Add a new GPU runtime built around:

- CUDA for host/device data movement and support kernels
- OptiX for acceleration structures and ray tracing programs
- RTX hardware traversal where available

This runtime is responsible for realtime multi-camera rendering.

### 2. Shared scene description

The CPU and GPU paths should not share runtime polymorphic objects. They should share a higher-level scene description that can be compiled into backend-specific data.

Shared description includes:

- geometry descriptors
- material descriptors
- texture descriptors
- light descriptors
- camera rig descriptors
- body pose input
- dynamic object descriptors

Compiled outputs:

- CPU backend scene objects
- GPU packed buffers plus OptiX GAS/TLAS structures

### 3. Camera subsystem

The current renderer mixes sampling, image output, recursion, and camera math in one class. The GPU design should separate camera math from the integrator.

Camera subsystem layers:

- `camera_model`
  - math-only projection and unprojection
- `camera_instance`
  - intrinsics, model payload, resolution, mask metadata
- `camera_rig`
  - `1..4` active camera instances
  - fixed body-to-camera extrinsics
- `raygen`
  - pixel to ray conversion using the chosen camera model

### 4. Coordinate systems

The design must make frame conventions explicit and central.

External conventions:

- camera: `x right`, `y down`, `z forward`
- body: `x up`, `y left`, `z back`

Renderer internals should use one explicit world convention. This design fixes the renderer frame to a right-handed convention:

- renderer/world: `x right`, `y up`, `z back`

Input poses and extrinsics are converted into that convention at the interface boundary, not inside scattered device code.

That means:

- a documented body-to-renderer transform
- a documented camera-to-renderer transform
- ray generation computes `dir_cam`, then applies `R_wc`

The implementation must not rely on implicit axis flips hidden inside shaders or helper functions.

For this design, the basis transforms are:

- camera to renderer:
  - `x_cam -> +x_r`
  - `y_cam -> -y_r`
  - `z_cam -> -z_r`
  - matrix form: `R_rc = diag(1, -1, -1)`
- body to renderer:
  - `x_body -> +y_r`
  - `y_body -> -x_r`
  - `z_body -> +z_r`
  - matrix form:
    - first row: `(0, -1, 0)`
    - second row: `(1, 0, 0)`
    - third row: `(0, 0, 1)`

## Camera Model Design

### Common interface

Each camera model must expose GPU-callable math with this conceptual interface:

- `project(dir_cam) -> pixel`
- `unproject(pixel) -> dir_cam`
- `is_valid_pixel(pixel) -> bool`

Here `dir_cam` is a unit direction in the SLAM camera frame.

For rendering, `unproject` is the hot path because ray generation starts from pixels.

### Pinhole32

The reference behavior follows [`src/cam/cam_pinhole32.h`](/home/huangkai/codes/ray_tracing/src/cam/cam_pinhole32.h):

- normalized projection using `x/z`, `y/z`
- Brown distortion with `k1`, `k2`, `k3`, `p1`, `p2`
- final scaling by `fx`, `fy`, `cx`, `cy`

GPU behavior:

- `project` uses the direct formula
- `unproject` subtracts principal point, divides by focal length, iteratively undistorts, then normalizes the 3D direction

Iteration count must be fixed and bounded for realtime predictability.

### Equi62Lut1D

The reference behavior follows [`src/cam/cam_equi62_lut1d.h`](/home/huangkai/codes/ray_tracing/src/cam/cam_equi62_lut1d.h):

- fisheye-style mapping driven by `theta = atan(r)` as the core angular term
- high-order radial distortion
- tangential distortion
- LUT-assisted undistortion behavior

GPU behavior:

- `project` uses the analytic forward mapping
- `unproject` uses precomputed LUT data and interpolation where possible, instead of a heavy per-pixel numerical solve

This matters because fisheye `unproject` is executed once per pixel per camera in the ray generation stage.

### Camera count and rig

The runtime must support:

- active camera count in `[1, 4]`
- per-camera enable or disable
- per-camera model type and parameters
- per-camera body-to-camera extrinsic
- per-camera output and history buffers

All active cameras share the same world and acceleration structures.

## Realtime Rendering Strategy

Use low-sample path tracing with temporal accumulation and denoising.

This is the only practical route that preserves reflections, refractions, and multi-bounce GI while still targeting realtime.

### Integrator

Use an OptiX path tracer with:

- next event estimation
- BSDF sampling
- MIS
- reflection and refraction support
- multi-bounce GI
- Russian roulette

Initial realtime profile:

- `1 spp` baseline, with `2 spp` as an optional higher-cost profile
- total bounce budget around `4..6`
- Russian roulette enabled after the first few bounces

This preserves the required light transport effects without pretending that a brute-force clean image per frame is affordable.

### Multi-camera launch model

The realtime path should treat camera index as a first-class dimension:

- shared scene data
- shared material and texture buffers
- shared OptiX acceleration structures
- per-camera ray generation parameters
- per-camera outputs and history

The system should not build four separate worlds for four cameras.

### Temporal accumulation

Each camera maintains its own history:

- previous radiance
- previous depth
- previous normal
- accumulation length
- reprojection support data

Temporal accumulation is expected to provide most of the visible quality gain for static or mildly dynamic scenes.

When motion, disocclusion, or material changes break reprojection validity, accumulation is reset locally.

### Denoising

Use the OptiX denoiser in the initial design.

Expected inputs:

- beauty
- albedo
- normal

Recommended order:

- low-spp traced result
- temporal accumulation and clamping
- denoising
- final output

### Dynamic objects

The scene assumption is:

- world is mostly static
- only a small number of objects change pose or state per frame

Design consequence:

- static world geometry is built once and reused
- dynamic objects update transforms every frame
- TLAS is updated or refit every frame
- local GAS rebuilds are used only when refit is not sufficient

## Module Breakdown

- `scene_description`
  - backend-neutral scene and rig description
- `cpu_reference`
  - existing CPU renderer kept as reference
- `gpu_runtime`
  - OptiX programs, SBT, GAS/TLAS build/update, tracing, denoiser
- `camera_models`
  - `pinhole32`
  - `equi62_lut1d`
- `camera_rig`
  - active camera list and extrinsics
- `realtime_pipeline`
  - frame execution, accumulation, reset logic, outputs
- `validation`
  - math checks, CPU/GPU comparisons, performance measurement

## Frame Data Flow

Per frame:

1. Receive world state, dynamic object updates, body pose, and active camera set
2. Update dynamic transforms
3. Update or refit TLAS
4. Prepare per-camera ray generation parameters
5. For each active camera pixel:
   - `unproject(pixel) -> dir_cam`
   - transform to world direction
   - trace path
6. Produce auxiliary buffers such as depth, normal, and albedo
7. Reproject and accumulate per camera
8. Denoise per camera
9. Output `N` final images, where `N in [1, 4]`

## Validation Plan

Validation must be layered.

### Math validation

- `project/unproject` round-trip for `pinhole32`
- `project/unproject` round-trip for `equi62_lut1d`
- explicit verification of body/camera/world axis transforms

### CPU/GPU comparison

- compare fixed scenes between CPU and GPU paths
- verify statistical agreement and visual agreement, not exact bitwise equality

### Multi-camera validation

- verify correct output for `1`, `2`, `3`, and `4` active cameras
- verify a shared 3D point projects consistently across different cameras
- verify inactive cameras do not allocate or trace unnecessary work

### Performance validation

Measure at least:

- TLAS update time
- trace time
- denoiser time
- total frame time

Run those measurements for:

- one active camera
- four active cameras

Target hardware class:

- RTX 3090
- RTX 4080 Super
- RTX 4090

## Risks

- Realtime quality depends heavily on scene complexity and motion
- Fast motion and disocclusion will reduce temporal accumulation benefit
- Fisheye unprojection cost can become a hotspot if LUT use is not designed carefully
- Trying to preserve the current CPU object model inside the GPU path would add complexity without helping the target

## Decision Summary

- Keep the current CPU path as a reference renderer
- Build a separate OptiX + CUDA realtime renderer
- Share backend-neutral scene descriptions, not runtime object graphs
- Support `1..4` cameras with shared scene data and per-camera outputs
- Make camera models first-class math components with explicit SLAM frame conventions
- Use low-spp path tracing plus temporal accumulation and OptiX denoising to reach the target
