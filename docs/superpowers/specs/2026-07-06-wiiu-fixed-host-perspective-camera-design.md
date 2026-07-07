# Wii U Fixed Host Perspective Camera Design

## Goal

Replace the temporary orthographic-looking baked cube rotation with one fixed host-side perspective camera path for the Wii U `cube_test` bring-up.

## Current State

- Wii U boots into `cube_test`.
- The first runtime model loads successfully and is handed to the GX2 presenter.
- The scene-cube shader path is proven only when it avoids GX2 uniform-block transforms.
- Real runtime cube geometry now renders, but only as an orthographic-looking yellow rectangle/cube because vertices are being baked directly into clip-space without a real perspective projection.

## Decision

Use one fixed host-side perspective transform computed on the CPU during scene-cube geometry upload.

Do not restore the GX2 uniform-block transform seam yet.

## Why This Approach

This is the smallest step that produces a real 3D-looking cube while preserving the currently proven shader and presenter path.

It avoids reopening the broken uniform-block seam and avoids mixing authored camera extraction into the first stable perspective milestone.

## Design

### Perspective Path

`WiiUGx2Presenter::ConfigureSceneCubeMesh` will stop baking a simple clip-space rotation and will instead apply one fixed world-view-projection transform on the CPU to the runtime cube positions before uploading them to the no-uniform scene-cube shader path.

### Camera Parameters

The fixed camera should intentionally approximate the authored `cube_test` framing:

- camera positioned roughly at `z = 5`
- moderate perspective projection
- slight yaw and pitch so the cube reads as solid 3D

The exact values do not need authored parity yet; they only need to produce a stable visible perspective cube.

### Shader Contract

The scene-cube shader should remain on the currently proven no-uniform path:

- vertex shader consumes one clip-space `aPosition`
- pixel shader emits the flat yellow color
- no GX2 uniform-block dependency for this milestone

### Scope

In scope:

- replace the current baked orthographic transform with a fixed host-side perspective transform
- keep real runtime cube geometry
- preserve the currently working no-uniform scene-cube shader path

Out of scope:

- GX2 uniform-block repair
- authored camera extraction from scene data
- generalized camera system plumbing
- material, lighting, or depth correctness work beyond what is needed for a stable visible perspective cube

## Verification

- Add one focused source-contract test that locks the host-side perspective seam.
- Rebuild the Wii U artifact.
- Launch in Cemu.
- Expected result: the yellow cube remains visible but reads as perspective 3D rather than a flat orthographic-looking rectangle.
