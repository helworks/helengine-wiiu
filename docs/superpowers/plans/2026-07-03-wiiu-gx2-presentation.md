# Wii U GX2 Presentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Wii U steady-state OSScreen per-pixel presentation path with a minimal GX2 presenter while keeping the current CPU software renderer intact.

**Architecture:** Add a new `WiiUGx2Presenter` platform seam that owns GX2 initialization, TV/DRC presentation resources, and per-frame upload from `WiiUSoftwareSurface`. Keep `WiiURenderManager2D` as the current CPU rasterizer and change only `WiiUApplication` so rendered frames delegate to the GX2 presenter after engine startup while boot and failure diagnostics stay on the current boot path.

**Tech Stack:** C++20, devkitPro wut GX2 APIs, existing Wii U source-contract tests in `builder.tests`, Cemu WUHB runtime verification.

---

## File Structure

### Existing files to modify

- `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
  - Add one new source-contract test that requires a GX2 presenter seam and rejects the old steady-state `OSScreenPutPixelEx` path.
- `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.hpp`
  - Add forward declarations and one presenter-owned member plus the new presenter initialization hook.
- `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
  - Construct, initialize, and destroy the presenter; route rendered presentation through it; preserve boot diagnostics.

### New files to create

- `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
  - Declare the GX2 presenter boundary and its platform-owned buffers.
- `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
  - Implement GX2 initialization, TV/DRC color-buffer allocation, CPU upload from `WiiUSoftwareSurface`, and scan-buffer presentation.

### Runtime verification targets

- `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`
- `C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt`
- `C:\Users\Helena\AppData\Roaming\Cemu\log.txt`

---

### Task 1: Lock The GX2 Presenter Contract In Source Tests

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test at the end of `WiiURuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures rendered Wii U frames delegate to a dedicated GX2 presenter seam instead of issuing steady-state OSScreen per-pixel writes inline.
/// </summary>
[Fact]
public void RuntimeSeam_PresentsRenderedFramesThroughGx2Presenter() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp");
    string presenterSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp");
    string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

    Assert.True(File.Exists(presenterHeaderPath), "Expected WiiUGx2Presenter.hpp to exist.");
    Assert.True(File.Exists(presenterSourcePath), "Expected WiiUGx2Presenter.cpp to exist.");
    Assert.Contains("class WiiUGx2Presenter;", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("WiiUGx2Presenter* Gx2Presenter;", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("#include \"platform/wiiu/WiiUGx2Presenter.hpp\"", applicationSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->Present(TvSurface, DrcSurface);", applicationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("OSScreenPutPixelEx(screen, x, y, ConvertSurfacePixelToScreenColor(pixels[pixelIndex]));", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_PresentsRenderedFramesThroughGx2Presenter --no-restore -v minimal
```

Expected: `FAIL` because `WiiUGx2Presenter.hpp` and `WiiUGx2Presenter.cpp` do not exist yet and `WiiUApplication` does not reference them.

- [ ] **Step 3: Commit the red test**

```bash
rtk git add C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs
rtk git commit -m "test: require wiiu gx2 presentation seam"
```

---

### Task 2: Add The GX2 Presenter Boundary And Application Ownership

**Files:**
- Create: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
- Create: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Add the presenter header**

Create `WiiUGx2Presenter.hpp` with this class skeleton:

