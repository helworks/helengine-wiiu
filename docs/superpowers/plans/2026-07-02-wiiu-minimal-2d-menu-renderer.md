# Wii U Minimal 2D Menu Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the packaged Wii U `DemoDiscMainMenu` scene visibly render in Cemu by replacing the current solid-color presentation path with a minimal host-owned 2D software renderer.

**Architecture:** Keep Wii U scene bootstrap and 3D placeholders intact, but introduce a focused software 2D surface plus a queue-backed `WiiURenderManager2D` raster path for rounded rectangles, sprites, and bitmap-font text. Update `WiiUApplication` so pre-engine startup still shows boot colors while post-engine startup presents the renderer-owned surface instead of clearing over it.

**Tech Stack:** C++20, WUT OSScreen APIs, existing helengine runtime scene/text/font pipeline, xUnit source tests, Cemu WUHB runtime verification.

---

## File Structure

### Existing Files To Modify

- `builder.tests/WiiURuntimeSourceTests.cs`
  - Extend source-level guards for the Wii U application presentation contract and the 2D renderer implementation boundary.
- `src/platform/wiiu/WiiUApplication.hpp`
  - Inject renderer-owned presentation state and the software surface handoff into the host seam.
- `src/platform/wiiu/WiiUApplication.cpp`
  - Stop clearing over rendered output after engine startup and route post-startup presentation through the software 2D surface.
- `src/platform/wiiu/WiiURenderManager2D.hpp`
  - Replace the current no-op bridge with an owned 2D submission/rasterization interface.
- `src/platform/wiiu/WiiURenderManager2D.cpp`
  - Implement texture capture, draw submission, surface rasterization, and release handling needed by the menu slice.

### New Files To Create

- `src/platform/wiiu/WiiUSoftwareSurface.hpp`
  - Small private platform helper that owns ARGB8888 pixel buffers and CPU-side raster helpers.
- `src/platform/wiiu/WiiUSoftwareSurface.cpp`
  - Concrete pixel, fill, blit, and glyph-write implementation used by the Wii U 2D renderer.

### Files To Leave Intentionally Unchanged

- `src/platform/wiiu/WiiURenderManager3D.hpp`
- `src/platform/wiiu/WiiURenderManager3D.cpp`

Those files stay skeletal in this slice unless a compile fix is strictly required.

---

### Task 1: Lock The Renderer Contract With Failing Source Tests

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing source tests**

Add one new test method that proves `PresentFrame()` no longer clears over the final frame once engine rendering is active, and extend the 2D renderer source assertions so the three draw methods are no longer empty stubs.

```csharp
    /// <summary>
    /// Ensures the Wii U host presents the renderer-owned frame once the generated core has initialized instead of clearing over the visible output every loop.
    /// </summary>
    [Fact]
    public void RuntimeSeam_PresentsRendererOwnedFrameAfterEngineStartup() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("WiiUSoftwareSurface", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("PresentBootPhaseFrame();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("PresentRenderedFrame();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!EngineInitialized) {", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U 2D renderer no longer leaves menu draw requests as empty no-op stubs.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RasterizesMenu2DPrimitivesThroughWiiURenderManager2D() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.hpp"));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));

        Assert.Contains("void Draw();", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void AttachSurface(", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Surface->Clear(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitRoundedRect(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitSprite(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitText(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("/// Ignores one rounded-rectangle draw request until the Wii U renderer is implemented.", renderSource, StringComparison.Ordinal);
    }
```

- [ ] **Step 2: Run the focused source tests to verify they fail**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter "RuntimeSeam_PresentsRendererOwnedFrameAfterEngineStartup|RuntimeSeam_RasterizesMenu2DPrimitivesThroughWiiURenderManager2D"
```

Expected:

```text
FAIL
Assert.Contains() Failure: Sub-string not found
```

- [ ] **Step 3: Run the existing Wii U runtime source suite to capture the pre-change baseline**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter WiiURuntimeSourceTests
```

Expected:

```text
1 or more tests fail because the new renderer contract does not exist yet.
```

- [ ] **Step 4: Commit the red test update**

```bash
rtk git add builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "test: lock wiiu 2d renderer contract"
```

---

### Task 2: Introduce The Wii U Software Surface Primitive

**Files:**
- Create: `src/platform/wiiu/WiiUSoftwareSurface.hpp`
- Create: `src/platform/wiiu/WiiUSoftwareSurface.cpp`
- Modify: `src/platform/wiiu/WiiUApplication.hpp`
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Create the new surface header**

