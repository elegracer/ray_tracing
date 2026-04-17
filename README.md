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
