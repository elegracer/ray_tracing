# Viewer Scene Selection And ImGui Design

Date: 2026-04-18

## Goal

Add shared scene selection across the CPU offline renderer, realtime CLI, and realtime viewer.

The viewer must support:

- startup scene selection through a command line argument
- runtime scene switching through an ImGui control panel

The scene list should be defined once and reused so the repository no longer keeps separate hard-coded scene option lists in each executable.

## Current State

The repository currently has three separate scene-selection patterns:

- `utils/render_scene.cpp` contains the full offline CPU scene list and dispatch logic
- `utils/render_realtime.cpp` has its own smaller realtime scene selection path
- `utils/render_realtime_viewer.cpp` always uses `make_default_viewer_scene()` and exposes no scene switching

This creates two problems:

- scene option drift across executables
- duplicated scene-selection logic and naming

The viewer also uses a hand-written OpenGL presentation layer with no extensible control surface, which becomes a liability once runtime controls such as denoiser toggles need to be added.

## Scope

This work includes:

1. introducing a shared scene registry for executable-facing scene options
2. making `render_realtime_viewer` accept `--scene`
3. adding runtime scene switching in the viewer through ImGui
4. reusing the shared scene registry in `render_scene.cpp`
5. reducing duplicated scene-selection strings and dispatch code
6. describing `imgui` in `vcpkg_json` without installing dependencies inside the repository
7. wiring CMake to use the vcpkg toolchain path already configured in `.vscode/settings.json`

This work does not include:

- forcing every offline CPU scene to become realtime-capable in this change
- installing or bootstrapping vcpkg inside the repository
- migrating all CPU and realtime scene implementations to one common scene graph
- adding OptiX denoiser or Open Image Denoise controls yet
- building a full editor UI

## Requirements

### Functional

- `render_scene.cpp` scene options must come from a shared definition instead of a private hard-coded list in `main()`
- `render_realtime.cpp` and `render_realtime_viewer.cpp` must also consume that shared scene-definition layer
- `render_realtime_viewer` must accept a `--scene <id>` argument
- the viewer must expose a runtime scene menu through ImGui
- selecting a supported realtime scene in the viewer must rebuild and prepare the new scene without restarting the process
- unsupported realtime scenes may still appear in the UI, but they must be disabled or clearly marked unavailable
- scene-switch failures must keep the old scene active and surface an error message in both the UI and stderr

### Dependency / Build

- `vcpkg_json` is descriptive only in this repository and should be updated to include `imgui`
- the implementation must not install packages locally into this repository
- CMake must rely on the existing global vcpkg toolchain file at `${env:HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake`, matching `.vscode/settings.json`
- build changes should remain minimal and should not introduce an additional dependency-management path

### Product

- the viewer UI should be treated as the foundation for future runtime controls such as OptiX denoiser and Open Image Denoise toggles
- the first UI should stay lightweight and should not replace the existing OpenGL image presentation path

## Options Considered

### Option A: Keep scene selection local to each executable and add ImGui only to the viewer

Pros:

- smallest immediate code change
- viewer menu can land quickly

Cons:

- does not actually solve scene-definition drift
- duplicated strings and dispatch code remain
- future additions still require touching multiple executables

### Option B: Add a shared scene registry and keep backend-specific scene builders behind it

Pros:

- one authoritative list of scene ids and labels
- allows CPU offline and realtime executables to share options without forcing backend unification
- clean base for future viewer controls and capability flags

Cons:

- adds a new registry layer
- slightly larger refactor than a viewer-only patch

### Option C: Fully unify CPU and realtime scenes under one common scene representation

Pros:

- theoretically the cleanest long-term architecture

Cons:

- too large for this task
- current CPU and realtime feature sets do not fully overlap
- would turn a scene-selection change into a renderer architecture rewrite

## Chosen Direction

Use Option B.

Introduce a shared scene registry that owns user-facing scene metadata and capability flags, while CPU and realtime scene construction remain backend-specific.

This reduces duplication now without forcing a broad renderer unification that is out of scope.

## Design

### 1. Shared Scene Registry

Add a small shared module that defines the authoritative scene list used by executable entrypoints and the viewer UI.

Each registered scene entry should include:

- stable `id`, such as `cornell_box`
- display label for UI presentation
- capability flags such as `supports_cpu_render` and `supports_realtime`

This registry is not the renderer implementation. It is the executable-facing source of truth for:

- which scenes exist
- how they are named
- where they should appear
- which backends can currently build them

The registry should include every scene id currently exposed by `utils/render_scene.cpp`, including the `_extreme` variants if they remain first-class CLI options there.

### 2. Backend-Specific Scene Factories

Keep scene construction separate by backend.

The offline CPU renderer and realtime renderer do not currently share the same full feature envelope, so each backend should expose its own scene factory helper:

- CPU path: scene id to offline render function or dispatch helper
- realtime path: scene id to `SceneDescription`

