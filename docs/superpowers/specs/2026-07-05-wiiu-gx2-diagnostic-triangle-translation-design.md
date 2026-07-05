# Wii U GX2 Diagnostic Triangle Translation Design

## Goal

Extend the proven presenter-owned Wii U GX2 diagnostic triangle so the vertex shader consumes one transform uniform and renders the triangle at a fixed off-center translated position. This slice must prove uniform upload and vertex shader constant binding without changing the public runtime seam again.

## Current State

- `WiiUGx2Presenter` already owns a working `RenderDiagnosticTriangleFrame()` path.
- The diagnostic triangle currently uses immutable position and color vertex buffers.
- The current vertex shader writes `gl_Position = aPosition;` directly.
- The runtime currently routes visible output through `RenderDiagnosticTriangleFrame()` inside the `PresentOnly` diagnostic frame loop.
- Cemu has already shown the centered diagnostic triangle, so shader loading, fetch shader setup, attribute binding, and draw submission are already proven.

## Scope

This slice adds only the minimum uniform-driven transform path needed to move the triangle to a fixed upper-right position.

Included:

- one vertex uniform transform path for the diagnostic triangle
- one presenter-owned GX2 uniform buffer for that transform
- one fixed translation matrix value
- one updated diagnostic triangle vertex shader
- one rebuilt diagnostic shader binary
- one focused source-contract test covering the new seam

Excluded:

- rotation
- per-frame animation
- indexed geometry
- cube rendering
- full-engine frame-loop restoration
- scene payload/version fixes

## Design

### Public Runtime Seam

Keep `WiiUApplication` and `WiiUGx2Presenter::RenderDiagnosticTriangleFrame()` as-is from a public API perspective. The runtime routing is already correct for this diagnostic slice and should not churn again.

### Shader Contract

Update `tools/wiiu-shaders/diagnostic_triangle.vs` so the shader declares one transform uniform and multiplies it against the incoming position:

- input position stays at attribute location `0`
- input color stays at attribute location `1`
- output color path stays unchanged
- `gl_Position` becomes `uTransform * aPosition`

The pixel shader remains unchanged.

### Presenter-Owned Uniform Buffer

Add one presenter-owned GX2R uniform buffer dedicated to the diagnostic triangle transform.

Responsibilities:

- allocate once during presenter initialization alongside the triangle vertex buffers
- store one 4x4 transform matrix
- invalidate after CPU writes
- bind during `RenderDiagnosticTriangleToColorBuffer()`
- release during presenter shutdown

This buffer should remain specific to the diagnostic triangle path for now. Generalized uniform ownership can wait until there is a second 3D path that actually needs it.

### Transform Representation

Use one explicit 4x4 float matrix laid out in a fixed, readable form. The initial value should be an identity transform with a translation that moves the triangle into the upper-right quadrant while keeping the full triangle visible.

This slice should not introduce a general matrix math layer. The buffer payload can be authored directly as one fixed matrix constant because the goal is proving uniform plumbing, not building a reusable math API yet.

### Draw Binding

During `RenderDiagnosticTriangleToColorBuffer()`:

- keep the existing clear, shader bind, and attribute buffer bind flow
- bind the diagnostic triangle uniform buffer to the vertex shader uniform block/constant slot required by the compiled shader
- preserve the existing vertex draw call shape

The clear color and vertex colors stay unchanged so the only visible change is the translated triangle position.

## Testing Strategy

Add one focused source-contract test first, then implement to green.

The contract should assert:

- the diagnostic triangle vertex shader declares and uses the transform uniform
- `WiiUGx2Presenter` owns a diagnostic triangle transform buffer
- the presenter initializes and destroys that buffer
- the presenter binds that buffer in the diagnostic triangle draw path

Then:

- run the focused source-contract test red
- implement to green
- rebuild the diagnostic shader binary
- rebuild the native Wii U host
- launch in Cemu
- verify the triangle is visibly shifted into the upper-right relative to the previous centered proof

## Risks

### Uniform Slot Mismatch

The main technical risk is binding the GX2 uniform buffer to the wrong slot or using the wrong shader variable expectations. This risk is contained because the current triangle path is already isolated and visually obvious in Cemu.

### Matrix Layout Mismatch

If the CPU-side matrix layout does not match the shader expectation, the triangle may disappear or move unpredictably. Keeping the matrix fixed and simple reduces the debugging surface for this slice.

### False Progress Through CPU Vertex Changes

It would be easy to fake translation by rewriting vertex positions on the CPU. This design explicitly avoids that so the result genuinely proves shader uniform plumbing.

## Acceptance Criteria

- the diagnostic triangle vertex shader consumes one transform uniform
- `WiiUGx2Presenter` uploads and binds one presenter-owned transform buffer for the triangle
- the runtime still presents through `RenderDiagnosticTriangleFrame()`
- the visible triangle is no longer centered and appears in the upper-right portion of the screen
- Cemu keeps the title alive in the current diagnostic loop
