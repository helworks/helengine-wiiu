# Wii U Clear-Only Hardware Stall Diagnostic Design

## Objective

Localize the real-Wii-U stall that leaves the TV at the authored menu clear color, leaves the GamePad black, prevents HOME from returning to the Wii U Menu, and prevents normal shutdown. Two controlled hardware tests have already ruled out presentation-resource heap placement and per-quad attribute-buffer rebasing as the immediate cause.

## Diagnostic Boundary

The diagnostic build will retain the current application initialization, generated-engine initialization, update/draw loop, GX2 presenter initialization, TV and DRC color buffers, scan buffers, scan-buffer copies, and process lifecycle. Presentation will select the presenter's existing `RenderDiagnosticClearFrame()` path instead of submitting captured 3D and 2D draw commands.

A compile-time presentation-mode constant in `WiiUApplication.cpp` will make this selection explicit and reversible. The diagnostic will not alter generated code, shader binaries, texture data, renderer allocations, or the implementation of the clear-only presenter path.

## Interpretation

- If both displays show the diagnostic clear and HOME returns normally to the Wii U Menu, the shared GX2 initialization and presentation path is functional. The stall is within the bypassed captured 3D or 2D draw submission.
- If the TV remains purple, the GamePad remains black, and HOME still cannot return, active scene and UI draws are not required to reproduce the stall. Investigation must move earlier to GX2 initialization, context/color-buffer commands, scan-buffer copies, presentation, or teardown.
- Any different combination of TV output, GamePad output, and HOME behavior will identify the first display or lifecycle boundary that diverges and will be recorded before another test is designed.

## Validation

A focused source-contract test will first fail while captured-frame presentation remains selected, then pass after the diagnostic mode selects `RenderDiagnosticClearFrame()`. The authoritative DemoDisc WUHB build must complete with a fresh, non-empty artifact before hardware testing. Hardware results, rather than compilation alone, determine the diagnostic outcome.

## Scope

This is evidence-gathering instrumentation, not a claimed fix. No additional rendering changes will be combined with it. The diagnostic presentation mode will remain isolated so the normal captured-frame path can be restored after the hardware result is recorded.
