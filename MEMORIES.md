# Wii U Rendering Memories

## Proven unshadowed StandardShader fix

- The Wii U shader pipeline is shared HLSL to SPIR-V to GLSL 330 to CafeGLSL/GX2. Generated shaders must never be patched after compilation; fix the C# shader backend instead.
- SPIRV-Cross originally emitted different names for separately compiled vertex outputs and pixel inputs. The vertex stage used names such as `_entryPointOutput_worldPos`, while the pixel stage expected names such as `input_worldPos`.
- GLSL 330 did not emit explicit locations for those varyings, so CafeGLSL could not reliably connect world position, normal, and texture-coordinate data. Lighting still reacted to the authored light color, but the cube looked like grayscale depth data because its interpolated surface inputs were invalid.
- `WiiUGlslShaderBackend` now assigns matching names by SPIR-V interface location before GLSL is emitted:
  - location 0: `WorldPosition`
  - location 1: `WorldNormal`
  - location 2: `TextureCoordinate`
- `Compile_standard_shader_stages_name_matching_varyings` is the regression test for this contract.
- Manual acceptance on July 28, 2026: DemoDisc `cube_test`, with shadows disabled, rendered exactly like Windows in Cemu.

## Validation workflow

- Rebuild the DemoDisc Wii U artifact after shader-backend changes.
- Before launching, close Cemu and clear only title `0005000f7dd6d7c4` from Cemu's driver, precompiled, transferable shader, and transferable Vulkan-pipeline caches.
- Launch with `scripts/launch_in_emulator.ps1` and the generated `helengine_wiiu.wuhb` artifact.
- Ask Helena to navigate to and visually check the scene. Do not automate scene navigation, type directly into her UI, or take screenshots without explicit permission. Use HelenUI's navigator service if UI typing is required.

## Current boundary

- Unshadowed `cube_test` is proven correct.
- Directional shadows are separate follow-up work and were intentionally excluded from this acceptance result.