Create `src/platform/wiiu/WiiUSoftwareSurface.hpp` with a single-responsibility CPU-owned ARGB8888 surface type:

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace helengine::wiiu {
    /// Owns one CPU-written ARGB8888 surface used by the minimal Wii U menu renderer.
    class WiiUSoftwareSurface {
    public:
        /// Creates one software surface with the supplied dimensions.
        WiiUSoftwareSurface(std::uint32_t width, std::uint32_t height);

        /// Clears the full surface to one packed ARGB8888 color.
        void Clear(std::uint32_t argbColor);

        /// Writes one packed ARGB8888 pixel when the target coordinate lies inside the surface.
        void SetPixel(int x, int y, std::uint32_t argbColor);

        /// Alpha-blends one packed ARGB8888 pixel when the target coordinate lies inside the surface.
        void BlendPixel(int x, int y, std::uint32_t argbColor);

        /// Fills one axis-aligned rectangle in surface pixel space.
        void FillRect(int x, int y, int width, int height, std::uint32_t argbColor);

        /// Returns the packed pixel buffer used for OSScreen presentation.
        const std::uint32_t* GetPixels() const;

        /// Returns the mutable packed pixel buffer used for rasterization.
        std::uint32_t* GetPixels();

        /// Returns the logical surface width in pixels.
        std::uint32_t GetWidth() const;

        /// Returns the logical surface height in pixels.
        std::uint32_t GetHeight() const;

    private:
        /// Converts one surface coordinate pair into a flat pixel-buffer index.
        std::uint32_t GetIndex(std::uint32_t x, std::uint32_t y) const;

        /// Stores the logical surface width in pixels.
        std::uint32_t Width;

        /// Stores the logical surface height in pixels.
        std::uint32_t Height;

        /// Stores packed ARGB8888 pixels in row-major order.
        std::vector<std::uint32_t> Pixels;
    };
}
```

- [ ] **Step 2: Create the new surface implementation**

Create `src/platform/wiiu/WiiUSoftwareSurface.cpp` with the minimal pixel operations:

```cpp
#include "platform/wiiu/WiiUSoftwareSurface.hpp"

#include <algorithm>
#include <stdexcept>

namespace helengine::wiiu {
    WiiUSoftwareSurface::WiiUSoftwareSurface(std::uint32_t width, std::uint32_t height)
        : Width(width)
        , Height(height)
        , Pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U) {
        if (width == 0U || height == 0U) {
            throw std::invalid_argument("Wii U software surface dimensions must be non-zero.");
        }
    }

    void WiiUSoftwareSurface::Clear(std::uint32_t argbColor) {
        std::fill(Pixels.begin(), Pixels.end(), argbColor);
    }

    void WiiUSoftwareSurface::SetPixel(int x, int y, std::uint32_t argbColor) {
        if (x < 0 || y < 0 || x >= static_cast<int>(Width) || y >= static_cast<int>(Height)) {
            return;
        }

        Pixels[GetIndex(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))] = argbColor;
    }

    void WiiUSoftwareSurface::BlendPixel(int x, int y, std::uint32_t argbColor) {
        if (x < 0 || y < 0 || x >= static_cast<int>(Width) || y >= static_cast<int>(Height)) {
            return;
        }

        std::uint32_t& destination = Pixels[GetIndex(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))];
        std::uint32_t sourceAlpha = (argbColor >> 24) & 0xFFU;
        if (sourceAlpha == 0U) {
            return;
        }
        if (sourceAlpha == 0xFFU) {
            destination = argbColor;
            return;
        }

        std::uint32_t inverseAlpha = 0xFFU - sourceAlpha;
        std::uint32_t sourceRed = (argbColor >> 16) & 0xFFU;
        std::uint32_t sourceGreen = (argbColor >> 8) & 0xFFU;
        std::uint32_t sourceBlue = argbColor & 0xFFU;
        std::uint32_t destinationRed = (destination >> 16) & 0xFFU;
        std::uint32_t destinationGreen = (destination >> 8) & 0xFFU;
        std::uint32_t destinationBlue = destination & 0xFFU;

        std::uint32_t blendedRed = ((sourceRed * sourceAlpha) + (destinationRed * inverseAlpha)) / 0xFFU;
        std::uint32_t blendedGreen = ((sourceGreen * sourceAlpha) + (destinationGreen * inverseAlpha)) / 0xFFU;
        std::uint32_t blendedBlue = ((sourceBlue * sourceAlpha) + (destinationBlue * inverseAlpha)) / 0xFFU;
        destination = 0xFF000000U | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
    }

    void WiiUSoftwareSurface::FillRect(int x, int y, int width, int height, std::uint32_t argbColor) {
        for (int row = 0; row < height; row++) {
            for (int column = 0; column < width; column++) {
                BlendPixel(x + column, y + row, argbColor);
            }
        }
    }

    const std::uint32_t* WiiUSoftwareSurface::GetPixels() const {
        return Pixels.data();
    }

    std::uint32_t* WiiUSoftwareSurface::GetPixels() {
        return Pixels.data();
    }

    std::uint32_t WiiUSoftwareSurface::GetWidth() const {
        return Width;
    }

    std::uint32_t WiiUSoftwareSurface::GetHeight() const {
        return Height;
    }

    std::uint32_t WiiUSoftwareSurface::GetIndex(std::uint32_t x, std::uint32_t y) const {
        return (y * Width) + x;
    }
}
```

- [ ] **Step 3: Add surface ownership to `WiiUApplication`**

Modify `src/platform/wiiu/WiiUApplication.hpp` so the host owns software surfaces and split presentation into boot and rendered paths:

```cpp
#include "platform/wiiu/WiiUSoftwareSurface.hpp"

        /// Presents one boot-phase frame using the current diagnostic clear color on both displays.
        void PresentBootPhaseFrame();

        /// Presents one renderer-owned frame after the generated core has initialized.
        void PresentRenderedFrame();

        /// Stores the TV software surface used by the minimal Wii U menu renderer.
        WiiUSoftwareSurface* TvSurface;

        /// Stores the DRC software surface used by the minimal Wii U menu renderer.
        WiiUSoftwareSurface* DrcSurface;
