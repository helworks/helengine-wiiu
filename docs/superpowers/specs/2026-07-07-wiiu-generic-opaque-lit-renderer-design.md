# Wii U Generic Opaque Lit Renderer Design

## Goal

Replace the current Wii U flat-color scene-mesh path with one correct generic opaque 3D renderer that uses GPU transforms, scene-owned lighting, Wii U runtime materials, and a build-owned GLSL shader pipeline.

## Current Problem

The Wii U runtime now renders a stable rotating `cube_test` cube again, but the 3D path is still architecturally incomplete:

- 3D geometry is expanded to clip space on the CPU.
- the presenter still behaves like a temporary flat-color scene-cube renderer
- materials are placeholder-only runtime objects
- lighting is ignored
- GLSL source is not yet the authoritative runtime shader source of truth in the normal build flow

That is acceptable for bring-up, but it is not correct enough to match other platforms.

## Approved Scope

This slice will:

- refactor the Wii U 3D path into one generic opaque-lit renderer
- move world/view/projection transforms onto the GPU
- read ambient and directional lights from the scene
- use only the first directional light for now
- fold emissive into the material result
- support opaque materials only
- use material tint only for base color
- fix the GLSL-to-runtime-shader pipeline so normal builds regenerate the correct shader blobs
- keep implementation incremental with visual checkpoints after each visible step

This slice will not yet:

- support textures
- support vertex-color-driven base color
- support transparency
- support normal maps
- support specular highlights
- support more than the first directional light
- support shadowing

## Architecture

### `WiiURenderManager3D`

`WiiURenderManager3D` remains the scene-capture boundary. Its job is to extract render intent from the runtime scene, not to perform GPU work.

For this slice it should capture:

- active camera state
- frame-level ambient light state
- frame-level first directional light state
- one opaque draw command per visible drawable
- the Wii U runtime material associated with each draw

The render manager should stop thinking in terms of temporary scene-cube presentation and instead produce one generic Wii U 3D frame contract.

### `WiiURuntimeMaterial`

Wii U needs one concrete runtime material type instead of the current placeholder-only material seam.

For this slice the runtime material should store only:

- material tint
- emissive color or emissive intensity data
- opaque/lit state needed by the presenter path

This keeps the first correct material contract intentionally narrow while still being scene-driven and renderable.

### `WiiUGx23DRenderFrame`

The 3D frame contract should carry both frame-level lighting state and per-draw material state.

Per frame:

- camera view state
- camera projection inputs
- ambient light
- first directional light

Per draw command:

- runtime model reference
- world matrix
- runtime material reference

That contract becomes the stable handoff from scene extraction to presentation.

### `WiiUGx2Presenter`

`WiiUGx2Presenter` should be refactored away from temporary `SceneCube` assumptions and toward one generic opaque-lit mesh path.

That path should own:

- stable model-space vertex/index buffers
- one transform uniform block for GPU world/view/projection
- one material uniform block for tint/emissive/flags
- one light uniform block for ambient + first directional light
- one opaque lit shader group

The presenter should render generic scene data, not special-case `cube_test` behavior.

### Shader Build Pipeline

The build must treat `tools/wiiu-shaders/*.vs` and `*.ps` as the source of truth.

Normal Wii U builds should:

- detect the shader sources needed by the runtime
- rebuild the binary shader blobs from GLSL
- fail if shader compilation fails

The renderer should not depend on manual shader refresh steps for correctness.

## Data Flow

The target runtime flow is:

1. The engine loads and updates the packaged startup scene normally.
2. `WiiURenderManager3D::Draw()` captures camera, lights, drawables, and runtime materials into one `WiiUGx23DRenderFrame`.
3. `WiiURenderManager2D` captures UI/2D overlay content as it already does.
4. `WiiUGx2Presenter` binds the generic opaque-lit shader path.
5. For each draw command, the presenter uploads the transform block, material block, and light block, then issues the GPU draw.
6. The presenter renders captured 2D UI after the 3D pass.
7. The application presents the composed TV/DRC frame.

This preserves the correct engine ownership model:

- scene/runtime decides what exists
- render manager captures what should be drawn
- presenter and shaders decide how the GPU renders it

## Lighting Model

The first correct Wii U lighting slice should implement:

- ambient scene contribution
- one directional-light Lambert contribution using the first scene directional light
- emissive contribution added after lighting

The intended pixel result is:

- `baseColor = materialTint`
- `lighting = ambient + max(dot(normal, lightDirection), 0) * directional`
- `finalColor = baseColor * lighting + emissive`

This is deliberately limited but correct for the chosen scope.

## Material Model

The first material seam should support only the opaque subset of the Wii U cooked material contract.

Supported:

- opaque materials
- material tint
- emissive
- lit/unlit behavior if represented in cooked Wii U material data

Unsupported in this slice:

- texture sampling
- vertex color blending into base color
- transparency
- specular
- normal mapping

Unsupported features should remain explicit in the Wii U path so failures are diagnosable and future slices have clear scope.

## Error Handling

Correctness takes priority over permissive fallback behavior.

The Wii U renderer should fail explicitly when:

- a draw command has no runtime model
- a draw command has no runtime material
- required GPU uniform buffers or shader resources are missing
- a cooked material cannot be translated into the supported opaque Wii U runtime material contract
- the GLSL shader pipeline fails to produce the runtime shader blob

Where scene light data is absent, the render manager should capture explicit default light values rather than leaving presenter behavior implicit.

Unsupported material features should only be ignored when the Wii U cooked material contract intentionally marks them optional for this slice. Otherwise the build or runtime should fail loudly.

## Incremental Validation

Implementation should proceed in small visible steps with user feedback after each one:

1. Fix the GLSL build pipeline and prove shader source changes flow into the build.
2. Move transforms onto the GPU and prove the current cube still renders correctly.
3. Switch from flat-color shading to the generic opaque-lit shader path.
4. Feed ambient and first directional light from the captured scene.
5. Feed material tint and emissive from Wii U runtime materials.
6. Validate `cube_test` against other platforms.
7. Validate additional main-menu-referenced scenes that use the same opaque path.

Each visual step should be confirmed in Cemu before proceeding to the next one.

## Testing Strategy

### Source-Audit Coverage

Add or update source-contract tests to prove:

- Wii U runtime materials are concrete typed objects rather than placeholders
- the 3D frame carries frame-level lighting and per-draw material state
- the presenter owns transform/material/light uniform uploads for the generic opaque-lit path
- the build pipeline treats GLSL sources as the authoritative shader inputs

### Runtime Verification

After each step:

1. Build `C:\dev\helprojs\city\project.heproj` for Wii U.
2. Launch `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb` in Cemu.
3. Confirm the title remains alive in the full engine loop.
4. Confirm the current visual milestone is correct before advancing.

## Why This Slice

This work prioritizes correctness over speed by fixing the actual renderer architecture instead of extending the temporary flat-color fallback path.

It creates the right long-term boundary:

- generic scene capture
- generic Wii U runtime materials
- GPU-driven transforms
- scene-driven lighting
- build-owned shader compilation

Once that seam is correct, later support for textures, more lights, specular, transparency, and richer material features can land on top of the right renderer structure instead of more temporary special cases.
