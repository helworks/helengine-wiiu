# Wii U Directional StandardShader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile the shared StandardShader into a Wii U directional-shadow variant that contains no point-shadow cube texture path, allowing Cemu to create the generated pixel pipeline.

**Architecture:** Retain `ForwardStandardShader.hlsl` as the cross-platform source of truth. The Wii U cooker adds a directional-only preprocessor define for `ForwardStandardShadowed`; conditional HLSL excludes point-shadow texture declarations, `SamplePointShadowTexture`, and the point-shadow branch, while retaining the existing directional atlas path.

**Tech Stack:** C# asset cooker, HLSL, shaderc, SPIRV-Cross, CafeGLSL/GX2, xUnit source and backend tests.

---

### Task 1: Specify the directional-only compiled resource contract

**Files:**

- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiUShaderBackendTests.cs`
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing compiler test**

```csharp
Assert.DoesNotContain("uniform samplerCube", generatedShaderSource, StringComparison.Ordinal);
Assert.DoesNotContain("pointShadowTexture", generatedShaderSource, StringComparison.Ordinal);
Assert.Contains("uniform sampler2D shadowAtlasTexture", generatedShaderSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused test and verify it fails because the current generated directional shader still declares cube samplers**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter FullyQualifiedName~WiiUShaderBackendTests -p:BaseOutputPath=C:\dev\helworks\helengine-wiiu\builder.tests\bin_diagnostic\`

Expected: the new assertion fails while the generated shader includes `samplerCube` declarations.

### Task 2: Add a Wii U directional-only compiler define

**Files:**

- Modify: `C:\dev\helworks\helengine-wiiu\builder\WiiUShaderArtifactCooker.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiUShaderArtifactCookerTests.cs`

- [ ] **Step 1: Write the failing cooker test**

```csharp
Assert.Contains(result.Defines, define => define.Name == "HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY" && define.Value == "1");
```

- [ ] **Step 2: Run the focused test and verify it fails because `ForwardStandardShadowed` only supplies `HELENGINE_STANDARD_SHADOWED`**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter FullyQualifiedName~WiiUShaderArtifactCookerTests -p:BaseOutputPath=C:\dev\helworks\helengine-wiiu\builder.tests\bin_diagnostic\`

Expected: the new define assertion fails.

- [ ] **Step 3: Implement the minimal cooker change**

```csharp
return [
    new ShaderDefine("HELENGINE_STANDARD_SHADOWED", "1"),
    new ShaderDefine("HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY", "1")
];
```

- [ ] **Step 4: Re-run the focused cooker test**

Expected: PASS.

### Task 3: Gate point-shadow-only HLSL in the shared source

**Files:**

- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\shaders\builtin\ForwardStandardShader.hlsl`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiUShaderBackendTests.cs`

- [ ] **Step 1: Wrap point-shadow cube declarations and sampling in the Wii U directional-only exclusion**

```hlsl
#if !defined(HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY)
TextureCube pointShadowTexture0 : register(t2);
TextureCube pointShadowTexture1 : register(t3);
TextureCube pointShadowTexture2 : register(t4);
TextureCube pointShadowTexture3 : register(t5);
SamplerState pointShadowSampler : register(s2);
#endif
```

- [ ] **Step 2: Wrap `SamplePointShadowTexture` and the point-shadow receiver branch with the same condition**

```hlsl
#if !defined(HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY)
else if (shadowSlotMetadata.x > 0.5f && shadowSlotMetadata.z > 1.5f && lightType == 1)
{
    // Existing point-shadow sampling implementation.
}
#endif
```

- [ ] **Step 3: Run compiler and source tests**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --no-restore --filter FullyQualifiedName~WiiUShader -p:BaseOutputPath=C:\dev\helworks\helengine-wiiu\builder.tests\bin_diagnostic\`

Expected: PASS and emitted `ForwardStandardShadowed.ps` contains the atlas sampler but no cube sampler.

### Task 4: Remove the failed cube-resource diagnostic and verify in Cemu

**Files:**

- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Remove `StandardShaderFallbackCubeTextureHandle`, its allocation, and cube-sampler binding loop**

```cpp
GX2SetPixelTexture(&DirectionalShadowTexture, shadowAtlasSamplerVar->location);
GX2SetPixelSampler(&DirectionalShadowSampler, shadowAtlasSamplerVar->location);
```

- [ ] **Step 2: Restore runtime emissive material data and directional shadow enablement**

```cpp
const float emissiveData[] = {
    runtimeMaterial.GetEmissiveColor().X,
    runtimeMaterial.GetEmissiveColor().Y,
    runtimeMaterial.GetEmissiveColor().Z,
    runtimeMaterial.GetEmissiveColor().W
};
```

- [ ] **Step 3: Run presenter source tests, build Demodisc for Wii U, and launch with `scripts\launch_in_emulator.ps1`**

Expected: Cemu no longer reports a GLSL parsing failure, and the directional-shadow scene renders using the generated StandardShader.
