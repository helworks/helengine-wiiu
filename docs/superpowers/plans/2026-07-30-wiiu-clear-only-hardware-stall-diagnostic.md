# Wii U Clear-Only Hardware Stall Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a real-Wii-U diagnostic WUHB that retains GX2 initialization and presentation while bypassing all captured 3D and 2D draw submission.

**Architecture:** Add an explicit compile-time diagnostic presentation mode in `WiiUApplication.cpp`. Clear-only mode makes `PresentRenderedFrame()` call the existing `RenderDiagnosticClearFrame()` method; captured-frame presentation remains the alternate branch.

**Tech Stack:** C++17 Wii U runtime, devkitPro/libwhb GX2 APIs, C# .NET 9 xUnit source-contract tests, Helengine platform builder

## Global Constraints

- Do not modify generated code, shader binaries, texture data, renderer allocations, or `RenderDiagnosticClearFrame()` itself.
- Retain application and engine initialization, the update/draw loop, GX2 initialization, both display buffers, scan-buffer copies, and process lifecycle.
- Change only presentation selection, preserve unrelated workspace changes, and do not describe compilation as proof of a hardware fix.

---

### Task 1: Select the Existing Clear-Only Presentation Boundary

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Modify: `src/platform/wiiu/WiiUApplication.cpp:55-63`
- Modify: `src/platform/wiiu/WiiUApplication.cpp:637-650`

**Interfaces:**
- Consumes: `void WiiUGx2Presenter::RenderDiagnosticClearFrame()` and `void WiiUGx2Presenter::RenderFrame(const WiiUGx23DRenderFrame&, const WiiUGx2RenderFrame&)`.
- Produces: `DiagnosticPresentationMode` and `DiagnosticPresentationModeValue`, used only by `WiiUApplication::PresentRenderedFrame()`.

- [ ] **Step 1: Write the failing source-contract test**

Add this test beside the existing Wii U runtime seam tests:

```csharp
/// <summary>
/// Ensures the hardware-stall diagnostic bypasses captured draw submission while retaining the presenter's GX2 clear-and-scan-buffer path.
/// </summary>
[Fact]
public void RuntimeSeam_SelectsClearOnlyHardwareStallDiagnosticPresentation() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
    int presentRenderedFrameStartIndex = applicationSource.IndexOf("void WiiUApplication::PresentRenderedFrame()", StringComparison.Ordinal);
    int appendRuntimeTraceStartIndex = applicationSource.IndexOf("void WiiUApplication::AppendRuntimeTrace(", presentRenderedFrameStartIndex, StringComparison.Ordinal);
    string presentRenderedFrameSource = applicationSource.Substring(presentRenderedFrameStartIndex, appendRuntimeTraceStartIndex - presentRenderedFrameStartIndex);

    Assert.Contains("enum class DiagnosticPresentationMode", applicationSource, StringComparison.Ordinal);
    Assert.Contains("constexpr DiagnosticPresentationMode DiagnosticPresentationModeValue = DiagnosticPresentationMode::ClearOnly;", applicationSource, StringComparison.Ordinal);
    Assert.Contains("if (DiagnosticPresentationModeValue == DiagnosticPresentationMode::ClearOnly) {", presentRenderedFrameSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderDiagnosticClearFrame();", presentRenderedFrameSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());", presentRenderedFrameSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test and verify the expected failure**

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_SelectsClearOnlyHardwareStallDiagnosticPresentation" --artifacts-path .diagnostics/clear_only_test --verbosity minimal
```

Expected: one failure because `DiagnosticPresentationMode` is absent and captured frames are still submitted unconditionally.

- [ ] **Step 3: Implement the diagnostic selector**

Add beside `DiagnosticFrameLoopMode`:

```cpp
/// Selects whether steady-state presentation submits captured engine frames or isolates the existing GX2 clear-only path.
enum class DiagnosticPresentationMode {
    ClearOnly,
    CapturedFrame
};

/// Selects clear-only presentation while the real-hardware GX2 stall is localized.
constexpr DiagnosticPresentationMode DiagnosticPresentationModeValue = DiagnosticPresentationMode::ClearOnly;
```

Change the end of `PresentRenderedFrame()` to:

```cpp
HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame begin.\n");
if (DiagnosticPresentationModeValue == DiagnosticPresentationMode::ClearOnly) {
    Gx2Presenter->RenderDiagnosticClearFrame();
    HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame clear-only completed.\n");
    return;
}

Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());
HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame captured frame completed.\n");
```

- [ ] **Step 4: Re-run the focused test and verify it passes**

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-build --filter "FullyQualifiedName~RuntimeSeam_SelectsClearOnlyHardwareStallDiagnosticPresentation" --artifacts-path .diagnostics/clear_only_test --verbosity minimal
```

Expected: one passed test and zero failures.

- [ ] **Step 5: Validate the exact diff**

```powershell
git diff --check -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUApplication.cpp
git diff -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUApplication.cpp
```

Expected: only the focused test, diagnostic enum/constant, and presentation branch are added by this task; line-ending warnings are allowed, whitespace errors are not.

- [ ] **Step 6: Build the authoritative DemoDisc WUHB**

```powershell
dotnet C:\dev\helworks\helengine\tools\build-waiter\bin\Debug\net9.0-windows\helengine.buildwaiter.dll --output C:\dev\helprojs\demodisc\wiiu-build --require helengine_wiiu.wuhb -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build
```

Expected: exit zero and the build-waiter fresh-artifact marker. Record the WUHB path, length, timestamp, and SHA-256.

- [ ] **Step 7: Test the diagnostic on real hardware**

Cold-launch the WUHB and record TV output, GamePad output, and whether HOME returns normally to the Wii U Menu.

- Two diagnostic clears plus normal exit localizes the stall to captured 3D/2D draw submission.
- Purple TV, black GamePad, and hanging exit proves captured draws are not required and moves investigation to shared GX2 initialization/presentation/teardown.
- A mixed result identifies the earliest divergent TV, DRC, or lifecycle boundary; record it before another modification.
