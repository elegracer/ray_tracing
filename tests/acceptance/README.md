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

Configure with `RT_ENABLE_PUBLIC_ACCEPTANCE_PROBES=ON` after fetching to add
the corpus integrity test and the real `import_openusd_stage` Vehicles probe to
CTest. The probe intentionally remains outside the default compatibility suite:
it must stay red while the product rejects required schemas, and become a
release gate only when the full import-to-realtime path passes.
