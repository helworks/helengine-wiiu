# Wii U Unshadowed StandardShader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route non-shadowed Wii U scene draws through the generated `ForwardStandardShaderGroup` and prove transforms, material colors, and textures independently of the directional-shadow pass.

**Architecture:** Consolidate generated StandardShader drawing behind one presenter method that receives the selected generated shader group plus an explicit shadow-enabled flag. Non-shadowed frames select `ForwardStandardShaderGroup`, upload disabled shadow metadata, and never bind the directional-shadow texture; shadowed frames preserve their existing group and depth-texture behavior.

**Tech Stack:** C++20 Wii U GX2/WHB runtime, C# xUnit source-contract tests, Helengine shader cooker, Docker-based devkitPro Wii U build, Cemu hardware-path validation.

---

## File Structure

- Modify `builder.tests/WiiURuntimeSourceTests.cs`: define the source contract for generated unshadowed StandardShader dispatch and resource isolation.
- Modify `src/platform/wiiu/WiiUGx2Presenter.hpp`: replace the shadow-only receiver method declaration with the shared generated StandardShader draw contract.
- Modify `src/platform/wiiu/WiiUGx2Presenter.cpp`: dispatch both frame modes through the generated StandardShader method, select the correct shader group, disable shadow metadata for control frames, and omit the directional-shadow texture binding.
- Validate `C:/dev/helprojs/demodisc/wiiu-build/content/cooked/shaders/ForwardStandard.ps`: confirm explicit uniform and sampler bindings in the staged control shader.

### Task 1: Lock the non-shadowed dispatch contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source-contract test**

Add this xUnit test to `WiiURuntimeSourceTests`:

```csharp
/// <summary>
/// Verifies that frames without directional shadows use the generated unshadowed StandardShader without binding the directional depth texture.
/// </summary>
[Fact]
public void RuntimeSeam_UsesGeneratedStandardShaderForNonShadowedFrames() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    int dispatchStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DFrameToColorBuffer", StringComparison.Ordinal);
    int quadRouteStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderQuadCommandsToColorBuffer", StringComparison.Ordinal);
    int standardDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer", StringComparison.Ordinal);
    int legacyDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer", StringComparison.Ordinal);

    Assert.True(dispatchStart >= 0 && quadRouteStart > dispatchStart, "Expected the 3D frame dispatcher before the 2D route.");
    Assert.True(standardDrawStart >= 0 && legacyDrawStart > standardDrawStart, "Expected the generated StandardShader draw before the legacy diagnostic draw.");
    string dispatchSource = presenterSource.Substring(dispatchStart, quadRouteStart - dispatchStart);
    string standardDrawSource = presenterSource.Substring(standardDrawStart, legacyDrawStart - standardDrawStart);
    Assert.Contains("&ForwardStandardShaderGroup", dispatchSource, StringComparison.Ordinal);
    Assert.Contains("&ForwardStandardShadowedShaderGroup", dispatchSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Render3DDrawCommandToColorBuffer(drawCommands[commandIndex]", dispatchSource, StringComparison.Ordinal);
    Assert.Contains("directionalShadowsEnabled ? 1.0f : 0.0f", standardDrawSource, StringComparison.Ordinal);
    Assert.Contains("if (directionalShadowsEnabled)", standardDrawSource, StringComparison.Ordinal);
    Assert.Contains("GX2SetPixelTexture(&DirectionalShadowTexture", standardDrawSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_UsesGeneratedStandardShaderForNonShadowedFrames"
```

Expected: FAIL because `RenderStandard3DDrawCommandToColorBuffer` and the generated unshadowed dispatch do not exist.

### Task 2: Route non-shadowed draws through the generated StandardShader

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Replace the shadow-only presenter declaration**

Replace `RenderShadowed3DDrawCommandToColorBuffer` in the header with:

```cpp
/// Renders one receiver command through the selected generated StandardShader variant.
void RenderStandard3DDrawCommandToColorBuffer(
    const WiiUGx23DDrawCommand& drawCommand,
    const WiiUGx23DRenderFrame& frame,
    const WiiUGx23DCameraState& cameraState,
    WHBGfxShaderGroup* shaderGroup,
    bool directionalShadowsEnabled,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight);
```