The registry should point callers toward capability checks, while the backend helper remains responsible for actual scene construction.

This keeps the change surgical:

- scene metadata is unified
- renderer internals are not over-unified

### 3. Executable Integration

`render_scene.cpp`, `render_realtime.cpp`, and `render_realtime_viewer.cpp` should all read scene options from the same registry layer.

Expected behavior:

- `render_scene.cpp` uses the shared registry to define accepted scene ids and then dispatches through a CPU helper
- `render_realtime.cpp` uses the shared registry to validate scene selection instead of keeping a private list
- `render_realtime_viewer.cpp` uses the same ids for `--scene` and for the runtime menu

The goal is that adding a new scene option becomes a single registry edit plus whichever backend implementation is actually supported.

### 4. Viewer Runtime Scene Switching

The viewer keeps its current GLFW main loop and OpenGL texture presentation path.

ImGui is added only for controls.

The viewer state should include:

- current scene id
- pending requested scene id
- last scene-switch error message, if any

Runtime scene switching should happen at a safe point in the main loop, not directly inside an ImGui callback.

Switch flow:

1. user selects a scene in ImGui
2. viewer records a pending scene id
3. main loop checks the pending request
4. viewer verifies realtime support
5. viewer builds the new `SceneDescription`
6. viewer packs and prepares the scene in the renderer pool
7. on success, viewer commits the new current scene id
8. on failure, viewer keeps the old scene and reports the error

This avoids interleaving UI events with renderer mutation.

### 5. Viewer ImGui Surface

Use a lightweight always-visible control panel rather than a complex docked editor layout.

The first panel should include:

- current scene display
- scene-selection combo or list
- disabled entries or availability notes for unsupported realtime scenes
- latest scene-switch error text when applicable

The four rendered camera views remain on the existing OpenGL quads. ImGui does not replace that rendering path.

This keeps the first UI narrow while creating the correct extension point for:

- OptiX denoiser toggles
- Open Image Denoise toggles
- render profile controls
- performance counters
- screenshot and debug actions

### 6. Scene Support And Disabled Entries

The viewer menu should expose the full registered scene list, not only the currently supported realtime subset.

If a scene is not realtime-capable yet:

- it should be visible
- it should be disabled or visibly marked
- it should not trigger a scene rebuild attempt

This makes capability gaps explicit and prevents hidden divergence between the viewer and offline tools.

### 7. Spawn Behavior

Realtime scenes should be allowed to define a default spawn pose.

When the viewer changes scenes:

- if the target realtime scene has a defined spawn pose, reset to it
- otherwise fall back to the current default viewer spawn

This prevents scene changes from leaving the body in a nonsensical camera position when scene scales differ.

### 8. Build And Dependency Strategy

The repository should describe `imgui` in `vcpkg_json`.

This repository should not install dependencies locally. The working assumption is that `imgui` is already installed in the developer's global vcpkg tree.

CMake should use the existing toolchain file path:

- `${env:HOME}/vcpkg_root/scripts/buildsystems/vcpkg.cmake`

This matches the current `.vscode/settings.json` configuration and avoids introducing a second dependency workflow.

The build system only needs enough CMake integration to find and link ImGui through that toolchain path.

## File / Responsibility Direction

Expected code organization:

- new shared registry header/source for scene metadata and capability flags
- new or adjusted realtime scene factory helper for `scene_id -> SceneDescription`
- `render_scene.cpp` simplified to use shared scene ids and CPU dispatch helpers
- `render_realtime.cpp` simplified to use the shared registry
- `render_realtime_viewer.cpp` extended with ImGui initialization, control panel, and safe scene switching
- build files updated only where needed for `imgui` linkage and the vcpkg toolchain path

The exact file names can follow existing project conventions, but the responsibility split should remain clear:

- registry owns metadata
- backend helpers own scene construction
- viewer owns UI and runtime switching

## Error Handling

Scene-switch failures should be handled conservatively:

- do not destroy the current active scene unless the replacement has been prepared successfully
- store the error string for display in ImGui
- print the same error to stderr for debugging

Unsupported scenes selected through CLI should produce a clear validation failure before rendering starts.

## Testing

Add focused tests around the new boundaries:

- registry test confirming the offline CPU scene ids are all registered
- realtime scene-factory test confirming supported realtime scenes can build and pack
- viewer-facing test confirming the default scene id is valid and the default scene still packs successfully

Do not add automated ImGui interaction tests in this change. The UI should be verified manually, while automated coverage stays on the scene registry and factory boundaries.

## Success Criteria

This design is successful when:

- scene ids are no longer maintained independently in each executable
- `render_realtime_viewer` supports both `--scene` and runtime ImGui switching
- unsupported realtime scenes are explicit in the viewer UI instead of silently missing
- CMake and `vcpkg_json` describe `imgui` usage without adding local dependency installation steps
- the UI surface is ready to absorb future denoiser controls without another viewer architecture rewrite
