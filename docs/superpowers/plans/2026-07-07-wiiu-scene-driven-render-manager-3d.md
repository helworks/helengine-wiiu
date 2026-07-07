# Wii U Scene-Driven RenderManager3D Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the steady-state Wii U `cube_test` scene-cube shortcut with one scene-driven 3D capture path that renders real drawables and the active camera through GX2 while preserving the existing 2D overlay.

**Architecture:** Add one generic Wii U 3D frame contract, have `WiiURenderManager3D` override `Draw()` to capture the current scene into that frame by using the shared runtime extraction service plus runtime-model/world-matrix/camera state, and have `WiiUGx2Presenter` render the captured 3D frame first and the existing `WiiUGx2RenderFrame` overlay second. Reuse the existing flat-color scene-cube shader blob, but remove the presenter-owned scene-cube public path and feed it generic captured data instead.

**Tech Stack:** C++20, generated Helengine runtime C++ types, `RenderFrameExtractionService`, GX2/GX2R, WHB shader loading, xUnit source-contract tests, PowerShell build scripts, Cemu

## Global Constraints

- Keep the first slice limited to mesh + transform + camera + clear color + 2D overlay.
- Do not add a runtime toggle that keeps the old `ConfigureSceneCubeMesh` / `RenderSceneCubeFrame` path alive.
- Use the current scene-owned camera transform, viewport, near plane, far plane, and clear color.
- Until the generated runtime exposes authored field-of-view data, keep one fixed Wii U perspective FOV constant for this slice and document that assumption in code comments.
- Keep lighting out of scope. Pass an empty light list into extraction and ignore material appearance beyond the existing flat-color shader.
- Preserve source-audit-first workflow, then run the smallest real verification: focused tests, full `WiiURuntimeSourceTests`, Wii U build, Cemu launch.

---

## File Map

