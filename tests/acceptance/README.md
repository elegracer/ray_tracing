# Public USD and OpenPBR Acceptance Assets

The final renderer acceptance suite uses two upstream, version-pinned corpora:

- `usd-wg/assets` at commit `1b91f3c464891af259d51d9ee9ee9e6c357f7079`,
  specifically the CC0 `full_assets/Vehicles` scene. It exercises composed
  layers, references, variants, meshes, textures, and common
  `UsdPreviewSurface` authoring.
- `AcademySoftwareFoundation/OpenPBR` at commit
  `f8d6d947dfae4c9b599965a86c22826ea7a8dbfb` (`v1.1.1`), specifically all 83
  Apache-2.0 `examples/*.mtlx` materials.

The upstream files are downloaded into the ignored `.cache` directory. The
repository stores only the source identity, exact revisions, Git tree ids,
file/byte counts, representative SHA-256 values, licenses, and required final
gates in `public_assets.lock.cmake`.

Fetch and verify the corpora with:

```bash
cmake --build build-clang-vcpkg-settings --target fetch_public_acceptance_assets
cmake --build build-clang-vcpkg-settings --target verify_public_acceptance_assets
```

Asset integrity is only the first acceptance layer. Completion of `VAL-02`
requires the product path to import the USD stage, compile the official
OpenPBR materials, render them through the realtime GPU backend, and compare
deterministic linear output against approved references. Unsupported-schema
failures are product gaps, not successful acceptance results.

The required render artifact matrix is locked in `public_assets.lock.cmake`:

- Render `front_three_quarter`, `rear_three_quarter`, and
  `elevated_three_quarter` poses derived from the composed stage bounds with
  both `pinhole32` and `equi62_lut1d` cameras. This produces six single-view
  samples.
- Render `orbit_4_mixed_models` as one four-camera `RendererPool::render_frame`
  submission. Cameras sit at 45, 135, 225, and 315 degree azimuths, all look at
  the stage bounds center, and alternate the two supported camera models.
- Emit both a scene-linear floating-point EXR and a display-transformed PNG for
  every sample: at least 20 images for the ten required views.
- Emit `manifest.json` with source revisions, render settings, sample seed,
  exact world-space camera transforms and intrinsics, output SHA-256 values,
  simultaneous submission id, and linear/perceptual reference error results.

The pose labels are stable acceptance identities; their exact transforms are
fit from the version-pinned composed stage bounds and recorded in the manifest.
Non-empty images alone do not pass: every declared artifact and reference
comparison must be present and valid.

Configure with both `RT_ENABLE_OPENUSD=ON` and
`RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES=ON` after fetching. This adds the corpus
integrity gate, the real `import_openusd_stage` Vehicles probe, compilation of
all 83 official OpenPBR examples through MaterialX, and the full reference
render to CTest:

```bash
cmake --build build-openusd-acceptance --target render_public_acceptance
ctest --test-dir build-openusd-acceptance -R public --output-on-failure
```

`test_public_render_acceptance` writes its deliverable to
`build-openusd-acceptance/public-acceptance-render`: 10 scene-linear EXRs, 10
display PNGs, and `manifest.json`. The repository-approved comparison bundle is
under `tests/references/public_acceptance`. Updating it is an explicit review
operation via `approve_public_acceptance_references`; ordinary test runs never
rewrite approved references.
