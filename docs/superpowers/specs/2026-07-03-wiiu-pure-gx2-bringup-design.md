# Wii U Pure GX2 Bring-Up Design

## Goal

Replace the current hybrid Wii U presentation experiment with a pure GX2 bring-up path that can be verified in tiny steps inside Cemu.

The immediate milestones are:

1. render a stable GX2 clear-only frame in the packaged Wii U runtime
2. render one hard-coded GX2 square on top of that clear
3. only after both work, reconnect higher-level engine rendering

## Problem Statement

The current runtime proves three things already:

1. the packaged Wii U runtime boots and advances the generated core
2. the software renderer produces non-black pixels in `WiiUSoftwareSurface`
3. GX2 scanout works, because direct GX2 clears show up in Cemu

What is not working is the hybrid seam that locks a GX2 surface and copies CPU-rendered pixels into it row by row. That path stayed black even after reducing the source image to a hard-coded square. Continuing to iterate on that seam is low-confidence and keeps mixing two problems:

1. native GX2 setup
2. software-surface upload semantics

The correct next step is to remove the upload seam from the bring-up path and prove native GX2 rendering by itself.

## Chosen Approach

Adopt a pure GX2 bring-up path with explicit temporary phases inside the Wii U runtime:

1. `clear-only`
2. `clear-plus-square`

During these phases, the runtime will not depend on `WiiUSoftwareSurface` contents for visible output. Instead, it will issue native GX2 commands directly against its TV and DRC color buffers and present those buffers through the existing scan-buffer path.

This keeps the next debugging slice small:

1. if clear-only works, the base GX2 frame lifecycle is valid
2. if square works, basic GX2 draw submission is valid
3. only then does it make sense to reintroduce engine-driven rendering

## Rejected Alternatives

### Keep fixing the CPU upload seam

Rejected because the hard-coded uploaded square still rendered black. That means the seam is already too indirect for early bring-up.

### Revert to OSScreen for visible output

Rejected because the user explicitly wants GX2, and OSScreen would only prove a different presentation path.

### Jump straight to textured menu rendering in GX2

Rejected because it would bundle shader, vertex, texture, and scene integration problems before we have a working native clear-and-primitive baseline.

## Runtime Shape

`WiiUApplication` will keep its boot/runtime ownership role, but the visible rendered frame path will temporarily become a pure GX2 diagnostics path.

The temporary behavior will be:

1. initialize GX2 presenter resources
2. clear TV and DRC to a known background color every frame
3. once clear-only is verified, draw one hard-coded square in clip-space or screen-space using GX2-native buffers and state
4. present through the existing scan-buffer swap path

`WiiUSoftwareSurface` and the 2D software renderer may remain in the tree for now, but they will no longer be part of the visible-frame success path for this bring-up slice.

## Implementation Boundaries

### `WiiUGx2Presenter`

This type becomes the owner of the pure GX2 presentation and diagnostic draw path.

In the first slice it should:

1. own the clear-only render step
2. own the final present/swap step

In the second slice it should additionally:

1. own the minimal square draw setup
2. allocate only the smallest extra GX2 resources needed for that square

### `WiiUApplication`

This type should stop treating `WiiUSoftwareSurface` output as the visible success condition for bring-up.

Its responsibility becomes:

1. run update/draw lifecycle as before
2. ask the presenter to render the current GX2 diagnostic frame
3. keep runtime tracing around phase boundaries and failures

## Verification Strategy

Verification should stay as small as possible.

For the `clear-only` slice:

1. add a focused source test that the runtime uses a pure GX2 clear path instead of software-surface upload in the visible frame path
2. rebuild the Wii U package
3. launch in Cemu
4. visually confirm the configured background color appears

For the `square` slice:

1. add a focused source test that the presenter binds the minimal GX2 state required for the square draw
2. rebuild the Wii U package
3. launch in Cemu
4. visually confirm the square appears over the clear color

## Risks

The main risk is trying to preserve too much of the old hybrid path while introducing the new GX2 path. That would blur ownership again and make failures ambiguous.

To avoid that, the pure GX2 bring-up path should be explicit and temporary. One path should own visible output during this slice.

## Success Criteria

This pivot is successful when:

1. the packaged Wii U runtime shows a stable pure GX2 background color in Cemu
2. the next slice shows a stable hard-coded square drawn through GX2
3. no visible output during these slices depends on copying pixels from `WiiUSoftwareSurface`
