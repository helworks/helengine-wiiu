# Wii U Minimal 2D Menu Renderer Design

## Summary

The current Wii U runtime no longer fails during startup. The packaged `DemoDiscMainMenu` scene loads, updates, and draws successfully, but nothing visible from the scene reaches the display. The immediate reason is that the Wii U host still presents a solid OSScreen clear color every frame while the Wii U render bridges remain skeletal. `WiiURenderManager2D` discards rounded-rect, sprite, and text draw requests, and `WiiURenderManager3D` only creates placeholder runtime assets.

This slice implements the smallest renderer that can make the authored `MainMenu` visible again on Wii U. It does not attempt to build the long-term GX2 renderer. It only provides enough 2D rasterization and presentation to render the current menu scene and the scenes referenced by it.

## Goal

Make the packaged Wii U build show the authored `DemoDiscMainMenu` scene in Cemu with visible menu panels, labels, logos, and platform information, using the existing cooked scene and font assets.

## Non-Goals

- No general-purpose Wii U 3D renderer in this slice.
- No full GX2 pipeline in this slice.
- No material, mesh, or model rendering beyond the current placeholders.
- No redesign of menu content or authored scene structure.
- No "best effort" fallback that hides failures. Rendering failures should still surface clearly through diagnostics.

## Current Root Cause

The current runtime trace proves that:

1. Wii U native startup completes.
2. Engine initialization completes.
3. Generated runtime modules register successfully.
4. `DemoDiscMainMenu` is queued successfully.
5. Update and draw frames complete.

The scene is therefore alive. The visible pink screen is caused by host presentation behavior and missing renderer implementation, not by scene bootstrap failure.

## Recommended Approach

Implement a host-owned software 2D presenter for Wii U that composes the menu scene into an ARGB8888 frame buffer and hands that buffer to `PresentFrame()` instead of clearing the screen to the current boot-phase color every frame.

This is the smallest useful path because the authored menu is already a pure 2D scene composed of:

- cameras
- rounded rectangles
- sprites
- text

That means the first visible `MainMenu` does not require real 3D support.

## Architecture

### 1. Wii U 2D Surface Ownership

Add a host-owned 2D surface abstraction inside the Wii U runtime that owns:

- one CPU-writable ARGB8888 back buffer for TV
- one CPU-writable ARGB8888 back buffer for DRC
- clear operations
- pixel write helpers
- rectangle fill helpers
- sprite blit helpers
- bitmap glyph blit helpers

This surface remains private to the Wii U platform layer. It does not change shared engine contracts.

### 2. WiiURenderManager2D Responsibility

Replace the current no-op draw methods with queue-backed draw submission that records exactly the 2D primitives needed by the menu:

- rounded rectangle draw requests
- sprite draw requests
- text draw requests

During `Draw()`, the manager rasterizes those recorded drawables into the host-owned surface using stable painter's-order submission.

This manager remains responsible for runtime texture creation from cooked assets where needed by sprites and text atlases.

### 3. WiiUApplication Presentation Contract

`WiiUApplication::PresentFrame()` must stop overwriting the frame with the boot-phase clear color once engine rendering is active.

The revised contract is:

- before engine initialization succeeds, present diagnostic boot colors exactly as today
- after engine initialization succeeds, present the renderer-owned 2D surface contents
- if rendering fails, return to the failure clear color and keep file-backed diagnostics

This preserves current startup visibility while allowing menu content to appear once the renderer is live.

### 4. WiiURenderManager3D Scope

Keep `WiiURenderManager3D` stubbed for this slice.

It may continue returning placeholder runtime assets as long as:

- it does not block `MainMenu`
- it does not claim to render visible 3D output

If a referenced scene later requires visible 3D content, that becomes a separate slice.

## Data Flow

1. Packaged scene loading remains unchanged.
2. Components submit their existing 2D draw calls into `WiiURenderManager2D`.
3. `WiiURenderManager2D::Draw()` resolves camera-visible 2D submissions and rasterizes them into the Wii U surface.
4. `WiiUApplication::PresentFrame()` copies or flips that composed surface to TV and DRC buffers.
5. Existing runtime trace logging stays active so silent presentation regressions remain diagnosable.

## Rendering Scope

### Rounded Rectangles

Rounded rectangles only need the current authored menu behavior:

- filled body color
- border color and border width when authored
- radius handling that is visually stable, not mathematically perfect

The implementation should prefer deterministic integer rasterization over elaborate anti-aliasing.

### Sprites

Sprites only need the current menu use cases:

- full-texture blit
- alpha blending
- authored size and position

No advanced sampling, rotation, or nine-slice behavior is required unless the current menu scene already depends on it.

### Text

Text must support the current cooked bitmap-font pipeline already used by the menu:

- font atlas texture usage
- glyph placement from cooked font metrics
- per-text color tint
- basic line breaking already encoded by the shared text component layout

This slice does not need rich text, effects, outlines, or kerning improvements beyond what the cooked font data already describes.

## Error Handling

- Keep the existing file-backed runtime trace in `wiiu_runtime_trace.txt`.
- Add narrow render-path tracing only where needed to prove whether primitive submission and presentation occur.
- Throw on invalid renderer inputs instead of silently constructing defaults.
- If cooked texture or font payload resolution fails, let the failure surface through the existing engine exception path and runtime trace.

## Testing Strategy

### Source Tests

Add focused Wii U source tests that guard:

- `WiiUApplication` no longer clears over the rendered frame after engine startup
- `WiiURenderManager2D` no longer leaves `DrawRoundedRect`, `DrawSprite`, and `DrawText` empty
- the renderer slice still keeps `WiiURenderManager3D` intentionally skeletal

### Runtime Verification

Use the smallest practical runtime verification:

1. build the `city` Wii U output
2. launch the produced WUHB in Cemu
3. verify `wiiu_runtime_trace.txt` still shows successful startup and frame progression
4. verify the menu is visibly rendered in Cemu

If visual verification is required and screenshots are not allowed, rely on direct observation during the run rather than captured images.

## Implementation Steps

1. Add source tests that define the renderer and presentation contract.
2. Introduce a small Wii U 2D surface/presenter owned by the platform layer.
3. Implement rounded-rect, sprite, and text rasterization in `WiiURenderManager2D`.
4. Route `PresentFrame()` to the renderer-owned composed surface after engine startup.
5. Rebuild the `city` Wii U artifact and verify in Cemu.

## Risks

- OSScreen pixel layout or stride details may differ from the assumed ARGB8888 write pattern.
- The shared cooked font atlas path may expose a texture-format assumption that the current placeholder runtime texture does not yet preserve.
- Some menu scenes referenced from `MainMenu` may require additional 2D features not exercised by the main menu itself.

These are acceptable risks for this slice because they are tightly scoped and directly testable in Cemu.

## Success Criteria

- `DemoDiscMainMenu` is visibly rendered in the Wii U Cemu run.
- Menu item labels, panel shapes, logo sprites, and platform info text are visible.
- Runtime trace still shows successful startup and frame progression.
- No new startup workaround is introduced to mask rendering failures.
