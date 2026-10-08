# Wii U 3D Material Texture Wrapping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Wii U 3D material textures repeat for authored UVs outside zero-to-one while retaining clamping for UI and shadow textures.

**Architecture:** Change the address mode at the Wii U 3D runtime-material texture sampler creation point, matching the shared DirectX 11 and Vulkan 3D backends. Leave independently owned 2D, fallback, and directional-shadow samplers unchanged.

**Tech Stack:** C++17, WUT GX2 sampler API, xUnit source-contract tests, PowerShell source verification.

---

### Task 1: Lock the Wii U sampler-addressing contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs:1199`
- Modify: `src/platform/wiiu/WiiURenderManager3D.cpp:991`

- [ ] **Step 1: Write the failing 3D material sampler test**

Add this focused test beside the existing cooked opaque-material texture test:

```csharp
/// <summary>
/// Ensures Wii U 3D material textures repeat authored UVs outside zero-to-one instead of clamping edge texels.
/// </summary>
[Fact]
public void RuntimeSeam_WrapsCooked3dMaterialTextureUvs() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
    int initializeStart = renderManagerSource.IndexOf("void WiiURenderManager3D::InitializeTextureHandle(", StringComparison.Ordinal);
    int destroyStart = renderManagerSource.IndexOf("void WiiURenderManager3D::DestroyTextureHandle(", initializeStart, StringComparison.Ordinal);

    Assert.True(initializeStart >= 0, "Expected the Wii U 3D texture initializer.");
    Assert.True(destroyStart > initializeStart, "Expected the Wii U 3D texture destroy helper after initialization.");

    string initializeSource = renderManagerSource.Substring(initializeStart, destroyStart - initializeStart);
    Assert.Contains(
        "GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);",
        initializeSource,
        StringComparison.Ordinal);
    Assert.DoesNotContain("GX2_TEX_CLAMP_MODE_CLAMP", initializeSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Record the unchanged clamp boundaries**

Add a separate source-contract test so the material fix cannot accidentally alter UI or shadow sampling:

```csharp
/// <summary>
/// Ensures Wii U UI and directional-shadow samplers retain edge clamping independently of 3D material textures.
/// </summary>
[Fact]
public void RuntimeSeam_KeepsUiAndShadowTextureSamplersClamped() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string render2dSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

    Assert.Contains(
        "GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);",
        render2dSource,
        StringComparison.Ordinal);
    Assert.Contains(
        "GX2InitSampler(&DirectionalShadowSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);",
        presenterSource,
        StringComparison.Ordinal);
}
```

- [ ] **Step 3: Verify the new 3D contract is red without building**

Run:

```powershell
$source = Get-Content -LiteralPath 'src/platform/wiiu/WiiURenderManager3D.cpp' -Raw
$expected = 'GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);'
if (-not $source.Contains($expected)) { Write-Error 'RED: Wii U 3D material sampler still clamps UVs.'; exit 1 }
```

Expected: exit code 1 with `RED: Wii U 3D material sampler still clamps UVs.`

- [ ] **Step 4: Implement the minimal sampler fix**

In `WiiURenderManager3D::InitializeTextureHandle`, replace only the sampler initialization line:

```cpp
GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);
```

Do not change `WiiURenderManager2D.cpp` or the sampler calls in `WiiUGx2Presenter.cpp`.

- [ ] **Step 5: Verify the source contracts are green without building**

Run:

```powershell
$render3d = Get-Content -LiteralPath 'src/platform/wiiu/WiiURenderManager3D.cpp' -Raw
$render2d = Get-Content -LiteralPath 'src/platform/wiiu/WiiURenderManager2D.cpp' -Raw
$presenter = Get-Content -LiteralPath 'src/platform/wiiu/WiiUGx2Presenter.cpp' -Raw
$wrap = 'GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);'
$uiClamp = 'GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);'
$shadowClamp = 'GX2InitSampler(&DirectionalShadowSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);'
if (-not $render3d.Contains($wrap)) { throw 'Wii U 3D material sampler does not wrap.' }
if (-not $render2d.Contains($uiClamp)) { throw 'Wii U 2D sampler no longer clamps.' }
if (-not $presenter.Contains($shadowClamp)) { throw 'Wii U shadow sampler no longer clamps.' }
Write-Output 'PASS: 3D wraps; UI and shadow samplers remain clamped.'
```

Expected: exit code 0 with `PASS: 3D wraps; UI and shadow samplers remain clamped.`

- [ ] **Step 6: Check the exact diff without compiling**

Run:

```powershell
rtk git diff --check -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiURenderManager3D.cpp
rtk git diff -- src/platform/wiiu/WiiURenderManager3D.cpp
```

Expected: `git diff --check` exits 0; the production diff contains exactly the clamp-to-wrap token change. Do not run `dotnet`, `make`, the Wii U packaging pipeline, or CEMU.

- [ ] **Step 7: Leave implementation uncommitted for isolated review**

Do not stage or commit the implementation because `builder.tests/WiiURuntimeSourceTests.cs` already contains the prior uncommitted Wii U buffer regression changes. Report the exact modified files and the source-only verification result.
