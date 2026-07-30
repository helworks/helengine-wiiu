# Wii U ProcUI Foreground Lifecycle Design

## Objective

Make the custom GX2 presenter correctly release foreground-owned graphics resources when the Wii U HOME Menu takes control, recreate them when the application resumes, and permit a normal transition to the Wii U Menu without holding the console power button.

The clear-only hardware diagnostic displayed the presenter's pastel-lilac clear on both TV and GamePad, proving that GX2 initialization, both display targets, scan-buffer copies, swapping, flushing, and `GX2DrawDone()` work. HOME-to-Wii-U-Menu still hung, proving that captured 3D and UI draw submission is not required to reproduce the lifecycle failure.

## Root Cause

`WHBProcIsRunning()` processes ProcUI messages and acknowledges `PROCUI_STATUS_RELEASE_FOREGROUND` with `ProcUIDrawDoneRelease()`. The custom presenter does not register ProcUI acquire or release callbacks, so that acknowledgement occurs while its foreground scan buffers and MEM1 render surfaces remain allocated.

The official libwhb GX2 implementation registers `PROCUI_CALLBACK_ACQUIRE` and `PROCUI_CALLBACK_RELEASE`. Its release callback waits for GPU completion, frees foreground scan buffers and MEM1 color/depth surfaces, and destroys the corresponding heaps before ProcUI completes the ownership transfer. Its acquire callback recreates and rebinds those resources.

## Architecture

`WiiUGx2Presenter` will own the ProcUI callback registration and expose no lifecycle responsibility to engine render managers or generated code. Static callback adapters will receive the presenter through the ProcUI context pointer and call instance-owned acquire and release methods without allowing C++ exceptions to cross the C callback boundary.

Graphics resources will be divided by lifetime:

- Persistent across foreground transitions: GX2 initialization, command-buffer pool, TV and DRC context-state allocations, GX2R allocator registration, shader programs, textures, and vertex/uniform buffers allocated from MEM2.
- Foreground-dependent: MEM1 and foreground heaps, TV and DRC scan buffers, TV and DRC color/depth surfaces, and the directional-shadow depth surface because its depth-buffer binding routes it through the MEM1 allocator.

The presenter will track whether it currently owns valid foreground resources. Rendering requires that state in addition to normal initialization.

## Foreground Release

The ProcUI release callback will:

1. Complete submitted GPU work with `GX2DrawDone()`.
2. Destroy the directional-shadow depth surface while retaining the persistent directional-shadow shader state needed to recreate it.
3. Destroy TV and DRC color/depth surfaces.
4. Free and clear TV and DRC foreground scan-buffer pointers.
5. Destroy the MEM1 and foreground heaps.
6. Mark foreground resources unavailable before returning success to ProcUI.

The operation will be idempotent so final shutdown can safely call it when the presenter still owns foreground resources.

## Foreground Acquire

The ProcUI acquire callback will:

1. Initialize the MEM1 and foreground heaps.
2. Allocate, invalidate, and bind both scan buffers.
3. Recreate TV and DRC color/depth surfaces.
4. Recreate the directional-shadow depth surface and its texture view.
5. Reconfigure TV and DRC context states, render targets, viewports, scissors, display scales, and swap interval.
6. Mark foreground resources available only after every required resource succeeds.

If acquisition fails, the callback will release any partially acquired foreground resources and return failure without throwing through ProcUI.

## Initialization and Final Shutdown

Initial presenter setup will initialize GX2 and persistent resources, acquire the initial foreground resource set through the same instance method used by the ProcUI callback, and then register the acquire/release callbacks with the presenter as context.

Final shutdown will release foreground resources if held, remove GX2R allocator ownership, shut down GX2, free persistent context and command-buffer allocations, and clear presenter state. The presenter object remains alive for the entire `WHBProcIsRunning()` loop, so callback context remains valid until ProcUI processing has ended.

## Validation

Validation will remain two-stage so foreground lifecycle and captured rendering are not changed in the same hardware experiment:

1. Keep `DiagnosticPresentationMode::ClearOnly`. Confirm pastel-lilac output on both displays, press HOME and resume the software, then press HOME and return to the Wii U Menu. Both resume and exit must complete normally.
2. Change only the presentation constant back to `DiagnosticPresentationMode::CapturedFrame`. Rebuild and record TV output, GamePad output, HOME resume, and HOME exit. Any remaining dark-purple/black rendering problem will then be investigated separately from foreground ownership.

Focused source-contract tests will verify callback registration, acquire/release ordering, foreground-resource state checks, and the two presentation modes. Each authoritative DemoDisc WUHB must be fresh and non-empty before hardware testing. Real-console behavior is the success criterion for lifecycle correctness.

## Scope

This repair will not replace the custom presenter with `WHBGfx`, rewrite generated code, change shader binaries, alter engine frame capture, or combine a new rendering hypothesis with the lifecycle change. Existing diagnostic changes remain isolated until the two-stage hardware result determines which ones should be retained.
