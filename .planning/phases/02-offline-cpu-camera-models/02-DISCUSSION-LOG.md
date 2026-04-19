# Phase 2: Offline CPU Camera Models - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-20
**Phase:** 02-offline-cpu-camera-models
**Areas discussed:** Offline fisheye output contract, offline/realtime consistency target, offline camera source of truth

---

## Offline fisheye output contract

| Option | Description | Selected |
|--------|-------------|----------|
| A | Keep full rectangular output and follow existing shared-camera fallback semantics for out-of-range fisheye pixels | ✓ |
| B | Keep rectangular output but force out-of-range pixels directly to background | |
| C | Introduce explicit valid-region or mask semantics in this phase | |

**User's choice:** `1A`
**Notes:** The user wants offline fisheye behavior to stay aligned with the current shared camera math contract instead of inventing a separate offline-only invalid-region rule.

---

## Offline/realtime consistency target

| Option | Description | Selected |
|--------|-------------|----------|
| A | Use pixel-center outgoing ray agreement as the primary contract, plus scene regressions that prove model switching changes the offline result | ✓ |
| B | Require stronger full-image parity with realtime already in this phase | |
| C | Only require the offline path to switch models and produce any image | |

**User's choice:** `2A`
**Notes:** The user values cross-path consistency, but for this phase the core acceptance should be correct offline ray generation rather than full offline/realtime image equivalence.

---

## Offline camera source of truth

| Option | Description | Selected |
|--------|-------------|----------|
| A | Make `CameraSpec` / `PackedCamera` the canonical offline camera-model and intrinsics source, while keeping existing pose-style preset fields only as pose input | ✓ |
| B | Keep pinhole on the old offline camera path and bolt fisheye on separately | |
| C | Replace even offline preset pose inputs immediately with explicit SE3-only camera declarations | |

**User's choice:** `3A`
**Notes:** The user wants the shared camera contract to become the true offline camera definition, but does not require the phase to delete existing pose-style preset fields right away.

## Additional constraints

- The user explicitly does not object to preserving current pinhole offline behavior while fisheye v1 ships without depth of field.
- Default fisheye switching stays out of this phase and remains scheduled for Phase 4.

## the agent's Discretion

- Exact internal adapters between the shared camera model math and the legacy offline `Camera` implementation.
- Exact regression test structure, provided model-switch behavior and direction-level correctness are both covered.

## Deferred Ideas

- Explicit fisheye masks or cropped output contracts.
- Richer dynamic calibration and default-model switching.
