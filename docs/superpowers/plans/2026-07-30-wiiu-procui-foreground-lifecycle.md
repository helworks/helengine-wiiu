# Wii U ProcUI Foreground Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the custom GX2 presenter release and reacquire foreground-owned resources so HOME can resume the software or exit normally to the Wii U Menu.

**Architecture:** `WiiUGx2Presenter` will register ProcUI acquire/release callbacks and divide resources into persistent MEM2/GX2 state and foreground-dependent scan-buffer/MEM1 surfaces. Clear-only presentation remains selected for the first hardware validation; captured presentation is restored only after foreground resume and exit pass.

**Tech Stack:** C++17 Wii U runtime, devkitPro wut/libwhb ProcUI and GX2 APIs, C# .NET 9 xUnit source-contract tests, Helengine platform builder

## Global Constraints

- Support both HOME-to-software resume and HOME-to-Wii-U-Menu exit.
- Do not replace the custom presenter with `WHBGfx`.
- Do not modify generated code, shader binaries, engine frame capture, or unrelated renderer behavior.
- Keep `DiagnosticPresentationMode::ClearOnly` for the first hardware validation.
- Preserve all existing tracked and untracked workspace changes.
- Do not commit runtime/test files from the dirty shared tree because they contain earlier hardware diagnostics.
- Treat real-console lifecycle behavior, not compilation alone, as proof of correctness.

---

### Task 1: Define the ProcUI Foreground Contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

**Interfaces:**
- Consumes: current `WiiUGx2Presenter` source and header text.
- Produces: failing contracts for callback registration, foreground acquire/release ordering, and foreground state.

- [ ] **Step 1: Add the callback-registration test**

