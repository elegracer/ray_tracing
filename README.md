# Ray Tracing

## CUDA Realtime Smoke

Configure with the clang + vcpkg toolchain setup from `.vscode/settings.json`, then build and run:

```bash
VCPKG_ROOT=$HOME/vcpkg_root cmake -S . -B build-clang-vcpkg-settings \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg_root/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-13.0/bin/nvcc \
  -DCMAKE_CUDA_HOST_COMPILER=clang++ \
  -DCMAKE_CUDA_ARCHITECTURES=86
cmake --build build-clang-vcpkg-settings --target render_realtime -j
./bin/render_realtime --camera-count 4 --frames 2 --profile realtime --output-dir build/realtime-smoke
bash utils/run_realtime_benchmark_matrix.sh build/realtime-matrix 3
```

The smoke CLI writes PNG outputs plus `benchmark_frames.csv` and `benchmark_summary.json`.

The matrix runner uses `--skip-image-write`, so each run directory keeps:

- `benchmark_frames.csv`
- `benchmark_summary.json`

Realtime benchmark runs execute camera work concurrently and keep host-side denoise bounded to active cameras (`1..4`) with deterministic `camera_index` reporting/output order.

The CLI prints per-frame timing plus an aggregate FPS summary.
`host_overhead_ms` is the residual `frame_ms - (render_ms + denoise_ms + download_ms + image_write_ms)` and may be negative once per-camera stage work overlaps.

For pure benchmark runs, use:

```bash
./bin/render_realtime --camera-count 4 --frames 3 --profile realtime --skip-image-write --output-dir build/realtime-benchmark
```

For correctness-focused `final_room` validation, use:

```bash
./bin/render_realtime --scene final_room --camera-count 4 --frames 1 --profile quality --output-dir build/final-room-check
```

`final_room` is intended for correctness-first checks, not the default benchmark path.
The automated CLI coverage keeps this path lightweight by running a skip-write verification pass separately.

## Optional OpenUSD Stage I/O

The default build keeps the official OpenUSD dependency disabled. To compile and validate
the composed-stage frontend against an OpenUSD installation that exports `pxrConfig.cmake`:

```bash
cmake -S . -B build-openusd -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DRT_ENABLE_OPENUSD=ON \
  -Dpxr_DIR=/path/to/openusd-install
cmake --build build-openusd \
  --target test_openusd_stage_importer test_openusd_stage_exporter -j
ctest --test-dir build-openusd \
  -R '^test_openusd_stage_(importer|exporter)$' --output-on-failure
```

The integration is validated with OpenUSD `v26.05`. The SDK-facing object library stays at
OpenUSD's C++17 language boundary while the renderer remains C++23. SDK-disabled builds retain
the legacy YAML/builtin execution path and report missing I/O capabilities explicitly.

`import_openusd_stage` compiles a composed stage into SceneIR v2. `export_openusd_stage` writes
the deterministic supported subset to `.usda`; the round-trip gate checks byte stability and
semantic equality for metadata, hierarchy, transforms, instances, sphere/mesh geometry, cameras,
supported lights/assets, resolved material bindings, and constant OpenPBR inputs. Unsupported
payloads fail explicitly instead of being discarded.

Connected OpenPBR color3 inputs may resolve directly through MaterialX 1.39.5
`ND_constant_color3`, `ND_image_color3`, `ND_checkerboard_color3`, and default
`ND_noise3d_color3` shaders. Image nodes require explicit USD `colorSpace` metadata; asset,
address, filter, fallback, checker tiling, literal, reuse, and cycle semantics are validated.
NodeGraph interfaces, multiple sources, scalar/vector connections, and fields outside the
declared SceneIR subset remain fail-closed.

## GUI Viewer

Build and run the default interactive viewer with:

```bash
cmake --build build-clang-vcpkg-settings --target render_realtime_viewer -j
./bin/render_realtime_viewer
```

Behavior:

- starts in `final_room`
- shows four pinhole cameras in a `2x2` grid
- mouse controls body `yaw + pitch`
- `WASD` moves the body through the scene

| Quads            | Earch Sphere            | Checkered Spheres            |
| ---------------- | ----------------------- | ---------------------------- |
| ![](./quads.png) | ![](./earth_sphere.png) | ![](./checkered_spheres.png) |

| Perlin Spheres            | Simple Light            | Bouncing Spheres            |
| ------------------------- | ----------------------- | --------------------------- |
| ![](./perlin_spheres.png) | ![](./simple_light.png) | ![](./bouncing_spheres.png) |

| Cornell Smoke            | Cornell Smoke Extreme            |
| ------------------------ | -------------------------------- |
| ![](./cornell_smoke.png) | ![](./cornell_smoke_extreme.png) |

| Cornell Box            | Cornell Box Extreme            |
| ---------------------- | ------------------------------ |
| ![](./cornell_box.png) | ![](./cornell_box_extreme.png) |

| Ray Tracing The Next Week Final Scene | Ray Tracing The Next Week Final Scene Extreme |
| ------------------------------------- | --------------------------------------------- |
| ![](./rttnw_final_scene.png)          | ![](./rttnw_final_scene_extreme.png)          |