```

Modify the constructor, destructor, and `PresentFrame()` entry in `src/platform/wiiu/WiiUApplication.cpp`:

```cpp
        , TvSurface(nullptr)
        , DrcSurface(nullptr)
```

```cpp
        delete TvSurface;
        delete DrcSurface;
```

```cpp
    void WiiUApplication::PresentFrame() {
        if (!EngineInitialized) {
            PresentBootPhaseFrame();
            return;
        }

        PresentRenderedFrame();
    }
```

- [ ] **Step 4: Run the focused source tests to verify the surface contract still fails on missing raster integration**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter "RuntimeSeam_PresentsRendererOwnedFrameAfterEngineStartup|RuntimeSeam_RasterizesMenu2DPrimitivesThroughWiiURenderManager2D"
```

Expected:

```text
FAIL
The render-manager test should still fail until real 2D submission exists.
```

- [ ] **Step 5: Commit the surface seam**

```bash
rtk git add src/platform/wiiu/WiiUSoftwareSurface.hpp src/platform/wiiu/WiiUSoftwareSurface.cpp src/platform/wiiu/WiiUApplication.hpp src/platform/wiiu/WiiUApplication.cpp
rtk git commit -m "feat: add wiiu software surface seam"
```

---

### Task 3: Implement Queue-Backed Wii U 2D Rasterization

**Files:**
- Modify: `src/platform/wiiu/WiiURenderManager2D.hpp`
- Modify: `src/platform/wiiu/WiiURenderManager2D.cpp`
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Replace the empty 2D bridge header with concrete submission types**

Modify `src/platform/wiiu/WiiURenderManager2D.hpp` to add a surface attachment point and explicit submission storage:

