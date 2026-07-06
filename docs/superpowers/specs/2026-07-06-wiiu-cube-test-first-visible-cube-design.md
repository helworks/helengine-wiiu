# Wii U `cube_test` First Visible Cube Design

## Goal

Render the single cube from the loaded `cube_test` scene on Wii U through GX2 as a stable, visible, flat-colored 3D mesh.

This slice proves the runtime scene-loading seam and the first real 3D geometry path without waiting for the full Wii U 3D renderer architecture.

## Current State

- The Wii U GX2 presenter is proven for:
  - clear-only output
  - a simple square
  - a translated diagnostic triangle
- `WiiUApplication` is still pinned to a diagnostic present path.
- `WiiURenderManager3D` still returns placeholder materials and placeholder runtime models.
- `cube_test` loads through the real runtime scene/bootstrap path, but that runtime data is not yet rendered as 3D geometry on Wii U.

## User-Approved Constraints

- The first milestone should target the real `cube_test` scene data, not hardcoded cube geometry.
- The scene contains only one mesh, a cube.
- Success means the cube is visible and stable in Cemu; authored camera/material correctness is not required yet.
- The preferred path is:
  - pull the cube mesh from loaded `cube_test` runtime data
  - render it through a simple temporary GX2 path
  - defer the full Wii U 3D renderer

## Approaches Considered

### 1. Temporary runtime-data-to-GX2 bridge

Extract the first runtime mesh from the loaded scene and feed it into a small presenter-owned flat-color mesh draw path.

Pros:
- smallest proof of the real `cube_test` seam
- isolates mesh extraction from the unfinished renderer architecture
- keeps the debugging surface narrow

Cons:
- introduces temporary bridge code that will later be deleted or absorbed

### 2. Immediate full Wii U 3D renderer bring-up

Route `cube_test` through the real Wii U 3D renderer from the start.

Pros:
- cleaner long-term architecture if it works immediately

Cons:
- mixes too many unknowns at once
- slower to debug
- higher chance of thrashing on camera, material, and renderer issues simultaneously

### 3. Hardcoded visible cube

Replace the triangle with hardcoded cube geometry.

Pros:
- quickest possible visible 3D output

Cons:
- does not prove the `cube_test` runtime seam
- misses the actual problem we need to solve

## Decision

Use approach 1.

The first Wii U `cube_test` 3D slice will read the real cube mesh from loaded runtime scene data and render it through a temporary flat-color GX2 path owned by the presenter.

## Scope

### In Scope

- load `cube_test`
- extract the single cube mesh from the real runtime model data
- upload cube geometry to GX2
- render the cube as a flat-colored visible mesh
- use a forced simple camera/view-projection that guarantees visibility

### Out Of Scope

- authored materials
- texture sampling
- lighting
- authored camera parity
- generalized multi-mesh scene rendering
- generalized material system support
- final Wii U 3D renderer architecture

## Runtime Design

### Application Flow

`WiiUApplication` remains on a controlled bring-up path while the 3D seam is proven.

Instead of presenting the translated diagnostic triangle forever, the application will:

1. initialize the generated runtime as it does now
2. load `cube_test`
3. obtain the first available runtime 3D model for the active scene
4. hand that model's first mesh geometry to a temporary Wii U presenter bridge
5. present that mesh through a dedicated scene-cube draw path

This keeps the application control flow simple while replacing synthetic geometry with real scene geometry.

### Runtime Model Geometry Seam

`WiiURuntimeModel` will stop being a pure placeholder and will expose the minimum data needed for this milestone:

- first mesh vertex positions
- first mesh indices, if the runtime data is indexed
- counts needed for safe GX2 buffer upload

The seam should be explicit and narrow. It is acceptable for this milestone to expose only the first mesh because `cube_test` contains a single cube mesh.

### Presenter Mesh Path

`WiiUGx2Presenter` will gain a dedicated temporary flat-color 3D mesh pipeline, separate from the diagnostic triangle path.

It will own:

- one vertex shader for position-only mesh rendering
- one pixel shader for constant flat color output
- one fetch shader and vertex buffer
- one index buffer when the runtime mesh uses indices
- one uniform buffer for a forced transform/view-projection matrix

The presenter path should be clearly named as a temporary scene-cube bring-up seam so it can be removed or generalized later.

### Camera Strategy

The first milestone will not trust the authored scene camera.

Instead, the presenter will use a forced camera/view-projection that is intentionally simple and visibility-oriented:

- place the cube in front of the camera
- use a conservative projection with a wide enough field of view
- prefer deterministic visibility over correctness

If the cube data is valid, this should guarantee that geometry appears on screen without coupling success to the authored camera system yet.

## Tests And Verification

### Source-Contract Test

Add one focused source-contract test that locks the seam:

- `WiiUApplication` routes rendered presentation through a scene-cube presenter method instead of the triangle method
- `WiiURuntimeModel` exposes the minimum first-mesh geometry access needed by the bridge
- `WiiUGx2Presenter` owns a dedicated flat-color mesh render path distinct from the triangle path

The test should be narrow and source-based, following the existing Wii U runtime seam tests.

### Visual Verification

The final proof for this milestone is Cemu:

- build the `city` Wii U output through the real `build-platform.ps1` path
- launch in Cemu
- confirm one visible stable cube on screen

### Debug Order If It Fails

If the cube does not appear, debug in this order:

1. verify scene mesh extraction returns non-empty position data
2. verify vertex and index counts match the uploaded GX2 buffers
3. verify the forced transform/view-projection places the cube inside clip space
4. if indexed drawing is suspect, temporarily draw the extracted mesh as non-indexed triangles

## Risks

### Temporary Bridge Code

This design intentionally introduces a temporary runtime-data-to-GX2 bridge.

That is acceptable because it isolates the hard seam we need to prove now:

- real scene loading
- real model geometry extraction
- real GX2 mesh presentation

### Runtime Model Uncertainty

The exact geometry layout exposed by the current generated runtime model may not match the assumed seam.

If that happens, the implementation should adapt the seam to the actual loaded runtime data, but still keep the first slice minimal and cube-specific.

## First Implementation Slice

1. Add a failing source-contract test for the scene-cube presenter path and minimal runtime mesh accessors.
2. Add the minimum runtime-model geometry seam needed to read the cube mesh from loaded `cube_test` data.
3. Add a temporary presenter-owned flat-color mesh path and route rendered presentation to it instead of the translated triangle.
4. Build `city` for Wii U and verify one visible cube in Cemu.
5. After the cube is visible, decide whether to generalize the seam or fold it into the real Wii U 3D renderer.