```cpp
#pragma once

#include <cstdint>

#include <gx2/display.h>
#include <gx2/surface.h>

#include "platform/wiiu/WiiUSoftwareSurface.hpp"

namespace helengine::wiiu {
    /// Owns the minimal GX2 presentation seam that uploads CPU-rendered TV and DRC software surfaces into GX2-owned display buffers.
    class WiiUGx2Presenter {
    public:
        /// Creates one uninitialized GX2 presenter.
        WiiUGx2Presenter();

        /// Releases all GX2-owned presentation resources.
        ~WiiUGx2Presenter();

        /// Initializes the GX2 presentation path for TV and DRC output.
        bool Initialize();

        /// Uploads and presents the supplied TV and DRC software surfaces.
        void Present(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface);

    private:
        /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
        void Shutdown();

        /// Initializes the TV color buffer used for software-surface upload and presentation.
        void InitializeTvColorBuffer();

        /// Initializes the DRC color buffer used for software-surface upload and presentation.
        void InitializeDrcColorBuffer();

        /// Uploads one packed ARGB8888 software surface into one GX2 color buffer image.
        void UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer);

        /// Presents the current TV and DRC color buffers to their scan buffers.
        void PresentScanBuffers();

        /// Tracks whether GX2 resources were initialized successfully.
        bool IsInitialized;

        /// Stores the TV scan buffer pointer returned by the GX2 allocation path.
        void* TvScanBuffer;

        /// Stores the DRC scan buffer pointer returned by the GX2 allocation path.
        void* DrcScanBuffer;

        /// Stores the TV color buffer used for steady-state presentation.
        GX2ColorBuffer TvColorBuffer;

        /// Stores the DRC color buffer used for steady-state presentation.
        GX2ColorBuffer DrcColorBuffer;

        /// Stores the raw scan-buffer size required for TV presentation.
        std::uint32_t TvScanBufferSize;

        /// Stores the raw scan-buffer size required for DRC presentation.
        std::uint32_t DrcScanBufferSize;
    };
}
```

- [ ] **Step 2: Add application ownership in the header**

Update `WiiUApplication.hpp` with these declarations:

```cpp
#include "platform/wiiu/WiiUSoftwareSurface.hpp"

namespace helengine::wiiu {
    class WiiUGx2Presenter;

    class WiiUApplication {
    private:
        bool InitializeGx2Presenter();

        WiiUGx2Presenter* Gx2Presenter;
    };
}
```

Also remove the now-obsolete rendered-presentation declarations once the presenter owns steady-state output:

```cpp
void PresentSurface(OSScreenID screen, WiiUSoftwareSurface* surface);
std::uint32_t ConvertSurfacePixelToScreenColor(std::uint32_t surfacePixel) const;
```

- [ ] **Step 3: Add presenter construction and lifetime wiring**

Update `WiiUApplication.cpp` so the constructor, destructor, and run loop own the presenter:

```cpp
#include "platform/wiiu/WiiUGx2Presenter.hpp"

WiiUApplication::WiiUApplication()
    : TvBuffer(nullptr)
    , DrcBuffer(nullptr)
    , TvSurface(nullptr)
    , DrcSurface(nullptr)
    , BootPhase(WiiUBootPhase::NativeStartup)
    , ClearColor(StartupClearColor)
    , Gx2Presenter(nullptr)
#if HELENGINE_WIIU_HAS_GENERATED_CORE
    , EngineInitialized(false)
    , UpdateFrameLogCount(0)
    , DrawFrameLogCount(0)
    , EngineCore(nullptr)
    , EngineRenderManager3D(nullptr)
    , EngineRenderManager2D(nullptr)
    , EnginePlatformInfo(nullptr)
#endif
{
}

WiiUApplication::~WiiUApplication() {
    delete Gx2Presenter;
    Gx2Presenter = nullptr;
    // keep the existing engine and surface teardown below this point
}
```

Add the new initialization hook next to `InitializeVideo()`:

```cpp
bool WiiUApplication::InitializeGx2Presenter() {
    if (Gx2Presenter != nullptr) {
        return true;
    }

    Gx2Presenter = new WiiUGx2Presenter();
    return Gx2Presenter->Initialize();
}
```

Call it inside `Run()` immediately after `InitializeVideo()` succeeds:

```cpp
if (!InitializeGx2Presenter()) {
    AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter failed.\n");
    SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
    OSScreenShutdown();
    WHBProcShutdown();
    return 1;
}
```