- [ ] **Step 2: Select the generated shader group in the frame dispatcher**

Replace the current shadowed/generic draw calls with:

```cpp
WHBGfxShaderGroup* standardShaderGroup = frame.GetHasDirectionalShadow()
    ? &ForwardStandardShadowedShaderGroup
    : &ForwardStandardShaderGroup;
RenderStandard3DDrawCommandToColorBuffer(
    drawCommands[commandIndex],
    frame,
    cameraState,
    standardShaderGroup,
    frame.GetHasDirectionalShadow(),
    targetWidth,
    targetHeight);
```

Retain one-time route tracing, but rename the non-shadowed trace to `unshadowed generated StandardShader route`.

- [ ] **Step 3: Generalize the generated StandardShader draw method**

Rename the existing shadowed draw definition and add the selected group and flag parameters:

```cpp
void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer(
    const WiiUGx23DDrawCommand& drawCommand,
    const WiiUGx23DRenderFrame& frame,
    const WiiUGx23DCameraState& cameraState,
    WHBGfxShaderGroup* shaderGroup,
    bool directionalShadowsEnabled,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight) {
    if (drawCommand.RuntimeModel == nullptr || drawCommand.RuntimeMaterial == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter requires one runtime model and material for generated StandardShader rendering.");
    } else if (shaderGroup == nullptr || shaderGroup->vertexShader == nullptr || shaderGroup->pixelShader == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter requires one initialized generated StandardShader group.");
    } else if (targetWidth == 0U || targetHeight == 0U) {
        return;
    }

    // Preserve the existing transform, material, uniform upload, mesh upload, and draw sequence below this validation.
}
```

Within that method, replace every hard-coded `ForwardStandardShadowedShaderGroup` reflection or binding access with `shaderGroup`.

Set the directional-light shadow-strength lane without reading absent frame shadow state:

```cpp
directionalDirection.X,
directionalDirection.Y,
directionalDirection.Z,
directionalShadowsEnabled ? frame.GetDirectionalShadow().Strength : 0.0f
```

- [ ] **Step 4: Build valid disabled shadow data without accessing absent shadow state**

Replace the shadow payload construction with a fixed reflected-size payload:

```cpp
float shadowData[100] = {};
shadowData[0] = directionalShadowsEnabled ? 1.0f : 0.0f;
shadowData[1] = static_cast<float>(DirectionalShadowMapSize);
shadowData[2] = static_cast<float>(DirectionalShadowMapSize);
if (directionalShadowsEnabled) {
    const WiiUGx23DDirectionalShadowState& directionalShadow = frame.GetDirectionalShadow();
    const float4x4& shadowMatrix = directionalShadow.LightViewProjection;
    shadowData[4] = 0.0f;
    shadowData[5] = 0.0f;
    shadowData[6] = 1.0f;
    shadowData[7] = 1.0f;
    shadowData[8] = 1.0f;
    shadowData[9] = directionalShadow.Strength;
    shadowData[10] = 1.0f;
    shadowData[12] = shadowMatrix.M11;
    shadowData[13] = shadowMatrix.M21;
    shadowData[14] = shadowMatrix.M31;
    shadowData[15] = shadowMatrix.M41;
    shadowData[16] = shadowMatrix.M12;
    shadowData[17] = shadowMatrix.M22;
    shadowData[18] = shadowMatrix.M32;
    shadowData[19] = shadowMatrix.M42;
    shadowData[20] = shadowMatrix.M13;
    shadowData[21] = shadowMatrix.M23;
    shadowData[22] = shadowMatrix.M33;
    shadowData[23] = shadowMatrix.M43;
    shadowData[24] = shadowMatrix.M14;
    shadowData[25] = shadowMatrix.M24;
    shadowData[26] = shadowMatrix.M34;
    shadowData[27] = shadowMatrix.M44;
}
```

Use `sizeof(shadowData)` for the reflected payload-size check.

- [ ] **Step 5: Bind only resources used by the control route**

Keep diffuse, roughness, and emissive bindings on reflected sampler entries 0, 1, and 2. Guard the directional depth texture and sampler together:

