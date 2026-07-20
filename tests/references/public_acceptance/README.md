# Public Acceptance References

This directory contains the approved deterministic render references for the
version-pinned public USD Vehicles scene. Each of the ten views has a
scene-linear RGB `FLOAT` OpenEXR image and a display-transformed PNG image.

`manifest.json` records the exact source revisions, renderer settings, seed,
camera transforms and intrinsics, artifact hashes, and reference thresholds.
Regenerate the complete bundle only through the product renderer:

```bash
cmake --build build-openusd-acceptance --target approve_public_acceptance_references
```

The normal `test_public_render_acceptance` path never updates these files. It
renders a fresh bundle and fails if either the linear or perceptual comparison
exceeds the locked threshold.
