# Wii U GX2 Diagnostic Triangle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render one presenter-owned static-color triangle through GX2 on Wii U using offline-compiled shader binaries and a single diagnostic draw call.

**Architecture:** Keep the first 3D slice entirely inside `WiiUGx2Presenter`. Add one triangle shader blob, one presenter-owned vertex buffer, and one `RenderDiagnosticTriangleFrame()` path, then temporarily route visible rendered output through that diagnostic path so Cemu verification is unambiguous.

**Tech Stack:** C++20, `wut`, GX2/GX2R, WHB GFD shader loading, CafeGLSL CLI under WSL/Linux, xUnit source-contract tests, PowerShell build and Cemu launch scripts

---

### Task 1: Lock The Presenter-Owned Triangle Contract

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test near the other GX2 presenter seam tests:

```csharp
/// <summary>
/// Ensures the first Wii U 3D bring-up slice renders one presenter-owned diagnostic triangle through offline-compiled GX2 shaders.
/// </summary>
[Fact]
public void RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
    string shaderVertexPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_triangle.vs");
    string shaderPixelPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_triangle.ps");
    string shaderBinaryPath = Path.Combine(repositoryRootPath, "data", "diagnostic_triangle_shader.bin");

    Assert.True(File.Exists(shaderVertexPath), "Expected diagnostic_triangle.vs to exist.");
    Assert.True(File.Exists(shaderPixelPath), "Expected diagnostic_triangle.ps to exist.");
    Assert.True(File.Exists(shaderBinaryPath), "Expected diagnostic_triangle_shader.bin to exist.");
    Assert.Contains("void RenderDiagnosticTriangleFrame();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("#include \"diagnostic_triangle_shader_bin.h\"", presenterSource, StringComparison.Ordinal);
    Assert.Contains("WHBGfxLoadGFDShaderGroup(&DiagnosticTriangleShaderGroup, 0, diagnostic_triangle_shader_bin)", presenterSource, StringComparison.Ordinal);
    Assert.Contains("GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticTriangleVertexCount, 0, 1);", presenterSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderDiagnosticTriangleFrame();", applicationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->RenderFrame(EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL because the triangle shader source/blob files and presenter method do not exist yet.

- [ ] **Step 3: Write minimal implementation to satisfy the contract**

Add the new test method to `builder.tests/WiiURuntimeSourceTests.cs` exactly as written above.

- [ ] **Step 4: Run test again to confirm it still fails for the right reason**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL on missing `diagnostic_triangle.vs`, `diagnostic_triangle.ps`, or `diagnostic_triangle_shader.bin`.

- [ ] **Step 5: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "test: lock Wii U diagnostic triangle contract"
```

### Task 2: Add The Triangle Shader Sources And Compile The Shader Blob

**Files:**
- Create: `C:\dev\helworks\helengine-wiiu\tools\wiiu-shaders\diagnostic_triangle.vs`
- Create: `C:\dev\helworks\helengine-wiiu\tools\wiiu-shaders\diagnostic_triangle.ps`
- Create: `C:\dev\helworks\helengine-wiiu\data\diagnostic_triangle_shader.bin`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing asset expectations**

Create `tools/wiiu-shaders/diagnostic_triangle.vs`:

```glsl
#version 450

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aColor;

layout(location = 0) out vec4 VertexColor;

void main() {
    VertexColor = aColor;
    gl_Position = aPosition;
}
```

Create `tools/wiiu-shaders/diagnostic_triangle.ps`:

```glsl
#version 450
#extension GL_ARB_shading_language_420pack: enable

layout(location = 0) in vec4 VertexColor;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = VertexColor;
}
```

- [ ] **Step 2: Run the contract test to verify it still fails on the missing compiled blob**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL because `data/diagnostic_triangle_shader.bin` does not exist yet.

- [ ] **Step 3: Compile the triangle shader blob with the local CafeGLSL CLI**

Run the local Linux/WSL compiler from the repo root:

```powershell
rtk wsl bash -lc "cd /mnt/c/dev/helworks/helengine-wiiu && tools/cafeglsl/glslcompiler.elf -ps tools/wiiu-shaders/diagnostic_triangle.ps -vs tools/wiiu-shaders/diagnostic_triangle.vs -o data/diagnostic_triangle_shader.bin"
```

Expected: command exits successfully and writes `C:\dev\helworks\helengine-wiiu\data\diagnostic_triangle_shader.bin`.

- [ ] **Step 4: Run the contract test to verify the asset side is now complete**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL, but now because the presenter-owned triangle method and shader loading path still do not exist.