- [ ] **Step 4: Run the focused test to verify it still fails for the right reason**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_PresentsRenderedFramesThroughGx2Presenter --no-restore -v minimal
```

Expected: `FAIL`, but now only because `PresentRenderedFrame()` still uses the OSScreen path and does not yet delegate to `Gx2Presenter->Present(...)`.

- [ ] **Step 5: Commit the presenter boundary**

```bash
rtk git add C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.hpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp
rtk git commit -m "feat: add wiiu gx2 presenter seam"
```

---

### Task 3: Implement Minimal Raw-GX2 TV/DRC Presentation

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Implement the raw-GX2 initialization path**

Use the public `wut` GX2 initialization pattern, mirroring the buffer-setup sequence already captured in `docs/Wii U Rendering Docs.md`:

```cpp
bool WiiUGx2Presenter::Initialize() {
    if (IsInitialized) {
        return true;
    }

    uint32_t initAttribs[] = {
        GX2_INIT_ARGC, 0,
        GX2_INIT_ARGV, 0,
        GX2_INIT_END
    };
    GX2Init(initAttribs);

    GX2SurfaceFormat surfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    GX2TVRenderMode tvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
    GX2DRCMode drcRenderMode = GX2GetSystemDRCMode();
    uint32_t unused = 0;

    GX2CalcTVSize(tvRenderMode, surfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &TvScanBufferSize, &unused);
    GX2CalcDRCSize(drcRenderMode, surfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &DrcScanBufferSize, &unused);

    TvScanBuffer = MEMAllocFromDefaultHeapEx(TvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
    DrcScanBuffer = MEMAllocFromDefaultHeapEx(DrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
    if (TvScanBuffer == nullptr || DrcScanBuffer == nullptr) {
        Shutdown();
        return false;
    }

    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvScanBuffer, TvScanBufferSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, DrcScanBuffer, DrcScanBufferSize);
    GX2SetTVBuffer(TvScanBuffer, TvScanBufferSize, tvRenderMode, surfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
    GX2SetDRCBuffer(DrcScanBuffer, DrcScanBufferSize, drcRenderMode, surfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

    InitializeTvColorBuffer();
    InitializeDrcColorBuffer();
    GX2SetSwapInterval(1);
    IsInitialized = true;
    return true;
}
```

- [ ] **Step 2: Implement the color-buffer setup helpers**

Use the same surface setup pattern for both display buffers:

```cpp
void WiiUGx2Presenter::InitializeTvColorBuffer() {
    std::memset(&TvColorBuffer, 0, sizeof(GX2ColorBuffer));
    TvColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
    TvColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    TvColorBuffer.surface.width = 1280;
    TvColorBuffer.surface.height = 720;
    TvColorBuffer.surface.depth = 1;
    TvColorBuffer.surface.mipLevels = 1;
    TvColorBuffer.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    TvColorBuffer.surface.aa = GX2_AA_MODE1X;
    TvColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
    TvColorBuffer.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&TvColorBuffer.surface);
    TvColorBuffer.surface.image = MEMAllocFromDefaultHeapEx(TvColorBuffer.surface.imageSize, TvColorBuffer.surface.alignment);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvColorBuffer.surface.image, TvColorBuffer.surface.imageSize);
    GX2InitColorBufferRegs(&TvColorBuffer);
}
```

Mirror the same helper for `DrcColorBuffer`, changing only the width and height to `854` and `480`.

- [ ] **Step 3: Implement the upload and present path**

Implement a simple full-frame CPU upload without partial dirty tracking:

```cpp
void WiiUGx2Presenter::UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer) {
    if (sourceSurface == nullptr) {
        throw std::runtime_error("Wii U GX2 presentation requires a valid source surface.");
    } else if (destinationBuffer == nullptr || destinationBuffer->surface.image == nullptr) {
        throw std::runtime_error("Wii U GX2 presentation requires a valid destination color buffer.");
    }

    std::memcpy(
        destinationBuffer->surface.image,
        sourceSurface->GetPixels(),
        static_cast<std::size_t>(sourceSurface->GetWidth()) * static_cast<std::size_t>(sourceSurface->GetHeight()) * sizeof(std::uint32_t));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, destinationBuffer->surface.image, destinationBuffer->surface.imageSize);
}

