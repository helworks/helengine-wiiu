# Wii U Pure GX2 Square Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the working pure GX2 clear-only frame to render one hard-coded centered square through GX2 with no software-surface upload path.

**Architecture:** Keep `WiiUApplication` on the already-verified clear-only presenter-owned frame path. Add a second presenter-owned method that renders a full-frame background clear plus a centered square using GX2 state only, implemented with a scissored clear so we avoid vertex/shader setup in this slice.

**Tech Stack:** C++17, `wut`, GX2, Cemu, xUnit source-contract tests, PowerShell build/launch scripts

---

### Task 1: Lock The Presenter-Owned GX2 Square Contract

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test near the other pure GX2 runtime seam tests:

```csharp
/// <summary>
/// Ensures the second pure GX2 bring-up slice renders a presenter-owned centered square through GX2 scissor state instead of software-surface upload.
/// </summary>
[Fact]
public void RuntimeSeam_UsesPresenterOwnedPureGx2SquareFrameForVisibleOutput() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

    Assert.Contains("void RenderDiagnosticSquareFrame();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderDiagnosticSquareFrame();", applicationSource, StringComparison.Ordinal);
    Assert.Contains("GX2SetScissor(", presenterSource, StringComparison.Ordinal);
    Assert.Contains("GX2ClearColor(&TvColorBuffer", presenterSource, StringComparison.Ordinal);
    Assert.Contains("GX2ClearColor(&DrcColorBuffer", presenterSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->RenderDiagnosticClearFrame();", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedPureGx2SquareFrameForVisibleOutput -v minimal
```

Expected: FAIL because `RenderDiagnosticSquareFrame()` does not exist yet and `WiiUApplication` still routes visible output through `RenderDiagnosticClearFrame()`.

- [ ] **Step 3: Write minimal implementation**

Update `WiiUGx2Presenter.hpp` to declare the square method:

```cpp
/// Renders one presenter-owned pure GX2 clear-plus-square frame for bring-up verification.
void RenderDiagnosticSquareFrame();
```

Update `WiiUGx2Presenter.cpp` to implement the method by:

1. clearing the full TV and DRC buffers to the existing lavender color
2. setting a centered scissor rectangle on each buffer
3. clearing only that rectangle to a contrasting square color
4. restoring the full-surface scissor
5. presenting the scan buffers

Use code shaped like this:

```cpp
void WiiUGx2Presenter::RenderDiagnosticSquareFrame() {
    if (!IsInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticSquareFrame.");
    }

    const std::uint32_t tvSquareWidth = TvColorBuffer.surface.width / 2U;
    const std::uint32_t tvSquareHeight = TvColorBuffer.surface.height / 2U;
    const std::uint32_t tvSquareX = (TvColorBuffer.surface.width - tvSquareWidth) / 2U;
    const std::uint32_t tvSquareY = (TvColorBuffer.surface.height - tvSquareHeight) / 2U;
    const std::uint32_t drcSquareWidth = DrcColorBuffer.surface.width / 2U;
    const std::uint32_t drcSquareHeight = DrcColorBuffer.surface.height / 2U;
    const std::uint32_t drcSquareX = (DrcColorBuffer.surface.width - drcSquareWidth) / 2U;
    const std::uint32_t drcSquareY = (DrcColorBuffer.surface.height - drcSquareHeight) / 2U;

    GX2SetContextState(TvContextState);
    GX2SetScissor(0, 0, TvColorBuffer.surface.width, TvColorBuffer.surface.height);
    GX2ClearColor(&TvColorBuffer, 0.5294118f, 0.36862746f, 0.6392157f, 1.0f);
    GX2SetScissor(tvSquareX, tvSquareY, tvSquareWidth, tvSquareHeight);
    GX2ClearColor(&TvColorBuffer, 1.0f, 0.92156863f, 0.0f, 1.0f);
    GX2SetScissor(0, 0, TvColorBuffer.surface.width, TvColorBuffer.surface.height);

    GX2SetContextState(DrcContextState);
    GX2SetScissor(0, 0, DrcColorBuffer.surface.width, DrcColorBuffer.surface.height);
    GX2ClearColor(&DrcColorBuffer, 0.5294118f, 0.36862746f, 0.6392157f, 1.0f);
    GX2SetScissor(drcSquareX, drcSquareY, drcSquareWidth, drcSquareHeight);
    GX2ClearColor(&DrcColorBuffer, 1.0f, 0.92156863f, 0.0f, 1.0f);
    GX2SetScissor(0, 0, DrcColorBuffer.surface.width, DrcColorBuffer.surface.height);

    PresentScanBuffers();
}
```

Update `WiiUApplication.cpp` so `PresentRenderedFrame()` calls:

```cpp
Gx2Presenter->RenderDiagnosticSquareFrame();
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedPureGx2SquareFrameForVisibleOutput -v minimal
```

Expected: PASS

- [ ] **Step 5: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUApplication.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: add wiiu pure gx2 square frame"
```

### Task 2: Verify The Packaged Wii U Square Frame In Cemu

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Test: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`

- [ ] **Step 1: Run the smallest launcher regression test**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiUCemuLauncherScriptTests -v minimal
```

Expected: PASS

- [ ] **Step 2: Rebuild the packaged Wii U artifact**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\city\wiiu-build
```

Expected: `Build completed for platform 'wiiu': C:\dev\helprojs\city\wiiu-build`

- [ ] **Step 3: Launch the packaged WUHB in Cemu**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

Expected: launcher prints `PROCESS_ID=` and Cemu starts with the packaged title mounted from the WUHB.

- [ ] **Step 4: Check the runtime trace for clean boot**

Run:

```powershell
Get-Content -Path C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt -Tail 40 | Out-String
```

Expected: packaged startup trace with no `Engine draw threw` failure.

- [ ] **Step 5: Manual visual verification**

Confirm in Cemu that the screen shows the same lavender background plus a centered contrasting square.

- [ ] **Step 6: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUApplication.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "test: verify wiiu pure gx2 square frame"
```