```csharp
/// <summary>
/// Ensures the custom GX2 presenter participates in ProcUI foreground ownership instead of acknowledging release while scan buffers remain allocated.
/// </summary>
[Fact]
public void RuntimeSeam_RegistersPresenterProcUiForegroundCallbacks() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

    Assert.Contains("static std::uint32_t HandleForegroundAcquired(void* context);", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static std::uint32_t HandleForegroundReleased(void* context);", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("bool AcquireForegroundResources();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void ReleaseForegroundResources();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("#include <proc_ui/procui.h>", presenterSource, StringComparison.Ordinal);
    Assert.Contains("ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &WiiUGx2Presenter::HandleForegroundAcquired, this, 100);", presenterSource, StringComparison.Ordinal);
    Assert.Contains("ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, &WiiUGx2Presenter::HandleForegroundReleased, this, 100);", presenterSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Add the resource-ordering test**

```csharp
/// <summary>
/// Ensures foreground release drains GPU work before freeing every foreground allocation and acquire rebuilds every released display dependency.
/// </summary>
[Fact]
public void RuntimeSeam_ReleasesAndReacquiresPresenterForegroundResources() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    int acquireStartIndex = presenterSource.IndexOf("bool WiiUGx2Presenter::AcquireForegroundResources()", StringComparison.Ordinal);
    int releaseStartIndex = presenterSource.IndexOf("void WiiUGx2Presenter::ReleaseForegroundResources()", StringComparison.Ordinal);
    int shutdownStartIndex = presenterSource.IndexOf("void WiiUGx2Presenter::Shutdown()", StringComparison.Ordinal);
    string acquireSource = presenterSource.Substring(acquireStartIndex, releaseStartIndex - acquireStartIndex);
    string releaseSource = presenterSource.Substring(releaseStartIndex, shutdownStartIndex - releaseStartIndex);

    Assert.Contains("bool AreForegroundResourcesAcquired;", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("GfxHeapInitMEM1()", acquireSource, StringComparison.Ordinal);
    Assert.Contains("GfxHeapInitForeground()", acquireSource, StringComparison.Ordinal);
    Assert.Contains("TvScanBuffer = GfxHeapAllocForeground", acquireSource, StringComparison.Ordinal);
    Assert.Contains("DrcScanBuffer = GfxHeapAllocForeground", acquireSource, StringComparison.Ordinal);
    Assert.Contains("InitializeTvColorBuffer();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("InitializeDrcColorBuffer();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("InitializeTvDepthBuffer();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("InitializeDrcDepthBuffer();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("InitializeDirectionalShadowResources();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("ConfigurePresentationContexts();", acquireSource, StringComparison.Ordinal);
    Assert.Contains("AreForegroundResourcesAcquired = true;", acquireSource, StringComparison.Ordinal);

    int drawDoneIndex = releaseSource.IndexOf("GX2DrawDone();", StringComparison.Ordinal);
    int shadowDestroyIndex = releaseSource.IndexOf("DestroyDirectionalShadowResources();", StringComparison.Ordinal);
    int tvSurfaceDestroyIndex = releaseSource.IndexOf("GX2RDestroySurfaceEx(&TvColorBuffer.surface", StringComparison.Ordinal);
    int tvScanFreeIndex = releaseSource.IndexOf("GfxHeapFreeForeground(TvScanBuffer);", StringComparison.Ordinal);
    int mem1DestroyIndex = releaseSource.IndexOf("GfxHeapDestroyMEM1();", StringComparison.Ordinal);
    int foregroundDestroyIndex = releaseSource.IndexOf("GfxHeapDestroyForeground();", StringComparison.Ordinal);
    Assert.True(drawDoneIndex >= 0 && drawDoneIndex < shadowDestroyIndex);
    Assert.True(shadowDestroyIndex < tvSurfaceDestroyIndex);
    Assert.True(tvSurfaceDestroyIndex < tvScanFreeIndex);
    Assert.True(tvScanFreeIndex < mem1DestroyIndex);
    Assert.True(mem1DestroyIndex < foregroundDestroyIndex);
    Assert.Contains("AreForegroundResourcesAcquired = false;", releaseSource, StringComparison.Ordinal);
}
```

- [ ] **Step 3: Run both tests and verify expected RED failures**

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_RegistersPresenterProcUiForegroundCallbacks|FullyQualifiedName~RuntimeSeam_ReleasesAndReacquiresPresenterForegroundResources" --artifacts-path .diagnostics/procui_lifecycle_test --verbosity minimal
```

Expected: both fail because the presenter has no callback or foreground resource contract.

---

### Task 2: Implement Presenter-Owned Foreground Lifecycle

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp:48-207`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp:21-636`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

**Interfaces:**
- Consumes: `ProcUIRegisterCallback`, existing display-surface initializers, and directional-shadow resource methods.
- Produces: `HandleForegroundAcquired`, `HandleForegroundReleased`, `AcquireForegroundResources`, `ReleaseForegroundResources`, `ConfigurePresentationContexts`, `AreForegroundResourcesAcquired`, and `AreProcUiCallbacksRegistered`.

- [ ] **Step 1: Add lifecycle declarations and state**

Add before `Shutdown()`:

```cpp
/// Adapts the ProcUI foreground-acquire callback to the owning presenter instance without allowing exceptions across the C ABI.
static std::uint32_t HandleForegroundAcquired(void* context);

/// Adapts the ProcUI foreground-release callback to the owning presenter instance.
static std::uint32_t HandleForegroundReleased(void* context);

/// Allocates and binds every GX2 resource whose storage is invalid outside the application foreground.
bool AcquireForegroundResources();

/// Drains GPU work and releases every GX2 resource whose storage belongs to the application foreground.
void ReleaseForegroundResources();

/// Rebuilds both GX2 context states after foreground display surfaces have been recreated.
void ConfigurePresentationContexts();
```

Add two documented fields, `bool AreForegroundResourcesAcquired;` and `bool AreProcUiCallbacksRegistered;`, and initialize them to `false` in the constructor.

- [ ] **Step 2: Add callback adapters**

Include `<proc_ui/procui.h>`. Implement both static callbacks with null checks. The acquire adapter returns `0U` only when `AcquireForegroundResources()` succeeds; the release adapter calls `ReleaseForegroundResources()` and returns `0U`. No exception may cross either callback.

```cpp
std::uint32_t WiiUGx2Presenter::HandleForegroundAcquired(void* context) {
    if (context == nullptr) {
        return static_cast<std::uint32_t>(-1);
    }

    WiiUGx2Presenter* presenter = static_cast<WiiUGx2Presenter*>(context);
    return presenter->AcquireForegroundResources() ? 0U : static_cast<std::uint32_t>(-1);
}
```

- [ ] **Step 3: Extract display-context configuration**

`ConfigurePresentationContexts()` must set up each context, bind its recreated color/depth buffers, configure viewport/scissor, set TV/DRC scale, and finish with `GX2SetSwapInterval(1)`. Move the existing commands without changing their values.

- [ ] **Step 4: Implement idempotent acquisition**

Return `true` when already acquired. Otherwise initialize MEM1 then foreground heaps, allocate/invalidate/bind both scan buffers, call all four display-surface initializers, call `InitializeDirectionalShadowResources()`, call `ConfigurePresentationContexts()`, and set `AreForegroundResourcesAcquired = true` only at the end.

Use these exact calls:

```cpp
TvScanBuffer = GfxHeapAllocForeground(TvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
DrcScanBuffer = GfxHeapAllocForeground(DrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
GX2SetTVBuffer(TvScanBuffer, TvScanBufferSize, PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
GX2SetDRCBuffer(DrcScanBuffer, DrcScanBufferSize, GX2GetSystemDRCMode(), PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
InitializeTvColorBuffer();
InitializeDrcColorBuffer();
InitializeTvDepthBuffer();
InitializeDrcDepthBuffer();
InitializeDirectionalShadowResources();
ConfigurePresentationContexts();
```

Wrap acquisition in `try/catch`; on any false return or exception call `ReleaseForegroundResources()` and return `false`.

- [ ] **Step 5: Implement idempotent partial-safe release**

If GX2 is initialized, call `GX2DrawDone()` first. Destroy directional-shadow resources, then all four non-null display surfaces. Free/null both scan buffers. Destroy each initialized heap and clear its flag. Set `AreForegroundResourcesAcquired = false` last. Do not return early merely because the final acquired flag is false; partial initialization still needs cleanup.

- [ ] **Step 6: Refactor initialization**

Keep command-buffer allocation, `GX2Init`, scan-size calculation, GX2R allocator registration, and persistent context allocation. Replace direct heap/scan/display/context/shadow setup with `AcquireForegroundResources()`. Initialize persistent diagnostic, scene, StandardShader, and UI resources afterward. Set `IsInitialized = true`, then register callbacks once:

```cpp
if (!AreProcUiCallbacksRegistered) {
    ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &WiiUGx2Presenter::HandleForegroundAcquired, this, 100);
    ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, &WiiUGx2Presenter::HandleForegroundReleased, this, 100);
    AreProcUiCallbacksRegistered = true;
}
```

- [ ] **Step 7: Refactor shutdown**

Call `ReleaseForegroundResources()` first. Remove duplicated foreground cleanup. Destroy persistent UI, StandardShader, scene, and diagnostic resources; clear the GX2R allocator; call `GX2Shutdown()`; free contexts and command buffer. Do not reset `AreProcUiCallbacksRegistered`, because ProcUI provides no unregister API and duplicate registration must remain impossible.

- [ ] **Step 8: Guard public render entry points**

In both `RenderFrame` overloads and all three diagnostic frame methods, add:

```cpp
} else if (!AreForegroundResourcesAcquired) {
    throw std::runtime_error("Wii U GX2 presenter requires foreground graphics ownership before rendering.");
```

- [ ] **Step 9: Rebuild and run lifecycle tests**

Rebuild the isolated test assembly, then run both new tests with `--no-build`. Expected: two passed, zero failed.

- [ ] **Step 10: Run related GX2 regressions**

Run both lifecycle tests together with clear-only presentation, heap-placement, and base-vertex contracts. Expected: all selected tests pass.

- [ ] **Step 11: Review the scoped diff**

```powershell
git -c core.autocrlf=false diff --check -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp
git diff --stat -- builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp
git diff -- src/platform/wiiu/WiiUGx2Presenter.hpp
```

Expected: lifecycle declarations/state, adapters, acquire/release/configuration, initialization/shutdown refactoring, render guards, and focused tests. Do not commit the dirty shared files.

---

### Task 3: Build and Test the Clear-Only Lifecycle Artifact

**Files:**
- Verify: `src/platform/wiiu/WiiUApplication.cpp`
- Create: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`

**Interfaces:**
- Consumes: clear-only presentation and Task 2 lifecycle implementation.
- Produces: one fresh WUHB for real-console foreground validation.

- [ ] **Step 1: Verify clear-only mode remains selected**

```powershell
rg -n "DiagnosticPresentationModeValue = DiagnosticPresentationMode::ClearOnly" src/platform/wiiu/WiiUApplication.cpp
```

Expected: exactly one active selection.

- [ ] **Step 2: Build the authoritative WUHB**

Use the existing build-waiter DLL and redirect native output to `.diagnostics/procui_lifecycle_native_build.log`. Expected: exit zero and the fresh-artifact marker.

- [ ] **Step 3: Record artifact identity**

Record absolute path, byte length, last-write timestamp, and SHA-256.

- [ ] **Step 4: Perform real-hardware lifecycle validation**

Cold-launch and confirm pastel-lilac output on both displays. Press HOME and resume; both displays must return to lilac. Press HOME again and choose Wii U Menu; the transition must finish without holding power. Stop and record exact behavior if either operation fails.

---

### Task 4: Restore Captured Presentation After Lifecycle Success

**Files:**
- Modify: `src/platform/wiiu/WiiUApplication.cpp:72`
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Create: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`

**Interfaces:**
- Consumes: a successful Task 3 hardware result.
- Produces: a captured-frame WUHB retaining verified foreground lifecycle.

- [ ] **Step 1: Change the presentation contract**

Require `DiagnosticPresentationModeValue = DiagnosticPresentationMode::CapturedFrame` in the focused application test.

- [ ] **Step 2: Run the focused test and verify it fails**

Expected: failure because `ClearOnly` remains selected.

- [ ] **Step 3: Change only the constant**

```cpp
constexpr DiagnosticPresentationMode DiagnosticPresentationModeValue = DiagnosticPresentationMode::CapturedFrame;
```

- [ ] **Step 4: Rebuild and verify focused and lifecycle tests**

Expected: zero failures.

- [ ] **Step 5: Build and identify a fresh WUHB**

Use `.diagnostics/procui_captured_native_build.log`; record path, size, timestamp, hash, and fresh-artifact marker.

- [ ] **Step 6: Perform captured-frame hardware validation**

Record TV output, GamePad output, HOME resume, and HOME exit. Treat any remaining dark-purple/black output as a separate rendering investigation only if lifecycle transitions still pass.