- [ ] **Step 5: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add tools/wiiu-shaders/diagnostic_triangle.vs tools/wiiu-shaders/diagnostic_triangle.ps data/diagnostic_triangle_shader.bin
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: add Wii U diagnostic triangle shaders"
```

### Task 3: Implement The Presenter-Owned Triangle Draw Path

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing presenter declarations**

Add these declarations to `WiiUGx2Presenter.hpp` alongside the existing diagnostic square resources:

```cpp
/// Renders one presenter-owned diagnostic triangle frame for first-pass 3D shader verification.
void RenderDiagnosticTriangleFrame();
```

Add these members in the presenter state:

```cpp
bool AreDiagnosticTriangleResourcesInitialized;
WHBGfxShaderGroup DiagnosticTriangleShaderGroup;
GX2RBuffer DiagnosticTrianglePositionBuffer;
GX2RBuffer DiagnosticTriangleColorBuffer;
```

Add these private helpers:

```cpp
void InitializeDiagnosticTriangleResources();
void ShutdownDiagnosticTriangleResources();
void RenderDiagnosticTriangleToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);
```

- [ ] **Step 2: Run the contract test to verify it fails on missing implementation**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL because `diagnostic_triangle_shader_bin.h`, shader loading, and draw code are still absent.

- [ ] **Step 3: Write the minimal presenter implementation**

At the top of `WiiUGx2Presenter.cpp`, add:

```cpp
#include "diagnostic_triangle_shader_bin.h"
```

Add triangle constants near the other presenter constants:

```cpp
constexpr std::uint32_t DiagnosticTriangleVertexCount = 3U;
constexpr std::uint32_t DiagnosticTriangleVertexElementSize = 4U * sizeof(float);
const float DiagnosticTrianglePositionData[] = {
    0.0f, 0.7f, 0.0f, 1.0f,
    -0.7f, -0.7f, 0.0f, 1.0f,
    0.7f, -0.7f, 0.0f, 1.0f
};
const float DiagnosticTriangleColorData[] = {
    1.0f, 0.2f, 0.2f, 1.0f,
    0.2f, 1.0f, 0.2f, 1.0f,
    0.2f, 0.4f, 1.0f, 1.0f
};
```

In the constructor initializer list, add:

```cpp
, AreDiagnosticTriangleResourcesInitialized(false)
, DiagnosticTriangleShaderGroup()
, DiagnosticTrianglePositionBuffer()
, DiagnosticTriangleColorBuffer()
```

After the existing `std::memset` calls, add:

```cpp
std::memset(&DiagnosticTriangleShaderGroup, 0, sizeof(DiagnosticTriangleShaderGroup));
std::memset(&DiagnosticTrianglePositionBuffer, 0, sizeof(DiagnosticTrianglePositionBuffer));
std::memset(&DiagnosticTriangleColorBuffer, 0, sizeof(DiagnosticTriangleColorBuffer));
```

Inside `Initialize()`, after `InitializeDiagnosticSquareResources();`, add:

```cpp
InitializeDiagnosticTriangleResources();
```

Inside `Shutdown()`, release the triangle resources before the UI quad resources:

```cpp
ShutdownDiagnosticTriangleResources();
```

Implement `InitializeDiagnosticTriangleResources()` using the existing square path as the template, but with two vertex buffers:

```cpp
void WiiUGx2Presenter::InitializeDiagnosticTriangleResources() {
    if (AreDiagnosticTriangleResourcesInitialized) {
        return;
    }
    if (!WHBGfxLoadGFDShaderGroup(&DiagnosticTriangleShaderGroup, 0, diagnostic_triangle_shader_bin)) {
        throw std::runtime_error("Could not load Wii U diagnostic triangle shader group.");
    }

    DiagnosticTrianglePositionBuffer.flags = DiagnosticVertexBufferFlags;
    DiagnosticTrianglePositionBuffer.elemSize = DiagnosticTriangleVertexElementSize;
    DiagnosticTrianglePositionBuffer.elemCount = DiagnosticTriangleVertexCount;
    GX2RCreateBuffer(&DiagnosticTrianglePositionBuffer);

    DiagnosticTriangleColorBuffer.flags = DiagnosticVertexBufferFlags;
    DiagnosticTriangleColorBuffer.elemSize = DiagnosticTriangleVertexElementSize;
    DiagnosticTriangleColorBuffer.elemCount = DiagnosticTriangleVertexCount;
    GX2RCreateBuffer(&DiagnosticTriangleColorBuffer);

    void* positionData = GX2RLockBufferEx(&DiagnosticTrianglePositionBuffer, GX2R_RESOURCE_BIND_NONE);
    std::memcpy(positionData, DiagnosticTrianglePositionData, sizeof(DiagnosticTrianglePositionData));
    GX2RUnlockBufferEx(&DiagnosticTrianglePositionBuffer, GX2R_RESOURCE_BIND_NONE);

    void* colorData = GX2RLockBufferEx(&DiagnosticTriangleColorBuffer, GX2R_RESOURCE_BIND_NONE);
    std::memcpy(colorData, DiagnosticTriangleColorData, sizeof(DiagnosticTriangleColorData));
    GX2RUnlockBufferEx(&DiagnosticTriangleColorBuffer, GX2R_RESOURCE_BIND_NONE);

    AreDiagnosticTriangleResourcesInitialized = true;
}
```

Implement the frame method:

```cpp
void WiiUGx2Presenter::RenderDiagnosticTriangleFrame() {
    if (!IsInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticTriangleFrame.");
    } else if (!AreDiagnosticTriangleResourcesInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must initialize triangle resources before RenderDiagnosticTriangleFrame.");
    }

    RenderDiagnosticTriangleToColorBuffer(TvContextState, &TvColorBuffer);
    RenderDiagnosticTriangleToColorBuffer(DrcContextState, &DrcColorBuffer);
    PresentScanBuffers();
}
```

Implement the per-target draw helper:

```cpp
void WiiUGx2Presenter::RenderDiagnosticTriangleToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer) {
    GX2SetContextState(contextState);
    GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
    GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
    GX2ClearColor(colorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);

    WHBGfxShaderGroup* shaderGroup = &DiagnosticTriangleShaderGroup;
    GX2SetFetchShader(&shaderGroup->fetchShader);
    GX2SetVertexShader(shaderGroup->vertexShader);
    GX2SetPixelShader(shaderGroup->pixelShader);
    GX2RSetAttributeBuffer(&DiagnosticTrianglePositionBuffer, 0, DiagnosticTrianglePositionBuffer.elemSize, 0);
    GX2RSetAttributeBuffer(&DiagnosticTriangleColorBuffer, 1, DiagnosticTriangleColorBuffer.elemSize, 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticTriangleVertexCount, 0, 1);
}
```

Implement `ShutdownDiagnosticTriangleResources()`:

```cpp
void WiiUGx2Presenter::ShutdownDiagnosticTriangleResources() {
    if (!AreDiagnosticTriangleResourcesInitialized) {
        return;
    }

    GX2RDestroyBufferEx(&DiagnosticTrianglePositionBuffer, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&DiagnosticTriangleColorBuffer, GX2R_RESOURCE_BIND_NONE);
    WHBGfxFreeShaderGroup(&DiagnosticTriangleShaderGroup);
    std::memset(&DiagnosticTrianglePositionBuffer, 0, sizeof(DiagnosticTrianglePositionBuffer));
    std::memset(&DiagnosticTriangleColorBuffer, 0, sizeof(DiagnosticTriangleColorBuffer));
    std::memset(&DiagnosticTriangleShaderGroup, 0, sizeof(DiagnosticTriangleShaderGroup));
    AreDiagnosticTriangleResourcesInitialized = false;
}
```

- [ ] **Step 4: Run the contract test to verify it now only fails on application routing**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: FAIL because `WiiUApplication` still presents the 2D frame path.

- [ ] **Step 5: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: add Wii U presenter-owned diagnostic triangle"
```