```cpp
if (directionalShadowsEnabled) {
    if (shaderGroup->pixelShader->samplerVarCount < 4U) {
        throw std::runtime_error("Wii U GX2 presenter requires one reflected directional-shadow sampler slot.");
    }

    const GX2SamplerVar* shadowAtlasSamplerVar = &shaderGroup->pixelShader->samplerVars[3];
    GX2SetPixelTexture(&DirectionalShadowTexture, shadowAtlasSamplerVar->location);
    GX2SetPixelSampler(&DirectionalShadowSampler, shadowAtlasSamplerVar->location);
}
```

Require at least three samplers before binding the material textures. Use `runtimeMaterial.GetBaseColor()`, `runtimeMaterial.GetEmissiveColor()`, and `runtimeMaterial.GetIsLit()` for the runtime material values. Preserve the StandardShader defaults of roughness `1.0`, metallic `0.0`, and specular `0.5` until those properties exist on `WiiURuntimeMaterial`; do not introduce diagnostic fragment colors.

- [ ] **Step 6: Run focused tests and verify GREEN**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_UsesGeneratedStandardShaderForNonShadowedFrames|FullyQualifiedName~RuntimeSeam_TransposesStandardShaderMatricesAndKeepsDirectionalShadowsEnabled|FullyQualifiedName~RuntimeSeam_BindsShadowedStandardShaderTexturesByGeneratedSamplerSlot|FullyQualifiedName~CafeGlslCompilerIntegrationTests|FullyQualifiedName~Compile_pixel_shader_names_standard_shader_texture_resources"
```

Expected: all selected tests PASS. Existing unrelated source-contract failures are not part of this focused result.

### Task 3: Build and validate the non-shadowed control scenes

**Files:**
- Validate: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`
- Validate: `C:/dev/helprojs/demodisc/wiiu-build/content/cooked/shaders/ForwardStandard.ps`

- [ ] **Step 1: Build the authoritative Wii U package**

Run:

```powershell
rtk dotnet run --project ..\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\wiiu-build --require helengine_wiiu.wuhb -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build
```

Expected: exit code 0 and fresh `helengine_wiiu.rpx` and `helengine_wiiu.wuhb` timestamps.

- [ ] **Step 2: Validate the staged generated shader**

Run:

```powershell
rtk rg -n "layout\(binding = (0|1|6|7)\) uniform sampler2D" C:\dev\helprojs\demodisc\wiiu-build\content\cooked\shaders\ForwardStandard.ps
```

Expected: diffuse binding 0, shadow atlas binding 1, roughness binding 6, and emissive binding 7.

- [ ] **Step 3: Clear only DemoDisc's Cemu shader cache and runtime trace**

Remove the exact title-id files `0005000f7dd6d7c4.bin`, `0005000f7dd6d7c4_spirv.bin`, `0005000f7dd6d7c4_shaders.bin`, and `0005000f7dd6d7c4_vkpipeline.bin`, plus `wiiu_runtime_trace.txt`. Do not recursively remove Cemu cache directories.

- [ ] **Step 4: Launch the fresh WUHB**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File scripts\launch_in_emulator.ps1 -ArtifactPath 'C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb'
```

Expected: Cemu remains open at the DemoDisc menu.

- [ ] **Step 5: Ask Helena to perform manual scene validation**

Ask Helena to navigate manually and report each scene before continuing:

1. `cube_test`: rotating cube has coherent face lighting and authored solid color.
2. `Colored Cubes`: distinct per-material base colors render simultaneously.
3. `Textured Cubes`: diffuse textures render with correct UVs and sampler selection.

Do not navigate Cemu automatically and do not take screenshots.

- [ ] **Step 6: Commit the validated production change**

After all three controls pass, stage only the implementation, tests, and plan:

```powershell
rtk git add -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUGx2Presenter.hpp docs/superpowers/plans/2026-07-28-wiiu-unshadowed-standard-shader.md
rtk git commit -m "feat: render Wii U scenes with generated StandardShader"
```

Do not stage build outputs, `.diagnostics`, temporary logs, or unrelated dirty-worktree files.
