# Wii U UI No-Blend Hardware Probe Design

## Purpose

Determine whether the slot-zero textured UI probe produces fragments with zero source alpha or produces no fragments at all. The verified real-hardware build presented the exact lilac diagnostic clear instead of the probe's cyan output, while using source-alpha blending.

## Diagnostic Change

Retain the existing full-screen cyan vertices, vertex offset zero, UI fetch shader, vertex shader, pixel shader, presenter-owned white texture, sampler, buffers, target contexts, and scan-buffer presentation. Change only the slot-zero probe's `GX2SetColorControl` target blend-enable mask from `0x1` to `0x0`.

Captured-frame UI rendering will retain its existing `0x1` blend mask. No engine capture, texture loading, generated code, asset, lifecycle, or production presentation behavior will change.

## Interpretation

- Cyan means the UI shader produced cyan RGB but zero alpha; the failure is texture alpha or sampled alpha handling.
- Black means the UI draw reached the color buffer but sampled zero RGB and alpha; the failure is texture upload, visibility, or binding.
- Unchanged lilac means the draw produced no color-buffer fragments; investigation moves earlier to attribute fetch, vertex processing, rasterization, shader mode, or shader compatibility.
- Different TV and GamePad results indicate target-context state divergence.

## Verification

Update the focused source-contract test first so it fails while the probe still enables blending. Require the slot-zero helper, isolated from captured rendering, to contain `GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x0, FALSE, TRUE);`. After implementation, rerun the focused Wii U renderer tests and build a fresh Demo Disc WUHB. Record its timestamp and SHA-256 hash before the real-hardware observation.

## Follow-Up

The result will select one next diagnostic. Texture-focused investigation follows cyan or black; an untextured square probe follows unchanged lilac. The no-blend state remains diagnostic-only and will not become a fallback renderer behavior.
