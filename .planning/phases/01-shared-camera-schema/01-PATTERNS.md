# Phase 1: Shared Camera Schema - Patterns

## Purpose

Map likely Phase 1 file changes to the closest existing code patterns so planning can reuse current repo structure instead of inventing a parallel schema path.

## File Pattern Map

| Target area | Likely files | Closest analogs | Notes |
|-------------|--------------|-----------------|-------|
| Shared scene schema types | `src/scene/shared_scene_builders.h`, `src/scene/scene_definition.h`, possible new shared camera schema header under `src/scene/` | `src/realtime/camera_models.h`, `src/realtime/camera_rig.h` | Reuse the existing dual-model type vocabulary (`CameraModelType`, `Pinhole32Params`, `Equi62Lut1DParams`) rather than inventing new math structs. |
| Builtin scene preset data | `src/scene/shared_scene_builders.cpp` | existing `CpuPresetRegistryEntry`, `RealtimePresetRegistryEntry`, helper builders like `make_cpu_camera(...)`, `make_realtime_view_preset(...)` | Replace vfov-centric helper outputs with canonical camera-spec construction, but keep the table-driven registration style. |
| YAML parsing and validation | `src/scene/yaml_scene_loader.cpp` | `parse_camera_preset(...)`, `parse_realtime_preset(...)`, `ensure_map(...)`, `ensure_unique_id(...)`, `parse_vec3(...)` | Extend existing parser/validator style; do not add ad hoc YAML access patterns outside the current helper approach. |
| Rig adaptation | `src/realtime/camera_rig.h`, `src/realtime/camera_rig.cpp` | current `CameraRig::Slot` + `PackedCameraRig::pack()` | The rig layer already shows how model-tagged camera data is packed. Adapt from canonical scene schema into this shape instead of making rig structs the new source of truth. |
| Scene catalog / regression tests | `tests/test_yaml_scene_loader.cpp`, `tests/test_shared_scene_regression.cpp`, `tests/test_camera_rig.cpp`, `tests/test_realtime_scene_factory.cpp` | existing structural tests | Keep Phase 1 verification structural: schema parse, builtin-vs-YAML equivalence, mixed-model rig packing. Avoid image-based validation here. |

## Reusable Patterns

### 1. Variant-based camera model data already exists
- `src/realtime/camera_rig.h`
  - `CameraRig::Slot` already uses `std::variant<Pinhole32Params, Equi62Lut1DParams>`.
  - `PackedCamera` already stores both parameter structs plus a `model` tag.
- Planning implication:
  - A scene-level canonical schema should follow this model-tag + per-model-params pattern.
  - Do not plan a generic string-keyed parameter map or YAML-only dynamic bag.

### 2. Scene metadata/preset registration is table-driven
- `src/scene/shared_scene_builders.cpp`
  - `SceneRegistryEntry`
  - `CpuPresetRegistryEntry`
  - `RealtimePresetRegistryEntry`
- Planning implication:
  - Keep builtin migration as table/data refactoring, not a new registration framework.
  - Prefer replacing preset payload types over restructuring the whole registry architecture.

### 3. YAML parsing has strict validation helpers
- `src/scene/yaml_scene_loader.cpp`
  - `ensure_map(...)`
  - `ensure_unique_id(...)`
  - focused parse helpers like `parse_camera_preset(...)` and `parse_realtime_preset(...)`
- Planning implication:
  - Model migration tasks should include dedicated parser helpers and explicit validation failures for missing `model`.
  - Reuse current exception style instead of permissive fallback parsing.

### 4. Builtin and YAML scene equivalence is already a tested pattern
- `tests/test_shared_scene_regression.cpp`
  - compares builtin and YAML definitions for the same scene-level data
- Planning implication:
  - Schema migration should lean on this test style to prove canonical scene definitions stay aligned across both sources.

### 5. Factory layers adapt scene data into runtime rigs
- `src/realtime/realtime_scene_factory.cpp`
  - currently adapts `RealtimeViewPreset` into a `CameraRig`
- `src/core/offline_shared_scene_renderer.cpp`
  - currently adapts `CpuRenderPreset` or `PackedCamera` into the offline `Camera`
- Planning implication:
  - Phase 1 should stop at the schema/adaptation boundary.
  - Do not plan deep behavior changes inside offline or realtime render math yet; only touch them as needed to keep compilation and structural tests valid.

## Recommended Read Order For Executors

1. `src/scene/shared_scene_builders.h`
2. `src/scene/scene_definition.h`
3. `src/scene/shared_scene_builders.cpp`
4. `src/scene/yaml_scene_loader.cpp`
5. `src/realtime/camera_models.h`
6. `src/realtime/camera_rig.h`
7. `src/realtime/camera_rig.cpp`
8. `tests/test_yaml_scene_loader.cpp`
9. `tests/test_shared_scene_regression.cpp`
10. `tests/test_camera_rig.cpp`

## Planning Guidance

- Favor one schema-ownership task early in the plan.
- Split builtin migration and YAML migration into separate tasks so regressions are easy to localize.
- Include at least one task dedicated to test migration/coverage updates.
- Keep runtime renderer semantic expansion out of this phase plan; later phases own offline/realtime behavior changes.

---
## PATTERN MAPPING COMPLETE
