# Wii U 3D Material Texture Wrapping Design

## Problem

Tilt Play course meshes intentionally use UV coordinates outside the zero-to-one range so the cube texture tiles across scaled geometry. The shared DirectX 11 and Vulkan 3D renderers configure material samplers to wrap, but `WiiURenderManager3D::InitializeTextureHandle` configures Wii U material textures with `GX2_TEX_CLAMP_MODE_CLAMP`. Wii U therefore stretches edge texels instead of repeating the texture.

## Design

Configure the sampler created by `WiiURenderManager3D::InitializeTextureHandle` with `GX2_TEX_CLAMP_MODE_WRAP`. This applies shared 3D material semantics to both U and V because `GX2InitSampler` initializes all texture axes from the supplied clamp mode.

Do not modify authored or generated UVs, texture assets, shaders, materials, 2D UI samplers, the solid-white fallback sampler, or the directional-shadow sampler. Those independently configured samplers must remain clamped.

## Regression Contract

Add a focused Wii U runtime source test that isolates `WiiURenderManager3D::InitializeTextureHandle` and requires its `GX2InitSampler` call to use `GX2_TEX_CLAMP_MODE_WRAP`. The test must also retain the existing 2D and shadow clamping behavior outside this method.

## Verification

Honor the request not to build or launch CEMU. Verify the source contract, exact diff scope, and whitespace only. Runtime confirmation remains a later manual step using Tilt Play level 1.
