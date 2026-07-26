# Wii U StandardShader Directional Shadows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile the shared StandardShader for Wii U and render demodisc's Directional Shadow scene with canonical `ShadowDepth` and `ForwardStandardShadowed` variants.

**Architecture:** The shared editor compiler emits the standard variants for a new Wii U GLSL target. The Wii U builder stages those generated source pairs; the native Makefile compiles them into GX2 binaries. The GX2 presenter creates a directional-shadow depth target, draws casters with `ShadowDepth`, then uses `ForwardStandardShadowed` for receiving draws.

**Tech Stack:** C#/.NET shader compilation and platform builders, HLSL-derived custom StandardShader source, generated GLSL, CafeGLSL, WUT/GX2, C++, Cemu.

---

### Task 1: Add the shared Wii U shader compile target

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.shader\shaders\compilation\ShaderCompileTarget.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.shader\shaders\compilation\ShaderTargetNames.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.shader\shaders\compilation\ShaderPlatformDefines.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.shader\shaders\compilation\WiiUGlslShaderBackend.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\shaders\WiiUStandardShaderCompilationTests.cs`

- [ ] **Step 1: Write a failing compiler contract test.**

```csharp
[Fact]
public void Compile_WhenTargetIsWiiU_EmitsCanonicalGlslForAllStandardVariants() {
    ShaderAsset shader = EditorBuiltInShaderAssetLibrary.LoadShaderAsset(
        ShaderCompileTarget.WiiU,
        "ForwardStandardShader.hlsl");

    Assert.Equal(new[] { "ForwardStandard", "ForwardStandardShadowed", "ShadowDepth" },
        shader.Definition.Binaries.Where(binary => binary.Stage == ShaderStage.Vertex)
            .Select(binary => binary.Variant).ToArray());
    Assert.All(shader.Definition.Binaries, binary => Assert.NotEmpty(binary.Bytecode));
}
```

- [ ] **Step 2: Run the new test and verify it fails because `WiiU` is not a target.**

Run: `rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~WiiUStandardShaderCompilationTests --no-restore`

Expected: compile failure naming the missing `ShaderCompileTarget.WiiU` member.

- [ ] **Step 3: Implement the target and GLSL backend.**

```csharp
// ShaderCompileTarget.cs
WiiU

// ShaderTargetNames.cs
case ShaderCompileTarget.WiiU:
    return "wiiu";

// ShaderPlatformDefines.cs
case ShaderCompileTarget.WiiU:
    return "HEL_API_WIIU";
```

Implement `WiiUGlslShaderBackend : IShaderBackend` beside the shared compile types. It must reject non-Wii-U requests, accept vertex and pixel stages, preserve `ShaderCompileRequest.EntryPoint`, `Variant`, defines, and the default binding policy, and produce CafeGLSL-compatible GLSL source bytes. Its `ShaderProgramDefinition` must retain the parsed shared binding metadata so packaging and the native renderer use the same names as other targets.

- [ ] **Step 4: Run the compiler test and the existing standard-variant tests.**

Run: `rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~WiiUStandardShaderCompilationTests|FullyQualifiedName~StandardShaderVariantsTests" --no-restore`

Expected: PASS.

- [ ] **Step 5: Commit the shared target.**

```powershell
rtk git add -- engine/helengine.shader/shaders/compilation engine/helengine.editor.tests/shaders/WiiUStandardShaderCompilationTests.cs
rtk git commit -m "feat: compile StandardShader GLSL for Wii U"
```

### Task 2: Select and package the Wii U target in editor builds

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\EditorCliBuildRunner.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\shaders\EditorBuiltInShaderAssetLibrary.cs`
- Modify: `builder\WiiUPlatformAssetBuilder.cs`
- Create: `builder\WiiUShaderSourceStager.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\EditorCliBuildRunnerTests.cs`
- Test: `builder.tests\WiiUShaderSourceStagerTests.cs`

- [ ] **Step 1: Write failing target-selection and staging tests.**

```csharp
[Fact]
public void ResolveShaderCompileTarget_WhenPlatformIsWiiU_ReturnsWiiU() {
    Assert.Equal(ShaderCompileTarget.WiiU, EditorCliBuildRunner.ResolveShaderCompileTarget("wiiu"));
}

[Fact]
public void Stage_WhenStandardShaderHasCanonicalVariants_WritesGeneratedGlslPairs() {
    // Build a Wii U ForwardStandardShader asset and assert ForwardStandard.vs/.ps,
    // ForwardStandardShadowed.vs/.ps, and ShadowDepth.vs/.ps exist in the native source staging directory.
}
```

- [ ] **Step 2: Run the focused tests and verify the expected failures.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiUShaderSourceStagerTests --no-restore`

Expected: the stager type and Wii U target selection do not yet exist.

- [ ] **Step 3: Implement explicit source staging.**

