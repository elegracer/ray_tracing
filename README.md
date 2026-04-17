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

Each run directory writes:

- PNG outputs for the smoke render
- `benchmark_frames.csv`
- `benchmark_summary.json`

The CLI prints per-frame timing plus an aggregate FPS summary.

For pure benchmark runs, use:

```bash
./bin/render_realtime --camera-count 4 --frames 3 --profile realtime --skip-image-write --output-dir build/realtime-benchmark
```

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
