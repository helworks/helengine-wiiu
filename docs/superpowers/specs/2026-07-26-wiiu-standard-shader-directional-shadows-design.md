# Wii U StandardShader Directional Shadows Design

## Goal

Make the Directional Shadow scene in `C:\dev\helprojs\demodisc` render on Wii U through the shared custom-language `ForwardStandardShader`, including the `ShadowDepth` and `ForwardStandardShadowed` variants.

## Scope

This slice supports exactly the StandardShader inputs exercised by the Directional Shadow scene. It does not introduce a separate hand-authored Wii U shader implementation or attempt broad material parity beyond that scene.

## Authoritative Shader Source

The shared custom-language StandardShader remains the only shader source of truth. The Wii U target must join the same compilation model used by the editor, DirectX 11, Vulkan, and PS Vita:

1. The editor shader pipeline compiles `ForwardStandardShader` for the Wii U target.
2. The shared variant definition produces `ForwardStandard`, `ForwardStandardShadowed`, and `ShadowDepth` with their existing canonical entry points and defines.
3. The Wii U target emits GLSL source for each variant into the platform build staging area.
4. The native Wii U Makefile invokes CafeGLSL on those generated sources to create GX2 binaries.

The existing `tools/wiiu-shaders/*.vs` and `*.ps` files are transitional artifacts and must not remain an alternate implementation of StandardShader after this work. Wii U-only diagnostic and UI shaders may continue to use native GLSL where they are not StandardShader variants.

## Build and Packaging Contract

The Wii U platform build must package a resolvable StandardShader artifact and stage each generated GLSL pair under deterministic canonical names. The native build must depend on those staged files, compile all three GX2 variants, and fail the build if a required source or GX2 binary is missing. A source-contract test must demonstrate that changing the selected Wii U compiler target alters the generated Wii U shader staging inputs rather than selecting hand-written shader files.

## Native Rendering Contract

The Wii U renderer must use the same three canonical variants that the shared shader pipeline emits:

- `ShadowDepth` draws all directional-shadow casters into a Wii U depth surface using the light view-projection transform.
- `ForwardStandardShadowed` draws receivers, samples that shadow surface, and receives directional-light data and shadow transforms through the shared StandardShader bindings.
- `ForwardStandard` remains the fallback only for draws that the captured scene explicitly marks as not receiving directional shadows.

The renderer owns GX2 surface allocation, transitions, binding, and presentation. It must not reimplement the StandardShader lighting or shadow algorithm in C++. Missing variants, missing required parameter bindings, or unsupported Directional Shadow material inputs are fatal build or runtime errors, not silent fallback cases.

## Data Flow

```text
custom StandardShader
  -> editor Wii U compile target
  -> staged canonical GLSL variant pairs
  -> CafeGLSL GX2 binaries
  -> packaged Wii U shader bundle
  -> ShadowDepth pass
  -> ForwardStandardShadowed receiver pass
  -> Cemu Directional Shadow scene
```

## Validation

Automated coverage must prove that:

- the Wii U target is accepted by the shared shader compiler and emits all three canonical StandardShader variants;
- Wii U packaging stages generated variant sources and requires their GX2 outputs;
- the Wii U renderer requests `ShadowDepth` for caster draws and `ForwardStandardShadowed` for shadow-receiving draws;
- missing required shader variants or bindings fail explicitly.

The runtime acceptance test is a fresh build of `C:\dev\helprojs\demodisc\project.heproj` for `wiiu`, followed by launching the generated WUHB in Cemu and selecting the existing Directional Shadow scene. Success means the scene remains alive and shows moving directional shadows from its existing light/caster setup.

## Non-Goals

- New scene content or a new shader-validation scene.
- Hand-maintained StandardShader-equivalent Wii U GLSL.
- Point-light shadows, cascaded shadows, or support for StandardShader features unused by Directional Shadow.