- Create: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx23DRenderFrame.hpp`
  Purpose: hold the generic captured 3D frame contract shared between `WiiURenderManager3D` and `WiiUGx2Presenter`.
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
  Purpose: lock the generic 3D capture/presentation seam and prove the steady-state code no longer depends on the public scene-cube shortcut.
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiURenderManager3D.hpp`
  Purpose: declare `Draw()`, `GetCurrentFrame()`, frame-capture helpers, and the current captured 3D frame state.
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiURenderManager3D.cpp`
  Purpose: capture the primary camera and scene drawables into the generic 3D frame and retain the existing runtime-model build path.
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
  Purpose: replace the public scene-cube entrypoints with a generic `RenderFrame` overload that accepts both 3D and 2D captured frames.
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
  Purpose: render one captured 3D frame into GX2 color buffers, then render the existing 2D overlay without clearing over the 3D pass.
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
  Purpose: remove startup mesh configuration and route steady-state presentation through captured 3D + 2D frames.

## Task 1: Lock The Scene-Driven 3D Source Contract

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\builder.tests\WiiURuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the new failing source-audit tests**

Add these tests near the current scene-cube source-contract tests:

```csharp
/// <summary>
/// Ensures the steady-state Wii U runtime presents one captured 3D frame plus the captured 2D overlay instead of calling the presenter-owned scene cube shortcut.
/// </summary>
[Fact]
public void RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
    string renderFrameHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));

    Assert.Contains("void RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D);", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("const WiiUGx23DRenderFrame& GetCurrentFrame() const;", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void Draw() override;", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("class WiiUGx23DRenderFrame {", renderFrameHeaderSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->RenderSceneCubeFrame();", applicationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->ConfigureSceneCubeMesh(", applicationSource, StringComparison.Ordinal);
}

/// <summary>
/// Ensures the Wii U 3D bridge captures one generic scene-driven frame from the active camera and drawable submissions instead of exposing only the latest runtime model shortcut.
/// </summary>
[Fact]
public void RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
    string renderFrameHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));

    Assert.Contains("RenderFrameExtractionService", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("WiiUGx23DRenderFrame CurrentFrame;", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void BeginFrame();", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void CaptureFrame(RenderFrame* frame, CameraComponent* camera);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("bool TryResolvePrimaryCamera(CameraComponent*& camera) const;", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("CurrentFrame.Clear();", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("CurrentFrame.SetCamera(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("CurrentFrame.AddDrawCommand(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("struct WiiUGx23DDrawCommand {", renderFrameHeaderSource, StringComparison.Ordinal);
    Assert.DoesNotContain("WiiURuntimeModel* GetLatestRuntimeModel() const;", renderManagerHeaderSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused tests and confirm they fail**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath|FullyQualifiedName~RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame" --no-restore -v minimal
```

Expected: FAIL because the generic 3D frame header, the new render-manager capture API, and the new presenter overload do not exist yet.

- [ ] **Step 3: Commit the red tests**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "test: lock Wii U scene-driven 3D frame contract"
```

## Task 2: Create The Generic Wii U 3D Frame Contract

**Files:**
- Create: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx23DRenderFrame.hpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Add the generic captured 3D frame types**

Create `src/platform/wiiu/WiiUGx23DRenderFrame.hpp` with this content:

```cpp
#pragma once

#include <vector>

#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wiiu/WiiUGx2RenderFrame.hpp"

namespace helengine::wiiu {
    class WiiURuntimeModel;

    /// Stores one captured Wii U camera state consumed by the GX2 presenter.
    struct WiiUGx23DCameraState {
        /// The world-to-view transform resolved from the active scene camera.
        float4x4 ViewMatrix;

        /// The viewport bounds requested by the active scene camera.
        float4 Viewport;

        /// The active scene camera near plane distance.
        float NearPlaneDistance;

        /// The active scene camera far plane distance.
        float FarPlaneDistance;
    };

    /// Stores one captured Wii U 3D draw command consumed by the GX2 presenter.
    struct WiiUGx23DDrawCommand {
        /// The runtime model resolved by the shared engine for this drawable submission.
        const WiiURuntimeModel* RuntimeModel;

        /// The world transform resolved from the drawable owner entity.
        float4x4 WorldMatrix;
    };

    /// Stores one full Wii U 3D frame captured from the generated-core scene state.
    class WiiUGx23DRenderFrame {
    public:
        /// Creates one empty captured 3D frame with an opaque black clear color.
        WiiUGx23DRenderFrame()
            : ClearColor { 0U, 0U, 0U, 255U }
            , HasCameraState(false)
            , CameraState()
            , DrawCommands() {
        }

        /// Resets the captured 3D frame to its default empty state.
        void Clear() {
            ClearColor = WiiUGx2Color { 0U, 0U, 0U, 255U };
            HasCameraState = false;
            CameraState = WiiUGx23DCameraState();
            DrawCommands.clear();
        }

        /// Stores the clear color that should be used before rendering 3D geometry.
        void SetClearColor(WiiUGx2Color color) {
            ClearColor = color;
        }

        /// Returns the clear color captured for the current frame.
        const WiiUGx2Color& GetClearColor() const {
            return ClearColor;
        }

        /// Stores the active camera state used for the current frame.
        void SetCamera(const WiiUGx23DCameraState& cameraState) {
            CameraState = cameraState;
            HasCameraState = true;
        }

        /// Returns whether the current frame captured one active camera.
        bool GetHasCamera() const {
            return HasCameraState;
        }

        /// Returns the captured camera state for the current frame.
        const WiiUGx23DCameraState& GetCamera() const {
            return CameraState;
        }

        /// Appends one draw command in render order.
        void AddDrawCommand(const WiiUGx23DDrawCommand& drawCommand) {
            DrawCommands.push_back(drawCommand);
        }

        /// Returns the captured draw commands in render order.
        const std::vector<WiiUGx23DDrawCommand>& GetDrawCommands() const {
            return DrawCommands;
        }

    private:
        /// Stores the clear color used before drawing 3D geometry.
        WiiUGx2Color ClearColor;

        /// Tracks whether one active camera state was captured.
        bool HasCameraState;

        /// Stores the active scene camera resolved for the current frame.
        WiiUGx23DCameraState CameraState;

        /// Stores captured 3D draw commands in render order.
        std::vector<WiiUGx23DDrawCommand> DrawCommands;
    };
}
```

- [ ] **Step 2: Run the focused tests again**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath|FullyQualifiedName~RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame" --no-restore -v minimal
```

Expected: FAIL, but now only on missing render-manager and presenter/application seams.

- [ ] **Step 3: Commit the 3D frame contract**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiUGx23DRenderFrame.hpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: add Wii U generic 3D frame contract"
```

## Task 3: Capture The Scene-Driven 3D Frame In `WiiURenderManager3D`

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiURenderManager3D.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiURenderManager3D.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Replace the latest-runtime-model presenter seam with a captured-frame seam**

Update `src/platform/wiiu/WiiURenderManager3D.hpp` so it declares the frame-capture API and removes `GetLatestRuntimeModel()`:

```cpp
#include "RenderManager3D.hpp"
#include "platform/wiiu/WiiUGx23DRenderFrame.hpp"

class CameraComponent;
class RenderFrame;
class RuntimeModel;

namespace helengine::wiiu {
    class WiiURuntimeModel;

    /// Provides the Wii U 3D renderer bridge that captures one generic scene-driven frame for the GX2 presenter.
    class WiiURenderManager3D final : public ::RenderManager3D {
    public:
        /// Creates one Wii U 3D bridge with an empty captured frame.
        WiiURenderManager3D();

        /// Releases cached bridge state.
        ~WiiURenderManager3D() override;

        /// Captures the current scene-driven 3D frame from the generated runtime.
        void Draw() override;

        /// Returns the most recently captured scene-driven 3D frame.
        const WiiUGx23DRenderFrame& GetCurrentFrame() const;

        /// Releases one runtime model built by the Wii U bridge.
        void ReleaseModel(::RuntimeModel* model) override;

    private:
        /// Resets the current frame before capture begins.
        void BeginFrame();

        /// Captures one extracted render frame into the Wii U frame contract.
        void CaptureFrame(RenderFrame* frame, CameraComponent* camera);

        /// Captures one extracted drawable submission into the current frame when its runtime model is Wii U-owned.
        void CaptureDrawCommand(IDrawable3D* drawable);

        /// Resolves the primary runtime camera for the current frame.
        bool TryResolvePrimaryCamera(CameraComponent*& camera) const;

        /// Converts one runtime camera clear color into the 8-bit GX2 color used by the presenter.
        static WiiUGx2Color ConvertClearColor(float4 clearColor);

        /// Converts one normalized float color channel into one 8-bit color channel.
        static std::uint8_t ConvertColorChannel(float value);

        /// Builds one view matrix from the active runtime camera transform.
        static float4x4 CreateViewMatrix(CameraComponent* camera);

        /// Builds one camera state record for the current frame.
        static WiiUGx23DCameraState CreateCameraState(CameraComponent* camera);

        /// Builds one Wii U runtime model from a shared model asset payload.
        WiiURuntimeModel* BuildRuntimeModelFromAsset(::ModelAsset* data);

        /// Releases one transient cooked model asset after the runtime geometry has been copied out.
        static void ReleaseTransientModelAsset(::ModelAsset* asset);

        /// Stores the most recently captured 3D frame.
        WiiUGx23DRenderFrame CurrentFrame;
    };
}
```

- [ ] **Step 2: Implement scene capture in `WiiURenderManager3D.cpp`**

Add the missing generated runtime includes near the top of `src/platform/wiiu/WiiURenderManager3D.cpp`:

```cpp
#include "CameraClearSettings.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "IDrawable3D.hpp"
#include "ObjectManager.hpp"
#include "RenderFrame.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameExtractionResult.hpp"
#include "RenderFrameExtractionService.hpp"
#include "runtime/native_list.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
```

Implement the new frame-capture path with this structure:

```cpp
WiiURenderManager3D::WiiURenderManager3D()
    : RenderManager3D()
    , CurrentFrame() {
}

void WiiURenderManager3D::Draw() {
    BeginFrame();

    CameraComponent* primaryCamera = nullptr;
    if (!TryResolvePrimaryCamera(primaryCamera)) {
        return;
    }

    Core* core = Core::get_Instance();
    if (core == nullptr || core->get_ObjectManager() == nullptr) {
        throw new InvalidOperationException("Wii U 3D frame capture requires one initialized Core object manager.");
    }

    ObjectManager* objectManager = core->get_ObjectManager();
    List<CameraComponent*>* cameras = new List<CameraComponent*>(1);
    cameras->Add(primaryCamera);

    List<LightComponent*>* lights = new List<LightComponent*>();
    RenderFrameExtractionService extractionService {};
    RenderFrameExtractionResult* extractionResult = extractionService.Extract(
        cameras,
        objectManager->get_Drawables3D(),
        lights,
        GetCapabilityProfile());

    if (extractionResult->get_Frames() == nullptr || extractionResult->get_Frames()->get_Count() == 0) {
        return;
    }

    CaptureFrame((*extractionResult->get_Frames()).get_Item(0), primaryCamera);
}

const WiiUGx23DRenderFrame& WiiURenderManager3D::GetCurrentFrame() const {
    return CurrentFrame;
}

void WiiURenderManager3D::BeginFrame() {
    CurrentFrame.Clear();
}

bool WiiURenderManager3D::TryResolvePrimaryCamera(CameraComponent*& camera) const {
    camera = nullptr;
    Core* core = Core::get_Instance();
    if (core == nullptr || core->get_ObjectManager() == nullptr || core->get_ObjectManager()->get_Cameras() == nullptr) {
        return false;
    }

    List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
    for (int32_t index = 0; index < cameras->get_Count(); index++) {
        CameraComponent* runtimeCamera = he_cpp_try_cast<CameraComponent>((*cameras).get_Item(index));
        if (runtimeCamera != nullptr && runtimeCamera->get_Parent() != nullptr) {
            camera = runtimeCamera;
            return true;
        }
    }

    return false;
}

void WiiURenderManager3D::CaptureFrame(RenderFrame* frame, CameraComponent* camera) {
    if (frame == nullptr) {
        throw new ArgumentNullException("frame");
    } else if (camera == nullptr) {
        throw new ArgumentNullException("camera");
    }

    CameraClearSettings clearSettings = camera->get_ClearSettings();
    CurrentFrame.SetClearColor(clearSettings.get_ClearColorEnabled()
        ? ConvertClearColor(clearSettings.get_ClearColor())
        : WiiUGx2Color { 0U, 0U, 0U, 255U });
    CurrentFrame.SetCamera(CreateCameraState(camera));

    List<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
    if (drawableSubmissions == nullptr) {
        return;
    }

    for (int32_t index = 0; index < drawableSubmissions->get_Count(); index++) {
        RenderFrameDrawableSubmission* submission = (*drawableSubmissions).get_Item(index);
        if (submission == nullptr || submission->get_Drawable() == nullptr) {
            continue;
        }

        CaptureDrawCommand(submission->get_Drawable());
    }
}

void WiiURenderManager3D::CaptureDrawCommand(IDrawable3D* drawable) {
    RuntimeModel* runtimeModel = drawable->get_Model();
    WiiURuntimeModel* wiiuRuntimeModel = he_cpp_try_cast<WiiURuntimeModel>(runtimeModel);
    if (wiiuRuntimeModel == nullptr) {
        throw new InvalidOperationException("Wii U 3D capture requires every runtime model to be one WiiURuntimeModel.");
    } else if (drawable->get_Parent() == nullptr) {
        throw new InvalidOperationException("Wii U 3D capture requires every drawable to have one parent entity.");
    }

    WiiUGx23DDrawCommand drawCommand {};
    drawCommand.RuntimeModel = wiiuRuntimeModel;
    drawCommand.WorldMatrix = drawable->get_Parent()->get_WorldTransformMatrix();
    CurrentFrame.AddDrawCommand(drawCommand);
}

std::uint8_t WiiURenderManager3D::ConvertColorChannel(float value) {
    if (value <= 0.0f) {
        return 0U;
    } else if (value >= 1.0f) {
        return 255U;
    }

    return static_cast<std::uint8_t>(value * 255.0f);
}

WiiUGx2Color WiiURenderManager3D::ConvertClearColor(float4 clearColor) {
    return WiiUGx2Color {
        ConvertColorChannel(clearColor.X),
        ConvertColorChannel(clearColor.Y),
        ConvertColorChannel(clearColor.Z),
        ConvertColorChannel(clearColor.W)
    };
}

float4x4 WiiURenderManager3D::CreateViewMatrix(CameraComponent* camera) {
    Entity* cameraEntity = camera->get_Parent();
    float3 cameraPosition = cameraEntity->get_Position();
    float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraEntity->get_Orientation());
    float3 cameraUp = float4::RotateVector(float3::get_UnitY(), cameraEntity->get_Orientation());
    float3 cameraTarget = cameraPosition + cameraForward;
    float4x4 viewMatrix;
    float4x4::CreateLookAt__ref0_ref1_ref2_out3(cameraPosition, cameraTarget, cameraUp, viewMatrix);
    return viewMatrix;
}

WiiUGx23DCameraState WiiURenderManager3D::CreateCameraState(CameraComponent* camera) {
    WiiUGx23DCameraState cameraState {};
    cameraState.ViewMatrix = CreateViewMatrix(camera);
    cameraState.Viewport = camera->get_Viewport();
    cameraState.NearPlaneDistance = camera->get_NearPlaneDistance();
    cameraState.FarPlaneDistance = camera->get_FarPlaneDistance();
    return cameraState;
}
```

Keep the existing cooked/raw model build logic, but remove the `LatestRuntimeModel` field, its getter, and the `ReleaseModel` branch that cleared it.

- [ ] **Step 3: Run the focused tests**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath|FullyQualifiedName~RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame" --no-restore -v minimal
```

Expected: FAIL only on the presenter/application strings that still reference the public scene-cube path.

- [ ] **Step 4: Commit the render-manager capture seam**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiURenderManager3D.hpp src/platform/wiiu/WiiURenderManager3D.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: capture Wii U scene-driven 3D frames"
```

## Task 4: Replace The Public Scene-Cube Presenter Path With Generic 3D + 2D Presentation

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.hpp`
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUGx2Presenter.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Replace the public presenter entrypoint**

Update `src/platform/wiiu/WiiUGx2Presenter.hpp` so the public steady-state 3D seam becomes:

```cpp
#include "platform/wiiu/WiiUGx23DRenderFrame.hpp"

/// Renders and presents one captured Wii U 3D frame plus the captured 2D overlay.
void RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D);
```

Remove these public declarations:

```cpp
void ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel);
void RenderSceneCubeFrame();
```

Rename the old scene-cube private helpers and fields so they describe generic flat-color mesh presentation, using these exact names:

```cpp
void InitializeFlatColorMeshResources();
void DestroyFlatColorMeshResources();
void EnsureFlatColorMeshVertexBuffer(const float* sourceData, std::uint32_t floatCount);
void EnsureFlatColorMeshIndexBuffer(const std::uint16_t* sourceData, std::uint32_t indexCount);
void InitializeFlatColorMeshTransformBuffer();
void Render3DFrameToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer, const WiiUGx23DRenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight);
void Render3DDrawCommandToColorBuffer(const WiiUGx23DDrawCommand& drawCommand, const WiiUGx23DCameraState& cameraState, std::uint32_t targetWidth, std::uint32_t targetHeight);
void RenderQuadCommandsToColorBuffer(const WiiUGx2RenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight);

bool AreFlatColorMeshResourcesInitialized;
WHBGfxShaderGroup FlatColorMeshShaderGroup;
GX2RBuffer FlatColorMeshPositionBuffer;
GX2RBuffer FlatColorMeshIndexBuffer;
GX2RBuffer FlatColorMeshTransformBuffer;
std::uint32_t FlatColorMeshIndexCount;
```

- [ ] **Step 2: Implement generic 3D presentation and preserve the 2D overlay**

Refactor `src/platform/wiiu/WiiUGx2Presenter.cpp` with this structure:

```cpp
namespace {
    constexpr double SceneDrivenFieldOfViewRadians = 1.0;
}

void WiiUGx2Presenter::RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D) {
    if (!IsInitialized) {
        throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderFrame.");
    }

    Render3DFrameToColorBuffer(TvContextState, &TvColorBuffer, frame3D, TvSurfaceWidth, TvSurfaceHeight);
    RenderQuadCommandsToColorBuffer(frame2D, TvSurfaceWidth, TvSurfaceHeight);

    Render3DFrameToColorBuffer(DrcContextState, &DrcColorBuffer, frame3D, DrcSurfaceWidth, DrcSurfaceHeight);
    RenderQuadCommandsToColorBuffer(frame2D, DrcSurfaceWidth, DrcSurfaceHeight);

    PresentScanBuffers();
}

void WiiUGx2Presenter::Render3DFrameToColorBuffer(
    GX2ContextState* contextState,
    GX2ColorBuffer* colorBuffer,
    const WiiUGx23DRenderFrame& frame,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight) {
    if (!frame.GetHasCamera()) {
        GX2SetContextState(contextState);
        const WiiUGx2Color& clearColor = frame.GetClearColor();
        GX2ClearColor(
            colorBuffer,
            static_cast<float>(clearColor.Red) / 255.0f,
            static_cast<float>(clearColor.Green) / 255.0f,
            static_cast<float>(clearColor.Blue) / 255.0f,
            static_cast<float>(clearColor.Alpha) / 255.0f);
        return;
    }

    GX2SetContextState(contextState);
    const WiiUGx2Color& clearColor = frame.GetClearColor();
    GX2ClearColor(
        colorBuffer,
        static_cast<float>(clearColor.Red) / 255.0f,
        static_cast<float>(clearColor.Green) / 255.0f,
        static_cast<float>(clearColor.Blue) / 255.0f,
        static_cast<float>(clearColor.Alpha) / 255.0f);

    const WiiUGx23DCameraState& cameraState = frame.GetCamera();
    const std::vector<WiiUGx23DDrawCommand>& drawCommands = frame.GetDrawCommands();
    for (std::size_t index = 0; index < drawCommands.size(); index++) {
        Render3DDrawCommandToColorBuffer(drawCommands[index], cameraState, targetWidth, targetHeight);
    }
}

void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer(
    const WiiUGx23DDrawCommand& drawCommand,
    const WiiUGx23DCameraState& cameraState,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight) {
    if (drawCommand.RuntimeModel == nullptr) {
        throw std::runtime_error("Wii U 3D draw command requires one runtime model.");
    }

    float4x4 projectionMatrix;
    float4x4::CreatePerspectiveFieldOfView__out4(
        static_cast<float>(SceneDrivenFieldOfViewRadians),
        static_cast<float>(static_cast<double>(targetWidth) / static_cast<double>(targetHeight)),
        cameraState.NearPlaneDistance,
        cameraState.FarPlaneDistance,
        projectionMatrix);

    float4x4 worldViewMatrix;
    float4x4::Multiply__ref0_ref1_out2(drawCommand.WorldMatrix, cameraState.ViewMatrix, worldViewMatrix);
    float4x4 worldViewProjectionMatrix;
    float4x4::Multiply__ref0_ref1_out2(worldViewMatrix, projectionMatrix, worldViewProjectionMatrix);

    const std::vector<float>& positionData = drawCommand.RuntimeModel->GetPositionData();
    const std::vector<std::uint16_t>& indexData = drawCommand.RuntimeModel->GetIndexData();
    std::vector<float> clipSpacePositionData;
    clipSpacePositionData.reserve(positionData.size());
    for (std::size_t vertexIndex = 0; vertexIndex < positionData.size(); vertexIndex += 4U) {
        const float x = positionData[vertexIndex + 0U];
        const float y = positionData[vertexIndex + 1U];
        const float z = positionData[vertexIndex + 2U];
        const float w = positionData[vertexIndex + 3U];

        clipSpacePositionData.push_back((x * worldViewProjectionMatrix.M11) + (y * worldViewProjectionMatrix.M21) + (z * worldViewProjectionMatrix.M31) + (w * worldViewProjectionMatrix.M41));
        clipSpacePositionData.push_back((x * worldViewProjectionMatrix.M12) + (y * worldViewProjectionMatrix.M22) + (z * worldViewProjectionMatrix.M32) + (w * worldViewProjectionMatrix.M42));
        clipSpacePositionData.push_back((x * worldViewProjectionMatrix.M13) + (y * worldViewProjectionMatrix.M23) + (z * worldViewProjectionMatrix.M33) + (w * worldViewProjectionMatrix.M43));
        clipSpacePositionData.push_back((x * worldViewProjectionMatrix.M14) + (y * worldViewProjectionMatrix.M24) + (z * worldViewProjectionMatrix.M34) + (w * worldViewProjectionMatrix.M44));
    }

    EnsureFlatColorMeshVertexBuffer(clipSpacePositionData.data(), static_cast<std::uint32_t>(clipSpacePositionData.size()));
    EnsureFlatColorMeshIndexBuffer(indexData.data(), static_cast<std::uint32_t>(indexData.size()));
    FlatColorMeshIndexCount = static_cast<std::uint32_t>(indexData.size());

    GX2SetFetchShader(&FlatColorMeshShaderGroup.fetchShader);
    GX2SetVertexShader(FlatColorMeshShaderGroup.vertexShader);
    GX2SetPixelShader(FlatColorMeshShaderGroup.pixelShader);
    GX2RSetVertexUniformBlock(&FlatColorMeshTransformBuffer, 0, 0);
    GX2RSetAttributeBuffer(&FlatColorMeshPositionBuffer, 0, FlatColorMeshPositionBuffer.elemSize, 0);
    GX2RSetIndexBuffer(&FlatColorMeshIndexBuffer, GX2_INDEX_TYPE_U16, FlatColorMeshIndexCount * sizeof(std::uint16_t));
    GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, FlatColorMeshIndexCount, GX2_INDEX_TYPE_U16, nullptr, 0, 1);
}
```

Keep the existing flat-color mesh shader blob, but rename the internal resource owner from `SceneCube` to generic `FlatColorMesh`. The transform buffer content should become identity once, because CPU-side clip-space expansion now happens per draw command:

```cpp
const float IdentityTransformData[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
```

Also split the current 2D `RenderFrameToColorBuffer(...)` behavior so the combined 3D + 2D path does not clear again before rendering UI. Keep the current 2D-only method available if useful, but move the quad loop into `RenderQuadCommandsToColorBuffer(...)`.

- [ ] **Step 3: Run the focused tests**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath|FullyQualifiedName~RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame" --no-restore -v minimal
```

Expected: FAIL only on the remaining application source still wiring the old scene-cube startup/present calls.

- [ ] **Step 4: Commit the presenter refactor**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: present Wii U captured 3D and 2D frames"
```

## Task 5: Route `WiiUApplication` Through The Captured 3D + 2D Frames

**Files:**
- Modify: `C:\dev\helworks\helengine-wiiu\src\platform\wiiu\WiiUApplication.cpp`
- Test: `C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Remove the startup scene-cube configuration**

Delete this startup-only block from `InitializeEngineCore()`:

```cpp
initializationStage = "ConfigureSceneCubeMesh";
WiiURuntimeModel* latestRuntimeModel = EngineRenderManager3D->GetLatestRuntimeModel();
if (latestRuntimeModel == nullptr) {
    throw new InvalidOperationException("Wii U cube_test bring-up requires one runtime model to be built during scene load.");
}

Gx2Presenter->ConfigureSceneCubeMesh(*EngineRenderManager3D->GetLatestRuntimeModel());
AppendRuntimeTrace("[WiiUFile] Scene cube mesh configured from latest runtime model.\n");
```

Also remove this include:

```cpp
#include "platform/wiiu/WiiURuntimeModel.hpp"
```

- [ ] **Step 2: Route steady-state presentation through the new frame capture seam**

Change `PresentRenderedFrame()` to:

```cpp
void WiiUApplication::PresentRenderedFrame() {
    if (Gx2Presenter == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter must exist before rendered presentation can begin.");
    } else if (EngineRenderManager3D == nullptr) {
        throw std::runtime_error("Wii U 3D render manager must exist before rendered presentation can begin.");
    } else if (EngineRenderManager2D == nullptr) {
        throw std::runtime_error("Wii U 2D render manager must exist before rendered presentation can begin.");
    }

    Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());
}
```

Keep the existing warm update + warm draw sequence untouched so deferred scene loading still commits before the steady-state loop begins.

- [ ] **Step 3: Run the focused tests and then the full Wii U source-contract suite**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath|FullyQualifiedName~RuntimeSeam_CapturesPrimaryCameraAndDrawableSubmissionsIntoGenericWiiU3dFrame" --no-restore -v minimal
```

Expected: PASS.

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~WiiURuntimeSourceTests --no-restore -v minimal
```

Expected: PASS, including updates to the older scene-cube tests that should now be rewritten or removed as part of this task.

- [ ] **Step 4: Commit the application routing**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add src/platform/wiiu/WiiUApplication.cpp builder.tests/WiiURuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: route Wii U steady-state rendering through captured scene frames"
```

## Task 6: Build And Verify In Cemu

**Files:**
- Verify: `C:\dev\helprojs\city\project.heproj`
- Verify: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`

- [ ] **Step 1: Build the Wii U artifact**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\city\wiiu-build
```

Expected: PASS and refreshed timestamps for `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb` and `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.rpx`.

- [ ] **Step 2: Launch the build in Cemu**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

Expected: Cemu boots into the packaged startup scene, the loop stays alive, the scene-owned 3D mesh renders, and the existing 2D overlay remains visible on top.

- [ ] **Step 3: Validate the runtime trace advances**

Run:

```powershell
rtk read C:\dev\helworks\helengine-wiiu\wiiu_runtime_trace.txt
```

Expected: the trace contains multiple steady-state update/draw entries after the initial warm draw, and does not contain the old `Scene cube mesh configured from latest runtime model.` line.

- [ ] **Step 4: If Cemu shows geometry but the framing is wrong, debug in this order**

Use this checklist exactly:

```text
1. Confirm `WiiURenderManager3D::TryResolvePrimaryCamera(...)` is choosing the expected camera by logging its parent entity position.
2. Confirm `CreateViewMatrix(...)` uses authored forward `float3(0.0f, 0.0f, -1.0f)` and rotated `float3::get_UnitY()` for the up vector.
3. Print the first transformed clip-space vertex from `Render3DDrawCommandToColorBuffer(...)` before upload.
4. Temporarily force `SceneDrivenFieldOfViewRadians` to the previously working value if the camera feels too zoomed in or out.
5. Temporarily skip the 2D overlay render to confirm the 3D pass is not being overwritten.
```

- [ ] **Step 5: Commit the verified slice**

Run:

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu status --short
rtk git -C C:\dev\helworks\helengine-wiiu add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx23DRenderFrame.hpp src/platform/wiiu/WiiURenderManager3D.hpp src/platform/wiiu/WiiURenderManager3D.cpp src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp src/platform/wiiu/WiiUApplication.cpp
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: render scene-driven 3D frames on Wii U"
```

Expected: only the files from this plan are staged for the final commit.