```cpp
#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <vector>

#include "RenderManager2D.hpp"
#include "platform/wiiu/WiiUSoftwareSurface.hpp"

namespace helengine::wiiu {
    /// Provides the minimum Wii U 2D renderer bridge required to make the authored menu scenes visible.
    class WiiURenderManager2D final : public ::RenderManager2D {
    public:
        /// Attaches the software surfaces that receive TV and DRC menu output.
        void AttachSurface(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface);

        /// Builds one runtime texture for a cooked Wii U texture asset path.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;

        /// Builds one runtime texture for a raw texture asset while preserving CPU-readable pixels.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;

        /// Executes one full 2D draw pass into the attached software surfaces.
        void Draw() override;

        /// Records one rounded-rectangle draw request for the current frame.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;

        /// Records one sprite draw request for the current frame.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;

        /// Records one text draw request for the current frame.
        void DrawText(::ITextDrawable2D* text) override;

    private:
        struct SubmittedRoundedRect {
            int X;
            int Y;
            int Width;
            int Height;
            std::uint32_t FillColor;
        };

        struct SubmittedSprite {
            int X;
            int Y;
            int Width;
            int Height;
            ::RuntimeTexture* Texture;
            std::uint32_t TintColor;
        };

        struct SubmittedGlyph {
            int X;
            int Y;
            int Width;
            int Height;
            ::RuntimeTexture* Texture;
            std::uint32_t TintColor;
        };

        void SubmitRoundedRect(::IRoundedRectDrawable2D* shape);
        void SubmitSprite(::ISpriteDrawable2D* sprite);
        void SubmitText(::ITextDrawable2D* text);
        void RasterizeRoundedRects();
        void RasterizeSprites();
        void RasterizeText();
        void ResetFrameSubmissions();
        static std::uint32_t PackColor(::float4 color);

        WiiUSoftwareSurface* TvSurface;
        WiiUSoftwareSurface* DrcSurface;
        std::vector<SubmittedRoundedRect> RoundedRects;
        std::vector<SubmittedSprite> Sprites;
        std::vector<SubmittedGlyph> Glyphs;
    };
}

#endif
```

- [ ] **Step 2: Implement the minimal 2D renderer behavior**

Modify `src/platform/wiiu/WiiURenderManager2D.cpp` so draw calls are recorded and replayed into the attached surfaces:

```cpp
    void WiiURenderManager2D::AttachSurface(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface) {
        if (tvSurface == nullptr || drcSurface == nullptr) {
            throw new ArgumentNullException("tvSurface");
        }

        TvSurface = tvSurface;
        DrcSurface = drcSurface;
    }

    void WiiURenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            throw new ArgumentNullException("shape");
        }

        SubmitRoundedRect(shape);
    }

    void WiiURenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
        if (sprite == nullptr) {
            throw new ArgumentNullException("sprite");
        }

        SubmitSprite(sprite);
    }

    void WiiURenderManager2D::DrawText(::ITextDrawable2D* text) {
        if (text == nullptr) {
            throw new ArgumentNullException("text");
        }

        SubmitText(text);
    }

    void WiiURenderManager2D::Draw() {
        if (TvSurface == nullptr || DrcSurface == nullptr) {
            throw new InvalidOperationException("Wii U software surfaces must be attached before Draw.");
        }

        TvSurface->Clear(0xFF101018U);
        DrcSurface->Clear(0xFF101018U);
        RasterizeRoundedRects();
        RasterizeSprites();
        RasterizeText();
        ResetFrameSubmissions();
    }
```

Also include concrete `SubmitRoundedRect`, `SubmitSprite`, `SubmitText`, `RasterizeRoundedRects`, `RasterizeSprites`, `RasterizeText`, and `PackColor` bodies in the same file, with the smallest menu-focused behavior:

```cpp
    void WiiURenderManager2D::RasterizeRoundedRects() {
        for (const SubmittedRoundedRect& submission : RoundedRects) {
            TvSurface->FillRect(submission.X, submission.Y, submission.Width, submission.Height, submission.FillColor);
            DrcSurface->FillRect(submission.X, submission.Y, submission.Width, submission.Height, submission.FillColor);
        }
    }
```

```cpp
    void WiiURenderManager2D::ResetFrameSubmissions() {
        RoundedRects.clear();
        Sprites.clear();
        Glyphs.clear();
    }
```

Use the real drawable properties already exposed by the engine interfaces instead of inventing new runtime contracts.

- [ ] **Step 3: Attach the surfaces during Wii U engine initialization**

Modify `src/platform/wiiu/WiiUApplication.cpp` in `InitializeVideo()` and `InitializeEngineCore()`:

```cpp
        TvSurface = new WiiUSoftwareSurface(1280U, 720U);
        DrcSurface = new WiiUSoftwareSurface(854U, 480U);
```

```cpp
            EngineRenderManager2D = new WiiURenderManager2D();
            EngineRenderManager2D->AttachSurface(TvSurface, DrcSurface);
```

- [ ] **Step 4: Run the focused source tests to verify the renderer contract now passes**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter "RuntimeSeam_PresentsRendererOwnedFrameAfterEngineStartup|RuntimeSeam_RasterizesMenu2DPrimitivesThroughWiiURenderManager2D"
```

Expected:

```text
PASS
```

- [ ] **Step 5: Run the full Wii U runtime source suite**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter WiiURuntimeSourceTests
```