`ResolveShaderCompileTarget` must map only `wiiu` to `ShaderCompileTarget.WiiU`. `WiiUShaderSourceStager` must read the compiled source bytes from the packaged `ForwardStandardShader` asset, validate one vertex and one pixel program for each canonical variant, and write them to the native workspace's `tools/wiiu-shaders/generated` directory using the exact variant basenames. It must throw when any pair is absent, empty, or not UTF-8 GLSL.

- [ ] **Step 4: Run focused builder/editor tests.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiUShaderSourceStagerTests --no-restore`

Expected: PASS.

- [ ] **Step 5: Commit the staging seam.**

```powershell
rtk git add -- builder builder.tests
rtk git commit -m "feat: stage generated Wii U StandardShader sources"
```

### Task 3: Compile generated GLSL and bind canonical GX2 programs

**Files:**
- Modify: `Makefile`
- Remove: `tools/wiiu-shaders/ForwardStandard.vs`
- Remove: `tools/wiiu-shaders/ForwardStandard.ps`
- Remove: `tools/wiiu-shaders/ForwardStandardShadowed.vs`
- Remove: `tools/wiiu-shaders/ForwardStandardShadowed.ps`
- Remove: `tools/wiiu-shaders/ShadowDepth.vs`
- Remove: `tools/wiiu-shaders/ShadowDepth.ps`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests\WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write failing native-build and source-contract tests.**

```csharp
[Fact]
public void NativeMakefile_CompilesStandardVariantsFromGeneratedSources() {
    string makefile = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
    Assert.Contains("tools/wiiu-shaders/generated", makefile, StringComparison.Ordinal);
    Assert.DoesNotContain("tools/wiiu-shaders/ForwardStandard.vs", makefile, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the source-contract test and verify it fails.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore`

Expected: generated source staging is absent from the Makefile/presenter contract.

- [ ] **Step 3: Make GX2 variant loading explicit.**

Change the Makefile pattern rule so StandardShader variant `.vs` and `.ps` inputs come only from `tools/wiiu-shaders/generated`, while diagnostic and UI shaders retain their native source directory. In `WiiUGx2Presenter`, load separate `WHBGfxShaderGroup` instances for `ForwardStandard`, `ForwardStandardShadowed`, and `ShadowDepth`; initialization must fail if any generated binary is unavailable. Select the forward program from the render frame's shadow-receiver flag rather than substituting a material fallback.

- [ ] **Step 4: Run source-contract tests and a native clean build.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore`

Expected: PASS.

Run: `rtk proxy docker run --rm -v /mnt/c/dev/helworks/helengine-wiiu:/workspace -w /workspace helengine-wiiu sh -lc "make clean && make"`

Expected: all three generated shader blobs and `build/helengine_wiiu.wuhb` are emitted.

- [ ] **Step 5: Commit native shader compilation.**

```powershell
rtk git add -- Makefile tools/wiiu-shaders src/platform/wiiu builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "feat: load generated StandardShader variants on Wii U"
```

### Task 4: Render the Directional Shadow scene through the two-pass GX2 path

**Files:**
- Modify: `src/platform/wiiu/WiiUGx23DRenderFrame.hpp`
- Modify: `src/platform/wiiu/WiiURenderManager3D.hpp`
- Modify: `src/platform/wiiu/WiiURenderManager3D.cpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests\WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write failing frame and presenter contracts.**

```csharp
Assert.Contains("ShadowCaster", renderFrameHeaderSource, StringComparison.Ordinal);
Assert.Contains("LightViewProjection", renderFrameHeaderSource, StringComparison.Ordinal);
Assert.Contains("RenderShadowDepthPass", presenterHeaderSource, StringComparison.Ordinal);
Assert.Contains("ForwardStandardShadowed", presenterSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the contract test and verify it fails.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore`

Expected: the captured frame lacks caster/light-space state and the presenter lacks a shadow pass.

- [ ] **Step 3: Implement captured shadow state and rendering.**

Add a frame-owned directional-shadow record containing the selected directional light's view-projection transform, shadow-map dimensions, and caster draw commands extracted from `RenderFrameShadowCasterSubmission`. Allocate one GX2 depth surface for the shadow map in the presenter. For each target display: render caster geometry with `ShadowDepth`, bind the resulting depth texture/sampler plus light-space transform for receiver draws, and use `ForwardStandardShadowed` only for receiver commands. Preserve the captured `ForwardStandard` route for explicitly unshadowed draws. Treat missing shadow data or a missing canonical shader binding as a runtime error.

- [ ] **Step 4: Run source tests, then build and validate demodisc.**

Run: `rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore`

Expected: PASS.

Run: `rtk dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\wiiu-build --require helengine_wiiu.wuhb -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build`

Expected: the build completes with generated GLSL, GX2 binaries, and a non-empty WUHB.

Run: `rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb`

Expected: Cemu starts the packaged demodisc build; the existing Directional Shadow scene is alive and its moving directional shadows render.

- [ ] **Step 5: Commit the two-pass renderer.**

```powershell
rtk git add -- src/platform/wiiu builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "feat: render directional StandardShader shadows on Wii U"
```