void WiiUGx2Presenter::PresentScanBuffers() {
    GX2SetColorBuffer(&TvColorBuffer, GX2_RENDER_TARGET_0);
    GX2CopyColorBufferToScanBuffer(&TvColorBuffer, GX2_SCAN_TARGET_TV);
    GX2SetColorBuffer(&DrcColorBuffer, GX2_RENDER_TARGET_0);
    GX2CopyColorBufferToScanBuffer(&DrcColorBuffer, GX2_SCAN_TARGET_DRC);
    GX2SwapScanBuffers();
    GX2Flush();
}

void WiiUGx2Presenter::Present(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface) {
    if (!IsInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must be initialized before Present.");
    }

    UploadSurface(tvSurface, &TvColorBuffer);
    UploadSurface(drcSurface, &DrcColorBuffer);
    PresentScanBuffers();
}
```

- [ ] **Step 4: Replace rendered-frame OSScreen presentation in `WiiUApplication`**

Update `PresentRenderedFrame()` to delegate to the presenter:

```cpp
void WiiUApplication::PresentRenderedFrame() {
    if (TvSurface == nullptr || DrcSurface == nullptr) {
        throw std::runtime_error("Wii U software surfaces must exist before rendered presentation can begin.");
    } else if (Gx2Presenter == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter must exist before rendered presentation can begin.");
    }

    Gx2Presenter->Present(TvSurface, DrcSurface);
}
```

Delete the obsolete steady-state helper implementations:

```cpp
void WiiUApplication::PresentSurface(OSScreenID screen, WiiUSoftwareSurface* surface) { ... }
std::uint32_t WiiUApplication::ConvertSurfacePixelToScreenColor(std::uint32_t surfacePixel) const { ... }
```

Keep `PresentBootPhaseFrame()` unchanged so the boot and failure path still uses the current diagnostic clear behavior.

- [ ] **Step 5: Run the focused test to verify it passes**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_PresentsRenderedFramesThroughGx2Presenter --no-restore -v minimal
```

Expected: `PASS`.

- [ ] **Step 6: Run the full Wii U runtime source test suite**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore -v minimal
```

Expected: all `WiiURuntimeSourceTests` pass.

- [ ] **Step 7: Commit the GX2 presentation implementation**

```bash
rtk git add C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.hpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp
rtk git commit -m "feat: present wiiu software surfaces through gx2"
```

---

### Task 4: Rebuild The Wii U Package And Verify In Cemu

**Files:**
- Runtime artifact: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`
- Runtime logs: `C:\Users\Helena\AppData\Roaming\Cemu\log.txt`
- Runtime logs: `C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt`

- [ ] **Step 1: Rebuild the Wii U package**

Run:

```bash
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\city\wiiu-build
```

Expected: build succeeds and refreshes `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`.

- [ ] **Step 2: Launch the new WUHB in Cemu**

Run:

```bash
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -Path C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

Expected: Cemu launches the packaged WUHB rather than the raw RPX.

- [ ] **Step 3: Check runtime logs after the run**

Run:

```bash
rtk powershell -NoProfile -Command "Get-Content C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt -Tail 120"
rtk powershell -NoProfile -Command "Get-Content C:\Users\Helena\AppData\Roaming\Cemu\log.txt -Tail 160"
```

Expected:

- the runtime trace still shows clean startup
- Cemu still loads the WUHB cleanly
- the menu remains visible
- the prior OSScreen steady-state behavior is no longer the active rendered-frame path

- [ ] **Step 4: Commit any final runtime-only follow-up if needed**

If no additional source changes were needed after runtime verification, skip this step.

If one small source follow-up was required, commit it separately:

```bash
rtk git add <exact changed files>
rtk git commit -m "fix: finalize wiiu gx2 presentation path"
```
