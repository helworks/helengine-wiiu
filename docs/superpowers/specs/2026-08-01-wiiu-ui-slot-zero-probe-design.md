# Wii U UI Slot-Zero Hardware Probe Design

## Purpose

Determine whether the real Wii U can execute the existing textured UI shader path when all vertex attributes begin at vertex zero. The current captured frame contains 465 valid UI commands and 433 on-screen commands, but the TV and GamePad show only a uniform purple result. The probe must separate a general UI shader, texture, or fetch failure from a failure selecting later quads in the shared vertex buffers.

## Diagnostic Behavior

Add a temporary presentation mode that bypasses captured engine-frame drawing after initialization and renders one opaque, full-screen cyan quad to both the TV and GamePad. The quad will use the existing UI quad shader, the presenter-owned solid-white texture and sampler, the existing UI attribute buffers, a full-target scissor, and `GX2DrawEx` with vertex offset zero.

The mode will retain the verified foreground lifecycle, render-target setup, scan-buffer copies, and Home Menu behavior. It will not change scene capture, texture loading, asset generation, or production renderer semantics outside the explicitly selected diagnostic branch.

## Interpretation

- A cyan image on both displays proves that the UI shader, texture sampling, fetch shader, vertex formats, buffer uploads, blending state, render targets, and scan-buffer copies work for vertex zero. Investigation then moves to nonzero vertex selection or per-command state.
- A purple or clear-only image proves that the failure occurs before batching becomes relevant. Investigation then moves to the UI shader metadata, sampler binding, texture visibility, attribute fetch, or draw state.
- Different results between TV and GamePad indicate a target-context or presentation difference rather than a common UI pipeline defect.

## Verification

Add a focused source-level test that initially fails because the diagnostic presentation mode and slot-zero UI probe do not exist. The test will require the diagnostic branch to call a dedicated presenter method and require that method to bind the UI resources and issue `GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, UiQuadVertexCount, 0, 1)`.

After the test passes, run only the focused Wii U runtime source tests and the smallest Wii U build needed to produce a deployable WUHB. The final result is verified on real hardware by observing the TV and GamePad colors; no screenshot is required.

## Removal

This is a temporary diagnostic mode. Once the hardware result is recorded, the probe branch will be replaced by the next evidence-driven renderer change rather than retained as a runtime fallback.
