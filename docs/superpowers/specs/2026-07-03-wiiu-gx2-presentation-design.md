# Wii U GX2 Presentation Design

## Summary

The current Wii U player can initialize the generated core, queue the packaged `DemoDiscMainMenu` startup scene, and complete at least the first update and draw frames. The current visible failure is not scene bootstrap. It is the steady-state presentation path.

The current runtime presents renderer output through `OSScreenPutPixelEx` over CPU-owned software surfaces. That was sufficient to prove the menu scene could be made visible, but it is not a stable long-term presentation seam for Cemu or for the Wii U renderer direction the project actually wants.

This slice replaces only the steady-state presentation seam with GX2 while keeping the current CPU 2D rasterizer intact. The Wii U runtime should continue composing menu output into `WiiUSoftwareSurface`, but the host should upload and present those surfaces through a minimal GX2 path instead of writing pixels directly through OSScreen every frame.

## Goal

Make the current Wii U runtime present the existing CPU-rendered TV and DRC software surfaces through GX2 so the `DemoDiscMainMenu` build keeps rendering reliably in Cemu without depending on OSScreen per-pixel presentation.

## Non-Goals

- No full GX2-native 2D renderer in this slice.
- No full GX2 3D renderer in this slice.
- No redesign of the current `WiiURenderManager2D` software rasterization path.
- No gameplay, scene, or content changes.
- No attempt to solve input in the same slice.

## Current Root Cause

The evidence gathered so far supports these facts:

1. Cemu loads the packaged WUHB successfully.
2. The Wii U runtime trace shows engine initialization, startup scene queueing, and early update/draw completion.
3. The menu can be rasterized into the current software surfaces.
4. The current host still depends on OSScreen per-pixel steady-state presentation.

That means the next correct slice is not renderer authoring or gameplay debugging. It is replacing the presentation seam.

## Recommended Approach

Add one host-owned `WiiUGx2Presenter` seam that owns GX2 initialization and steady-state scan-buffer presentation while leaving the current CPU software renderer untouched.

The revised flow should be:

1. `WiiURenderManager2D` draws into `WiiUSoftwareSurface` exactly as it does now.
2. `WiiUApplication::PresentRenderedFrame()` delegates the TV and DRC surfaces to `WiiUGx2Presenter`.
3. `WiiUGx2Presenter` uploads those surfaces into GX2-owned presentation resources and flips them to the active scan buffers.

This is the smallest change that isolates the problem the user reported while preserving the current renderer work.

## Architecture

### 1. Presenter Ownership

Add one `WiiUGx2Presenter` class under `src/platform/wiiu`.

Its responsibilities are:

- initialize GX2 for steady-state runtime rendering
- allocate and own TV presentation resources
- allocate and own DRC presentation resources
- expose one `Present` method that accepts the current `WiiUSoftwareSurface` instances
- release GX2 resources cleanly during shutdown

It should not own scene logic, draw submission, or CPU rasterization.

### 2. Upload Model

The presenter should treat `WiiUSoftwareSurface` as the source of truth for this slice.

For each frame:

- read the packed ARGB8888 pixels from the TV and DRC software surfaces
- copy or upload them into GX2 presentation resources sized for each display
- issue the minimum GX2 work required to present those resources

This slice should prefer correctness and simplicity over aggressive optimization. If a later slice wants persistent textures, staging buffers, or partial dirty-rect uploads, that belongs to a separate optimization pass.

### 3. Application Contract

`WiiUApplication` should preserve the current boot behavior before the engine is live:

- boot and failure colors may continue using the existing diagnostic path
- once `EngineInitialized` is true, steady-state presentation must stop using `OSScreenPutPixelEx`
- rendered-frame presentation must route through `WiiUGx2Presenter`

This keeps startup diagnostics intact while moving real frame presentation onto GX2.

### 4. Renderer Contract

`WiiURenderManager2D` remains the current CPU renderer for this slice.

It may continue to:

- own texture decode state
- rasterize into `WiiUSoftwareSurface`
- stay independent from GX2 details

That boundary matters. The renderer should not start partially owning GX2 upload concerns in this slice.

## Data Flow

1. The packaged startup scene loads as it does today.
2. Shared-engine 2D drawables submit into `WiiURenderManager2D`.
3. `WiiURenderManager2D::Draw()` rasterizes those drawables into `WiiUSoftwareSurface`.
4. `WiiUApplication::PresentRenderedFrame()` passes the current TV and DRC surfaces to `WiiUGx2Presenter`.
5. `WiiUGx2Presenter` uploads and presents them through GX2.

## Error Handling

- Keep the existing file-backed runtime trace.
- Add narrow GX2 presenter diagnostics only around initialization and frame presentation boundaries.
- Throw when GX2 setup or required presentation resources cannot be created.
- Do not silently fall back to OSScreen steady-state presentation after GX2 activation fails.

If GX2 presentation cannot initialize, that failure should remain visible and diagnosable.

## Testing Strategy

### Source Tests

Add one focused source test first that locks the presentation seam change:

- `WiiUApplication` steady-state rendered presentation should delegate to a GX2 presenter seam rather than performing per-pixel OSScreen presentation inline.

Add narrow supporting source assertions if needed for:

- `WiiUGx2Presenter` file presence and public contract
- steady-state rendered presentation no longer depending on `OSScreenPutPixelEx`

### Runtime Verification

Run the smallest practical runtime verification:

1. build the Wii U `city` output
2. launch the WUHB in Cemu
3. confirm the menu still appears
4. confirm the title continues presenting instead of sticking on the OSScreen path the user reported as broken
5. confirm `wiiu_runtime_trace.txt` still shows clean startup

## Risks

- GX2 setup on Wii U may require additional display-resource initialization details not currently present in the repo.
- The current software-surface pixel layout may need one additional channel-order conversion at upload time depending on the exact GX2 presentation format selected.
- TV and DRC presentation may require separate resource sizing or copy logic that cannot be fully shared.

These are acceptable risks for this slice because they stay within one bounded platform seam.

## Success Criteria

- `WiiUApplication` no longer uses OSScreen per-pixel steady-state presentation for rendered frames.
- The current menu scene remains visible.
- The Cemu run continues presenting through steady state instead of exhibiting the prior OSScreen behavior that motivated this slice.
- No gameplay or renderer-authoring logic is mixed into the presenter seam.
