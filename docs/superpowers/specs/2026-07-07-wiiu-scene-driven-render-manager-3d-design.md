# Wii U Scene-Driven RenderManager3D Design

## Goal

Replace the current Wii U `cube_test` presenter shortcut with one scene-driven 3D submission path that captures real `RenderManager3D` output and renders it through GX2 with flat-color shading.

## Current Problem

The current Wii U runtime now boots, enters the real engine loop, and renders again, but its 3D path is still hardcoded around one presenter-owned scene cube flow:

- `WiiUApplication` loads the startup scene.
- `WiiURenderManager3D` caches `LatestRuntimeModel`.
- `WiiUApplication` calls `WiiUGx2Presenter::ConfigureSceneCubeMesh(...)`.
- `WiiUGx2Presenter` renders that special-case mesh every frame through `RenderSceneCubeFrame()`.

That path is good enough for `cube_test` bring-up, but it is not a real engine-integrated 3D renderer. Scene-driven models, transforms, and cameras are not the real source of presented 3D output.

## Approved Scope

This slice will:

- remove the presenter-owned `cube_test` 3D shortcut from steady-state runtime
- capture scene-driven 3D submissions from `WiiURenderManager3D`
- pass one generic 3D frame submission into `WiiUGx2Presenter`
- render scene-owned mesh geometry with entity/world transform and camera
- keep flat-color 3D shading for the first slice
- preserve the current captured 2D overlay path on top of 3D

This slice will not yet:

- use scene materials or textures to drive 3D pixel output
- implement lighting
- support advanced multi-camera behavior
- solve advanced render ordering or material passes

## Architecture

### `WiiUApplication`

`WiiUApplication` should stop owning `cube_test`-specific 3D behavior.

After the change:

- it still initializes the engine core, render bridges, input backend, and presenter
- it still warms the startup scene through one update and one draw
- it no longer configures presenter-owned scene-cube geometry
- it no longer depends on `LatestRuntimeModel` for steady-state presentation
- it continues to run the real full engine loop: update, draw, present

Its job becomes orchestration only, not special-case scene rendering.

### `WiiURenderManager3D`

`WiiURenderManager3D` becomes the Wii U-side capture point for real scene-driven 3D output.

For this slice it should capture:

- resolved runtime model geometry
- resolved per-drawable world transform
- one resolved active camera view/projection contract for the frame

It should build one generic per-frame submission object instead of only keeping `LatestRuntimeModel` as the meaningful presenter input.

`LatestRuntimeModel` may remain temporarily if required by existing code paths, but it should no longer be the presenter-facing steady-state architecture.

### `WiiUGx2Presenter`

`WiiUGx2Presenter` should stop owning a dedicated scene-cube runtime path for steady-state rendering.

Instead it should:

- accept one generic captured 3D frame submission
- upload or bind the captured geometry needed for the frame
- render that captured scene data with the existing minimal flat-color 3D shader path
- render captured 2D UI after the 3D pass

The current `ConfigureSceneCubeMesh(...)` and `RenderSceneCubeFrame()` behavior should be removed from the steady-state runtime path in favor of generic scene submission.

## Data Flow

The target runtime flow for this slice is:

1. The packaged startup scene loads through the normal engine runtime.
2. `EngineCore->Draw()` invokes the normal `RenderManager3D` path.
3. `WiiURenderManager3D` captures the scene's 3D drawables and active camera into one frame submission.
4. `WiiURenderManager2D` captures scene-owned 2D/UI output as it already does.
5. `WiiUGx2Presenter` renders captured 3D first.
6. `WiiUGx2Presenter` renders captured 2D on top.
7. The application presents the final TV/DRC frame.

This preserves the existing engine ordering and moves Wii U closer to the real engine contract instead of a one-off diagnostic pipeline.

## Behavioral Requirements

The first scene-driven 3D slice must immediately support:

- correct mesh selection from the loaded scene
- correct entity/world transform
- correct primary camera framing
- steady-state update/draw behavior inside the real engine loop
- visible 2D overlay rendering on top of 3D

Flat color is acceptable for all 3D geometry in this slice. Material-driven appearance is explicitly deferred.

## Implementation Constraints

- Keep the first slice focused on architecture correction, not shader/material completeness.
- Do not add a fallback toggle for the old presenter-owned cube path.
- Replace the current steady-state shortcut entirely.
- Keep validation minimal and targeted to the seam being changed.
- Prefer source-audit tests for the host/runtime contract, then verify with one real Wii U build and Cemu launch.

## Testing Strategy

### Source-Audit Coverage

Add or update Wii U source-audit tests to prove:

- `WiiUApplication` no longer configures presenter-owned scene-cube geometry during startup
- the steady-state presenter path no longer depends on `RenderSceneCubeFrame()`
- `WiiURenderManager3D` exposes generic captured frame data rather than only the current latest-model shortcut

### Runtime Verification

After source audits pass:

1. Build `C:\dev\helprojs\city\project.heproj` for Wii U.
2. Launch `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb` in Cemu.
3. Confirm the title stays alive in the full engine loop.
4. Confirm the scene still renders.
5. Confirm the runtime trace advances through multiple update/draw frames.

## Why This Slice

This change removes the biggest remaining architectural dead-end in the Wii U runtime without coupling the work to full material or shader support.

It gets Wii U onto the correct host/render-manager/presenter boundary first:

- scene runtime owns scene loading and draw intent
- render manager captures that intent
- presenter only presents captured frame data

Once that seam is correct, later work on materials, textures, lighting, and richer 3D batching can land on top of the right structure instead of more `cube_test`-specific glue.
