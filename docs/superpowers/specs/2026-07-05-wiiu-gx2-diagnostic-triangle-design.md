# Wii U GX2 Diagnostic Triangle Design

## Summary

The current Wii U runtime has working GX2 presentation, working packaged scene bootstrap, and working menu input. What it does not have yet is an isolated 3D shader bring-up slice.

The next step should not start inside `WiiURenderManager3D` or scene-driven 3D rendering. That would make it harder to distinguish GX2 shader setup failures from engine integration failures. The correct first 3D slice is one presenter-owned diagnostic triangle that proves the Wii U host can load precompiled GX2 shaders, bind a vertex format, and issue one real 3D draw call.

## Goal

Render one static-color diagnostic triangle through GX2 on Wii U using offline-compiled shader binaries and a presenter-owned draw path.

## Non-Goals

- No scene-driven 3D rendering in this slice.
- No `WiiURenderManager3D` feature work beyond compile fixes if strictly required.
- No camera, transform, matrix, or uniform-buffer system in this slice.
- No textures or samplers in this slice.
- No mesh loading, material system, or asset-pipeline redesign in this slice.
- No attempt to render existing authored 3D scenes yet.

## Why This Slice

This slice isolates the exact technical question the user asked: whether Wii U can run our shaders and whether we can compile to it.

The repo and local Wii U research already support the right answer:

1. Wii U homebrew graphics should target `GX2`.
2. Practical Wii U shader workflows use offline-compiled binaries rather than desktop-style runtime shader compilation.
3. The current presenter already loads precompiled GX2 shader blobs for the diagnostic square and UI quad paths.

That means the next useful proof is a minimal 3D shader path, not a broad renderer port.

## Recommended Approach

Add one presenter-owned `RenderDiagnosticTriangleFrame()` path to `WiiUGx2Presenter` that uses one dedicated vertex shader, one dedicated pixel shader, and one small GPU vertex buffer containing clip-space positions plus per-vertex color.

The triangle should be rendered directly by the presenter, parallel to the existing diagnostic square path:

1. initialize triangle shader resources during presenter startup
2. allocate one GX2 vertex buffer for three vertices
3. bind the vertex and pixel shaders
4. bind position and color attributes
5. issue one draw call
6. present the result to TV and DRC

This keeps the first 3D slice entirely inside the already-working GX2 host seam.

## Alternatives Considered

### 1. Presenter-Owned Static-Color Triangle

This is the recommended option.

It proves:

- offline shader-binary loading
- vertex-buffer creation and invalidation
- attribute layout binding
- one non-UI GX2 draw call
- steady-state presentation of a shader-driven frame

### 2. Presenter-Owned MVP Triangle

This would still be isolated, but it introduces one more variable immediately: constant-buffer or uniform setup. That is a reasonable second slice after the static triangle succeeds.

### 3. `WiiURenderManager3D`-Owned Triangle

This is the right long-term seam, but it is the wrong first slice. Any failure could come from scene integration, render-manager ownership, or shader setup. That is too much surface area for first-pass Wii U 3D bring-up.

## Architecture

### 1. Shader Assets

Add one new diagnostic shader pair for the triangle:

- one vertex shader
- one pixel shader

They should follow the same offline-binary pattern already used by:

- `diagnostic_square_shader_bin`
- `ui_quad_shader_bin`

This slice assumes shader binaries are produced outside the runtime and linked into the Wii U build as generated headers or blobs.

### 2. Presenter Ownership

`WiiUGx2Presenter` should own the full triangle path for this slice.

Add:

- one public `RenderDiagnosticTriangleFrame()` method
- one initialization helper for triangle shader and buffer resources
- one shutdown helper for triangle resources
- one draw helper that renders the triangle into a target color buffer

The presenter should remain the only owner of this diagnostic path. `WiiURenderManager3D` stays untouched.

### 3. Vertex Format

Use the smallest useful format:

- `float4` position in clip space
- `float4` color

No index buffer is required. One triangle with three vertices is enough.

The initial geometry should be hard-coded and centered on screen. Per-vertex colors should be distinct enough to prove interpolation is working.

### 4. Frame Path

The presenter should render the triangle over a known clear color. This is important because the result must be obvious in Cemu screenshots and must not depend on any scene content.

The expected frame is:

- solid background clear
- one large visible triangle

No UI composition or menu content should be mixed into this diagnostic path while the slice is being verified.

## Data Flow

1. `WiiUApplication` enters a diagnostic frame path for the triangle slice.
2. `WiiUGx2Presenter` clears the active TV and DRC color buffers.
3. `WiiUGx2Presenter` binds the triangle vertex shader and pixel shader.
4. `WiiUGx2Presenter` binds the triangle vertex buffers and attribute layout.
5. `WiiUGx2Presenter` issues one triangle draw call.
6. `WiiUGx2Presenter` copies the color buffers to the scan buffers and presents them.

## Error Handling

- Fail fast if the triangle shader group cannot load.
- Fail fast if the triangle vertex buffer cannot be created or locked.
- Keep runtime tracing narrow and local to triangle initialization and draw boundaries.
- Do not silently fall back to the square path if triangle resources fail.

The point of the slice is to reveal real GX2 3D bring-up failures, not to hide them.

## Testing Strategy

### Source Tests

Add one focused source-contract test that requires:

- triangle shader blob includes in `WiiUGx2Presenter.cpp`
- one `RenderDiagnosticTriangleFrame()` presenter method
- triangle-specific GX2 resource initialization
- one GX2 draw path for the triangle

The test should reject accidental reuse of the square-only path as the claimed 3D slice.

### Runtime Verification

Run the smallest practical runtime check:

1. build the Wii U `city` artifact
2. launch it in Cemu
3. confirm a stable colored triangle is visible
4. confirm the frame still runs at steady state
5. capture a screenshot for verification

## Risks

- The offline shader-binary toolchain may need one extra build step or asset-generation convention not yet encoded in this repo.
- Attribute layout mismatches may produce a blank or corrupted frame even when shader loading succeeds.
- The presenter may need explicit GX2 state reset or invalidation around the triangle draw if it conflicts with existing UI-path assumptions.

These are acceptable risks because they remain confined to one host-owned GX2 seam.

## Success Criteria

- The Wii U build loads one dedicated diagnostic triangle shader pair.
- The presenter issues one real GX2 triangle draw call.
- Cemu shows a stable colored triangle on screen.
- The slice remains presenter-owned and does not yet broaden into scene or render-manager integration.
- The result gives a reliable base for the next slice: uniforms, depth, and then `WiiURenderManager3D`.
