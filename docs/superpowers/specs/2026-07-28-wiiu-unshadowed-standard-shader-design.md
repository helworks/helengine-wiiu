# Wii U Unshadowed StandardShader Control Path

## Objective

Prove that Wii U can render the shared generated `ForwardStandardShader` correctly without involving directional-shadow rendering. The control path must exercise the production shader, runtime material data, runtime lighting data, and generated Cafe shader artifacts while excluding the shadow depth pass and directional-shadow texture.

## Current State

Non-shadowed Wii U frames currently use the hand-written `SceneOpaqueShaderGroup` through `Render3DDrawCommandToColorBuffer`. Consequently, `cube_test` does not validate the generated StandardShader pipeline. Frames with directional shadows use `ForwardStandardShadowedShaderGroup`, so their output mixes base StandardShader behavior with shadow-pass behavior.

The current `ForwardStandard` and `ForwardStandardShadowed` cooked pixel sources are identical because `HELENGINE_STANDARD_SHADOWED` is not consumed by the shared HLSL. This design isolates the runtime path first; compile-time removal of shadow resources is outside this control-path change.

## Rendering Route

When `WiiUGx23DRenderFrame.GetHasDirectionalShadow()` is false, the presenter will render every opaque draw command with `ForwardStandardShaderGroup`. The route will not execute `RenderDirectionalShadowDepthPass` and will not bind `DirectionalShadowTexture`.

The existing shadowed path remains unchanged until the unshadowed control scenes pass on Cemu.

## StandardShader Data

The unshadowed route will upload the reflected StandardShader uniform blocks using the bindings emitted by the corrected CafeGLSL compiler:

- `TransformBuffer`
- `ForwardLightBuffer`
- `ShadowBuffer`
- `BaseColorBuffer`
- `RoughnessBuffer`
- `MetallicBuffer`
- `SpecularBuffer`
- `EmissiveBuffer`

`ShadowBuffer` will contain valid disabled metadata so the shader cannot enter directional-shadow sampling. Matrix payloads will use the transposed layout already proven by the Wii U vertex path. Required reflection or payload-size mismatches will remain hard failures.

The route will bind runtime diffuse, roughness, and emissive textures to their reflected generated sampler slots. Missing optional material textures will use the presenter's established solid-white fallback. No directional-shadow texture will be bound by this route.

Material scalar and color payloads will come from `WiiURuntimeMaterial`; diagnostic hard-coded material values are not valid for this production control path.

## Diagnostics

The runtime trace will identify the first use of the unshadowed generated StandardShader route. It will retain enough reflected binding information to distinguish this route from `SceneOpaqueShaderGroup` and the directional-shadow route without changing fragment output.

## Automated Validation

Source-level regression tests will verify that:

- frames without directional shadows dispatch to the generated unshadowed StandardShader route;
- the unshadowed route selects `ForwardStandardShaderGroup`;
- the unshadowed route disables shadow metadata;
- the unshadowed route does not bind `DirectionalShadowTexture`;
- the route uploads real runtime material parameters and reflected StandardShader blocks.

Existing shader compiler tests will continue to verify explicit uniform-block bindings and sampler bindings.

## Hardware Validation

After a successful authoritative Wii U package build and DemoDisc shader-cache clear, Helena will manually navigate and validate scenes in this order:

1. `cube_test` proves transform, directional lighting, and the authored solid material.
2. `Colored Cubes` proves distinct runtime base colors across multiple draw commands.
3. `Textured Cubes` proves diffuse texture sampling and sampler selection.

Directional shadows will not be debugged further until these three non-shadowed controls render correctly.

## Scope Boundaries

This change does not modify DemoDisc scenes, shared StandardShader behavior on other platforms, directional-shadow projection, shadow-depth texture handling, or compile-time shader variant specialization. The old hand-written opaque shader may remain available for diagnostics, but production non-shadowed scene draws will no longer use it.
