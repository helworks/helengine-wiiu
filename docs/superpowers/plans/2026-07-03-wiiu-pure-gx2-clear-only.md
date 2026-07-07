# Wii U Pure GX2 Clear-Only Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current visible Wii U frame path with a pure GX2 clear-only frame that does not depend on `WiiUSoftwareSurface` upload.

**Architecture:** Keep `WiiUApplication` as the runtime owner, but make the visible rendered frame path call a GX2-native diagnostic clear owned by `WiiUGx2Presenter`. Preserve the current engine update/draw lifecycle and runtime tracing, but stop using software-surface pixels as the visible success path for this slice.

**Tech Stack:** C++17, `wut`, GX2, Cemu, xUnit source-contract tests, PowerShell build/launch scripts

---

### Task 1: Lock The Clear-Only Runtime Contract

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test near the other Wii U runtime seam tests:

```csharp
/// <summary>
/// Ensures the first pure GX2 bring-up slice renders a presenter-owned clear-only frame instead of uploading software-surface pixels for visible output.
/// </summary>
[Fact]
public void RuntimeSeam_UsesPresenterOwnedPureGx2ClearFrameForVisibleOutput() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

    Assert.Contains("void RenderDiagnosticClearFrame();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderDiagnosticClearFrame();", applicationSource, StringComparison.Ordinal);
    Assert.Contains("GX2ClearColor(&TvColorBuffer", presenterSource, StringComparison.Ordinal);
    Assert.Contains("GX2ClearColor(&DrcColorBuffer", presenterSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->Present(TvSurface, DrcSurface);", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedPureGx2ClearFrameForVisibleOutput -v minimal
```

Expected: FAIL because `WiiUGx2Presenter` does not expose `RenderDiagnosticClearFrame()` yet and `WiiUApplication` still routes visible output through the old presenter call shape.

- [ ] **Step 3: Write minimal implementation**

Update `WiiUGx2Presenter.hpp` to declare the clear-only method:

```cpp
/// Renders one presenter-owned pure GX2 clear-only frame for early bring-up verification.
void RenderDiagnosticClearFrame();
```

Update `WiiUGx2Presenter.cpp` to implement the method by clearing and presenting both color buffers:

```cpp
void WiiUGx2Presenter::RenderDiagnosticClearFrame() {
    if (!IsInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticClearFrame.");
    }

    GX2SetContextState(TvContextState);
    GX2ClearColor(&TvColorBuffer, 0.5294118f, 0.36862746f, 0.6392157f, 1.0f);
    GX2SetContextState(DrcContextState);
    GX2ClearColor(&DrcColorBuffer, 0.5294118f, 0.36862746f, 0.6392157f, 1.0f);
    PresentScanBuffers();
}
```

Update `WiiUApplication.cpp` so `PresentRenderedFrame()` becomes:

```cpp
void WiiUApplication::PresentRenderedFrame() {
    if (Gx2Presenter == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter must exist before rendered presentation can begin.");
    }

    Gx2Presenter->RenderDiagnosticClearFrame();
}
```

Keep `TvSurface` and `DrcSurface` allocation intact for now. This slice changes only the visible frame path.

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedPureGx2ClearFrameForVisibleOutput -v minimal
```

Expected: PASS

- [ ] **Step 5: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUApplication.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: add wiiu pure gx2 clear frame"
```

### Task 2: Verify The Real Wii U Build Uses The Clear-Only Path

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Test: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`

- [ ] **Step 1: Run the smallest launcher regression test**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiUCemuLauncherScriptTests -v minimal
```

Expected: PASS

- [ ] **Step 2: Rebuild the Wii U package**

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

- [ ] **Step 4: Check the runtime trace**

Run:

```powershell
Get-Content -Path C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt -Tail 40 | Out-String
```

Expected: the usual packaged startup trace with no `Engine draw threw` failure.

- [ ] **Step 5: Manual visual verification**

Confirm in Cemu that the screen shows the configured solid GX2 background color instead of a black frame.

- [ ] **Step 6: Commit**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUApplication.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "test: verify wiiu pure gx2 clear path"
```