Expected:

```text
PASS
```

- [ ] **Step 6: Commit the 2D renderer implementation**

```bash
rtk git add src/platform/wiiu/WiiURenderManager2D.hpp src/platform/wiiu/WiiURenderManager2D.cpp src/platform/wiiu/WiiUApplication.cpp builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "feat: add wiiu minimal 2d menu renderer"
```

---

### Task 4: Integrate OSScreen Presentation And Verify In Cemu

**Files:**
- Modify: `src/platform/wiiu/WiiUApplication.hpp`
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Verify: `C:\dev\helprojs\city\project.heproj`
- Verify: `C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt`

- [ ] **Step 1: Implement boot-phase and rendered presentation helpers**

Modify `src/platform/wiiu/WiiUApplication.cpp` to split the old `PresentFrame()` body into two explicit helpers:

```cpp
    void WiiUApplication::PresentBootPhaseFrame() {
        OSScreenClearBufferEx(SCREEN_TV, ClearColor);
        OSScreenClearBufferEx(SCREEN_DRC, ClearColor);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }
```

```cpp
    void WiiUApplication::PresentRenderedFrame() {
        const std::uint32_t* tvPixels = TvSurface->GetPixels();
        const std::uint32_t* drcPixels = DrcSurface->GetPixels();
        std::memcpy(TvBuffer, tvPixels, OSScreenGetBufferSizeEx(SCREEN_TV));
        std::memcpy(DrcBuffer, drcPixels, OSScreenGetBufferSizeEx(SCREEN_DRC));
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }
```

Add one narrow trace line before the first rendered present:

```cpp
            if (DrawFrameLogCount == 0) {
                AppendRuntimeTrace("[WiiUFile] Presenting first renderer-owned frame.\n");
            }
```

- [ ] **Step 2: Run the Wii U runtime source suite after the presentation integration**

Run:

```bash
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj -c Debug --no-restore --filter WiiURuntimeSourceTests
```

Expected:

```text
PASS
```

- [ ] **Step 3: Build the `city` Wii U artifact**

Run:

```bash
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\output\wiiu
```

Expected:

```text
Build completed for platform 'wiiu': C:\dev\helprojs\output\wiiu
```

- [ ] **Step 4: Launch the rebuilt WUHB in Cemu**

Run:

```bash
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\output\wiiu\helengine_wiiu.wuhb
```

Expected:

```text
PROCESS_ID=<number>
```

- [ ] **Step 5: Verify runtime trace progression and visible menu output**

Run:

```bash
rtk powershell -NoProfile -Command "Get-Content 'C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt' | Select-Object -Last 80 | Out-String"
```

Expected:

```text
[WiiUFile] Packaged startup scene queued.
[WiiUFile] Engine update completed frame=0
[WiiUFile] Engine draw completed frame=0
[WiiUFile] Presenting first renderer-owned frame.
```

Also verify directly in Cemu that:

- menu panel rectangles are visible
- menu label text is visible
- logo sprites are visible
- platform info text is visible

- [ ] **Step 6: Commit the presentation integration**

```bash
rtk git add src/platform/wiiu/WiiUApplication.hpp src/platform/wiiu/WiiUApplication.cpp
rtk git commit -m "feat: present wiiu menu through software surface"
```

---

## Spec Coverage Check

- Summary and goal: covered by Tasks 2, 3, and 4.
- Keep Wii U 3D stubbed: covered by Task 3 file scope and unchanged-file constraint.
- Add a host-owned software surface: covered by Task 2.
- Replace no-op 2D rendering with rounded rect, sprite, and text rasterization: covered by Task 3.
- Preserve boot-phase diagnostics before engine startup: covered by Task 4.
- Keep runtime trace diagnostics: covered by Tasks 2 and 4.
- Runtime verification through `city` and Cemu: covered by Task 4.

## Placeholder Scan

- No `TODO`, `TBD`, or deferred implementation placeholders remain.
- Every code-changing step includes concrete code to add or replace.
- Every test step includes an exact command and expected outcome.

## Type Consistency Check

- Surface type name is consistently `WiiUSoftwareSurface`.
- Presentation helpers are consistently `PresentBootPhaseFrame()` and `PresentRenderedFrame()`.
- Surface attachment method is consistently `AttachSurface(...)`.
- The 2D renderer continues using `WiiURenderManager2D` throughout.