### Task 4: Route Visible Output Through The Triangle Path And Verify In Cemu

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`
- Test: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`

- [ ] **Step 1: Write the minimal routing change**

In `WiiUApplication::PresentRenderedFrame()`, replace the 2D frame presentation call:

```cpp
Gx2Presenter->RenderFrame(EngineRenderManager2D->GetCurrentFrame());
```

with:

```cpp
Gx2Presenter->RenderDiagnosticTriangleFrame();
```

- [ ] **Step 2: Run the contract test to verify it passes**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp -v minimal
```

Expected: PASS

- [ ] **Step 3: Run the focused Wii U seam subset**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp|FullyQualifiedName~RuntimeSeam_PresentsCapturedGx2FrameThroughDedicatedPresenter|FullyQualifiedName~RuntimeSeam_UsesFullEngineFrameLoopAfterDiagnosticMeasurement" -v minimal
```

Expected: PASS

- [ ] **Step 4: Rebuild the packaged Wii U artifact**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\city\wiiu-build
```

Expected: `Build completed for platform 'wiiu': C:\dev\helprojs\city\wiiu-build`

- [ ] **Step 5: Launch the packaged WUHB in Cemu**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

Expected: launcher prints `PROCESS_ID=` and Cemu starts with the updated Wii U build.

- [ ] **Step 6: Check the runtime trace for a clean draw loop**

Run:

```powershell
rtk powershell -NoProfile -Command "Get-Content 'C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt' | Select-Object -Last 40"
```

Expected: startup plus update/draw trace with no `Engine draw threw` or presenter initialization failure.

- [ ] **Step 7: Capture the visual result**

Run:

```powershell
rtk powershell -NoProfile -Command @'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Start-Sleep -Milliseconds 1500
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$path = 'C:\dev\helworks\helengine-wiiu\tmp\cemu-diagnostic-triangle.png'
$bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
Write-Output $path
'@
```

Expected: `C:\dev\helworks\helengine-wiiu\tmp\cemu-diagnostic-triangle.png`

- [ ] **Step 8: Manual visual verification**

Confirm the captured frame shows:

- lavender background
- one large colored triangle
- steady-state FPS rather than a frozen frame

- [ ] **Step 9: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiUApplication.cpp builder.tests/WiiURuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: verify Wii U GX2 diagnostic triangle frame"
```
