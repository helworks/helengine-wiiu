# Wii U `cube_test` First Visible Cube Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the single cube from loaded `cube_test` runtime data as one stable flat-colored GX2 mesh on Wii U.

**Architecture:** Keep the application on the controlled bring-up loop, but replace the translated triangle with a presenter-owned scene-cube path. Capture the latest runtime model inside `WiiURenderManager3D`, expose its positions and indices through `WiiURuntimeModel`, upload that geometry once into `WiiUGx2Presenter`, and render it with a simple fixed transform and flat-color shader.

**Tech Stack:** C++20, generated Helengine runtime C++ types, GX2/GX2R, WHB GFD shader loading, CafeGLSL shader compilation, xUnit source-contract tests, PowerShell build scripts, Cemu

---

## File Map

- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
  Purpose: lock the scene-cube presenter seam, runtime-model geometry seam, and application routing contract.
- Modify: `src/platform/wiiu/WiiURuntimeModel.hpp`
  Purpose: store copied mesh positions and indices for the first visible cube slice.
- Modify: `src/platform/wiiu/WiiURuntimeModel.cpp`
  Purpose: implement the geometry setter and accessors.
- Modify: `src/platform/wiiu/WiiURenderManager3D.hpp`
  Purpose: expose the latest built runtime model and declare helpers used to copy geometry from cooked or raw model assets.
- Modify: `src/platform/wiiu/WiiURenderManager3D.cpp`
  Purpose: deserialize cooked model assets, build one `WiiURuntimeModel`, copy positions and indices, and cache the latest model.
- Create: `tools/wiiu-shaders/scene_cube_flat_color.vs`
  Purpose: transform position-only cube vertices with one fixed matrix.
- Create: `tools/wiiu-shaders/scene_cube_flat_color.ps`
  Purpose: emit one constant flat color for the temporary cube bring-up path.
- Create: `data/scene_cube_flat_color_shader.bin`
  Purpose: offline-compiled GX2 shader blob for the scene-cube flat-color path.
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
  Purpose: declare one presenter-owned scene-cube mesh path and its GX2 resources.
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
  Purpose: create the scene-cube shader/buffer resources, upload runtime model geometry, and draw indexed cube triangles.
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
  Purpose: configure the presenter with the latest runtime model after `cube_test` loads and render the scene cube instead of the translated triangle.

### Task 1: Lock The Scene-Cube Runtime Seam

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test near the current diagnostic triangle seam tests:

```csharp
/// <summary>
/// Ensures the first cube_test 3D slice routes one real runtime model into a dedicated presenter-owned flat-color scene cube path.
/// </summary>
[Fact]
public void RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
    string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
    string runtimeModelHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeModel.hpp"));
    string shaderVertexSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_cube_flat_color.vs"));
    string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_cube_flat_color.ps"));

    Assert.Contains("WiiURuntimeModel* GetLatestRuntimeModel() const;", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void SetGeometry(std::vector<float> positionData, std::vector<std::uint16_t> indexData);", runtimeModelHeaderSource, StringComparison.Ordinal);
    Assert.Contains("const std::vector<float>& GetPositionData() const;", runtimeModelHeaderSource, StringComparison.Ordinal);
    Assert.Contains("const std::vector<std::uint16_t>& GetIndexData() const;", runtimeModelHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel);", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void RenderSceneCubeFrame();", presenterHeaderSource, StringComparison.Ordinal);
    Assert.Contains("GX2DrawIndexedEx(", presenterSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->ConfigureSceneCubeMesh(*EngineRenderManager3D->GetLatestRuntimeModel());", applicationSource, StringComparison.Ordinal);
    Assert.Contains("Gx2Presenter->RenderSceneCubeFrame();", applicationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("Gx2Presenter->RenderDiagnosticTriangleFrame();", applicationSource, StringComparison.Ordinal);
    Assert.Contains("gl_Position = uTransform * aPosition;", shaderVertexSource, StringComparison.Ordinal);
    Assert.Contains("passColor = vec4(", shaderPixelSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp -v minimal
```

Expected: FAIL because none of the scene-cube methods, shaders, or application routing exist yet.

- [ ] **Step 3: Commit the red test**

Run:

```powershell
rtk git add builder.tests/WiiURuntimeSourceTests.cs
rtk git commit -m "test: lock Wii U scene cube presenter contract"
```

### Task 2: Capture Real Cube Geometry In The Wii U Runtime Model

**Files:**
- Modify: `src/platform/wiiu/WiiURuntimeModel.hpp`
- Modify: `src/platform/wiiu/WiiURuntimeModel.cpp`
- Modify: `src/platform/wiiu/WiiURenderManager3D.hpp`
- Modify: `src/platform/wiiu/WiiURenderManager3D.cpp`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Run the focused test and confirm the geometry seam is still missing**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp -v minimal
```

Expected: FAIL with missing `WiiURuntimeModel` and `WiiURenderManager3D` seam strings.

- [ ] **Step 2: Add position and index storage to `WiiURuntimeModel`**

Update `src/platform/wiiu/WiiURuntimeModel.hpp` with these members and methods:

```cpp
#include <cstdint>
#include <vector>

namespace helengine::wiiu {
    /// Represents one Wii U runtime model that exposes copied mesh geometry for the first visible cube bring-up slice.
    class WiiURuntimeModel final : public ::RuntimeModel {
    public:
        /// Creates one empty runtime model with no uploaded geometry.
        WiiURuntimeModel();

        /// Replaces the copied model geometry exposed to the Wii U presenter bridge.
        void SetGeometry(std::vector<float> positionData, std::vector<std::uint16_t> indexData);

        /// Returns the copied position stream as XYZW float quads.
        const std::vector<float>& GetPositionData() const;

        /// Returns the copied 16-bit index stream used for indexed GX2 drawing.
        const std::vector<std::uint16_t>& GetIndexData() const;

    private:
        /// Stores copied model positions as tightly packed XYZW float quads.
        std::vector<float> PositionData;

        /// Stores copied 16-bit triangle indices.
        std::vector<std::uint16_t> IndexData;
    };
}
```

Update `src/platform/wiiu/WiiURuntimeModel.cpp` with these method bodies:

```cpp
void WiiURuntimeModel::SetGeometry(std::vector<float> positionData, std::vector<std::uint16_t> indexData) {
    PositionData = std::move(positionData);
    IndexData = std::move(indexData);
}

const std::vector<float>& WiiURuntimeModel::GetPositionData() const {
    return PositionData;
}

const std::vector<std::uint16_t>& WiiURuntimeModel::GetIndexData() const {
    return IndexData;
}
```

- [ ] **Step 3: Build real runtime models instead of empty stubs**

Update `src/platform/wiiu/WiiURenderManager3D.hpp` so the class owns the latest built model and exposes it:

```cpp
class WiiURenderManager3D final : public ::RenderManager3D {
public:
    /// Creates one Wii U 3D bridge with no cached runtime model.
    WiiURenderManager3D();

    /// Releases cached bridge state.
    ~WiiURenderManager3D() override;

    /// Returns the most recently built runtime model captured during scene loading.
    WiiURuntimeModel* GetLatestRuntimeModel() const;

    /// Releases one runtime model and clears the cached latest-model pointer when it matches.
    void ReleaseModel(::RuntimeModel* model) override;

private:
    /// Builds one Wii U runtime model from a shared model asset payload.
    WiiURuntimeModel* BuildRuntimeModelFromAsset(::ModelAsset* data);

    /// Stores the most recently built runtime model captured during scene loading.
    WiiURuntimeModel* LatestRuntimeModel;
};
```

Update `src/platform/wiiu/WiiURenderManager3D.cpp` so cooked model loading deserializes real model data and copies positions and indices:

```cpp
#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "ModelAsset.hpp"
#include "runtime/finally.hpp"
#include "system/io/file.hpp"

WiiURenderManager3D::WiiURenderManager3D()
    : RenderManager3D()
    , LatestRuntimeModel(nullptr) {
}

WiiURenderManager3D::~WiiURenderManager3D() {
    LatestRuntimeModel = nullptr;
}

WiiURuntimeModel* WiiURenderManager3D::GetLatestRuntimeModel() const {
    return LatestRuntimeModel;
}

::RuntimeModel* WiiURenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
    FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
    auto streamGuard = he_cpp_make_scope_exit([&]() {
        if (stream != nullptr) {
            stream->Dispose();
            delete stream;
        }
    });

    Asset* asset = AssetSerializer::Deserialize(stream);
    ModelAsset* modelAsset = he_cpp_try_cast<ModelAsset>(asset);
    if (modelAsset == nullptr) {
        delete asset;
        throw new InvalidOperationException("Wii U cooked model payload did not deserialize into a ModelAsset.");
    }

    auto assetGuard = he_cpp_make_scope_exit([&]() {
        modelAsset->Dispose();
        delete modelAsset;
    });
    return BuildRuntimeModelFromAsset(modelAsset);
}

::RuntimeModel* WiiURenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
    return BuildRuntimeModelFromAsset(data);
}

WiiURuntimeModel* WiiURenderManager3D::BuildRuntimeModelFromAsset(::ModelAsset* data) {
    std::vector<float> positionData;
    std::vector<std::uint16_t> indexData;
    const int32_t positionCount = data->Positions == nullptr ? 0 : data->Positions->get_Length();

    positionData.reserve(static_cast<std::size_t>(positionCount) * 4U);
    for (int32_t positionIndex = 0; positionIndex < positionCount; positionIndex++) {
        const float3 position = (*data->Positions)[positionIndex];
        positionData.push_back(position.X);
        positionData.push_back(position.Y);
        positionData.push_back(position.Z);
        positionData.push_back(1.0f);
    }

    if (data->Indices16 != nullptr && data->Indices16->get_Length() > 0) {
        indexData.reserve(static_cast<std::size_t>(data->Indices16->get_Length()));
        for (int32_t index = 0; index < data->Indices16->get_Length(); index++) {
            indexData.push_back((*data->Indices16)[index]);
        }
    } else if (data->Indices32 != nullptr && data->Indices32->get_Length() > 0) {
        indexData.reserve(static_cast<std::size_t>(data->Indices32->get_Length()));
        for (int32_t index = 0; index < data->Indices32->get_Length(); index++) {
            const std::uint32_t sourceIndex = (*data->Indices32)[index];
            if (sourceIndex > 0xFFFFU) {
                throw new InvalidOperationException("Wii U first cube bring-up only supports 16-bit indexable geometry.");
            }

            indexData.push_back(static_cast<std::uint16_t>(sourceIndex));
        }
    } else {
        indexData.reserve(static_cast<std::size_t>(positionCount));
        for (int32_t index = 0; index < positionCount; index++) {
            indexData.push_back(static_cast<std::uint16_t>(index));
        }
    }

    WiiURuntimeModel* runtimeModel = new WiiURuntimeModel();
    runtimeModel->SetGeometry(std::move(positionData), std::move(indexData));
    LatestRuntimeModel = runtimeModel;
    return runtimeModel;
}

void WiiURenderManager3D::ReleaseModel(::RuntimeModel* model) {
    if (model == LatestRuntimeModel) {
        LatestRuntimeModel = nullptr;
    }

    RenderManager3D::ReleaseModel(model);
}
```

- [ ] **Step 4: Run the focused test to verify the presenter and app routing are still missing**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp -v minimal
```

Expected: FAIL because `WiiUGx2Presenter` still does not implement `ConfigureSceneCubeMesh` / `RenderSceneCubeFrame`, and `WiiUApplication` still renders the triangle.

- [ ] **Step 5: Commit the geometry seam**

Run:

```powershell
rtk git add src/platform/wiiu/WiiURuntimeModel.hpp src/platform/wiiu/WiiURuntimeModel.cpp src/platform/wiiu/WiiURenderManager3D.hpp src/platform/wiiu/WiiURenderManager3D.cpp
rtk git commit -m "feat: capture Wii U cube geometry from runtime models"
```

### Task 3: Add The Presenter-Owned Flat-Color Scene Cube Path

**Files:**
- Create: `tools/wiiu-shaders/scene_cube_flat_color.vs`
- Create: `tools/wiiu-shaders/scene_cube_flat_color.ps`
- Create: `data/scene_cube_flat_color_shader.bin`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Add the flat-color cube shaders**

Create `tools/wiiu-shaders/scene_cube_flat_color.vs`:

```glsl
#version 330

layout(location = 0) in vec4 aPosition;

layout(std140) uniform TransformBlock {
    mat4 uTransform;
};

void main() {
    gl_Position = uTransform * aPosition;
}
```

Create `tools/wiiu-shaders/scene_cube_flat_color.ps`:

```glsl
#version 330

layout(location = 0) out vec4 passColor;

void main() {
    passColor = vec4(0.92, 0.78, 0.32, 1.0);
}
```

- [ ] **Step 2: Compile the new shader blob**

Run:

```powershell
rtk wsl bash -lc "cd /mnt/c/dev/helworks/helengine-wiiu && tools/cafeglsl/glslcompiler.elf -ps tools/wiiu-shaders/scene_cube_flat_color.ps -vs tools/wiiu-shaders/scene_cube_flat_color.vs -o data/scene_cube_flat_color_shader.bin"
```

Expected: PASS and update `data/scene_cube_flat_color_shader.bin`.

- [ ] **Step 3: Add scene-cube presenter resources and draw code**

Update `src/platform/wiiu/WiiUGx2Presenter.hpp` with these scene-cube methods and fields:

```cpp
class WiiURuntimeModel;

/// Uploads one runtime model into the temporary scene-cube GX2 mesh path.
void ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel);

/// Renders the configured scene-cube mesh to both displays.
void RenderSceneCubeFrame();

void InitializeSceneCubeResources();
void DestroySceneCubeResources();
void InitializeSceneCubeVertexBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t floatCount);
void InitializeSceneCubeIndexBuffer(GX2RBuffer* buffer, const std::uint16_t* sourceData, std::uint32_t indexCount);
void InitializeSceneCubeTransformBuffer();
void RenderSceneCubeToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);

bool AreSceneCubeResourcesInitialized;
bool IsSceneCubeMeshConfigured;
WHBGfxShaderGroup SceneCubeShaderGroup;
GX2RBuffer SceneCubePositionBuffer;
GX2RBuffer SceneCubeIndexBuffer;
GX2RBuffer SceneCubeTransformBuffer;
std::uint32_t SceneCubeIndexCount;
```

Update `src/platform/wiiu/WiiUGx2Presenter.cpp` to load `scene_cube_flat_color_shader_bin.h`, create the new resources during `Initialize()`, upload one forced simple camera matrix, and draw indexed triangles:

```cpp
#include "scene_cube_flat_color_shader_bin.h"

const float SceneCubeTransformData[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, -2.5f, 1.0f
};

void WiiUGx2Presenter::ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel) {
    const std::vector<float>& positionData = runtimeModel.GetPositionData();
    const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
    if (positionData.empty() || indexData.empty()) {
        throw std::runtime_error("Wii U scene cube configuration requires non-empty runtime model geometry.");
    }

    if (SceneCubePositionBuffer.buffer != nullptr) {
        GX2RDestroyBufferEx(&SceneCubePositionBuffer, NoGx2rResourceFlags);
        std::memset(&SceneCubePositionBuffer, 0, sizeof(SceneCubePositionBuffer));
    }
    if (SceneCubeIndexBuffer.buffer != nullptr) {
        GX2RDestroyBufferEx(&SceneCubeIndexBuffer, NoGx2rResourceFlags);
        std::memset(&SceneCubeIndexBuffer, 0, sizeof(SceneCubeIndexBuffer));
    }

    InitializeSceneCubeVertexBuffer(&SceneCubePositionBuffer, positionData.data(), static_cast<std::uint32_t>(positionData.size()));
    InitializeSceneCubeIndexBuffer(&SceneCubeIndexBuffer, indexData.data(), static_cast<std::uint32_t>(indexData.size()));
    SceneCubeIndexCount = static_cast<std::uint32_t>(indexData.size());
    IsSceneCubeMeshConfigured = true;
}

void WiiUGx2Presenter::RenderSceneCubeFrame() {
    if (!IsInitialized || !AreSceneCubeResourcesInitialized || !IsSceneCubeMeshConfigured) {
        throw std::runtime_error("Wii U scene cube frame rendering requires initialized GX2 scene cube resources and uploaded mesh data.");
    }

    RenderSceneCubeToColorBuffer(TvContextState, &TvColorBuffer);
    RenderSceneCubeToColorBuffer(DrcContextState, &DrcColorBuffer);
    PresentScanBuffers();
}

void WiiUGx2Presenter::RenderSceneCubeToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer) {
    GX2SetContextState(contextState);
    GX2ClearColor(colorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
    GX2SetFetchShader(&SceneCubeShaderGroup.fetchShader);
    GX2SetVertexShader(SceneCubeShaderGroup.vertexShader);
    GX2SetPixelShader(SceneCubeShaderGroup.pixelShader);
    GX2RSetAttributeBuffer(&SceneCubePositionBuffer, 0, SceneCubePositionBuffer.elemSize, SceneCubePositionBuffer.elemCount * SceneCubePositionBuffer.elemSize);
    GX2RSetVertexUniformBlock(&SceneCubeTransformBuffer, 0, 0);
    GX2RSetIndexBuffer(&SceneCubeIndexBuffer, GX2_INDEX_TYPE_U16, SceneCubeIndexCount * sizeof(std::uint16_t));
    GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneCubeIndexCount, GX2_INDEX_TYPE_U16, nullptr, 0, 1);
}
```

- [ ] **Step 4: Run the focused test to verify only the application routing remains**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp -v minimal
```

Expected: FAIL because `WiiUApplication` still configures and presents the diagnostic triangle path.

- [ ] **Step 5: Commit the presenter path**

Run:

```powershell
rtk git add tools/wiiu-shaders/scene_cube_flat_color.vs tools/wiiu-shaders/scene_cube_flat_color.ps data/scene_cube_flat_color_shader.bin src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp
rtk git commit -m "feat: add Wii U scene cube GX2 presenter path"
```

### Task 4: Route `cube_test` Through The Scene-Cube Presenter And Verify In Cemu

**Files:**
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`
- Build: `C:\dev\helworks\helengine\artifacts\build-platform.ps1`

- [ ] **Step 1: Configure the presenter mesh after `cube_test` loads**

Update `InitializeEngineCore()` in `src/platform/wiiu/WiiUApplication.cpp` immediately after the `cube_test` startup scene is queued and before `EngineInitialized = true;`:

```cpp
WiiURuntimeModel* latestRuntimeModel = EngineRenderManager3D->GetLatestRuntimeModel();
if (latestRuntimeModel == nullptr) {
    throw new InvalidOperationException("Wii U cube_test bring-up requires one runtime model to be built during scene load.");
}

Gx2Presenter->ConfigureSceneCubeMesh(*latestRuntimeModel);
AppendRuntimeTrace("[WiiUFile] Scene cube mesh configured from latest runtime model.\n");
```

Also add the include near the existing Wii U includes:

```cpp
#include "platform/wiiu/WiiURuntimeModel.hpp"
```

- [ ] **Step 2: Replace the translated triangle present path**

Update `PresentRenderedFrame()` in `src/platform/wiiu/WiiUApplication.cpp` to render the scene cube:

```cpp
void WiiUApplication::PresentRenderedFrame() {
    if (Gx2Presenter == nullptr) {
        throw std::runtime_error("Wii U GX2 presenter must exist before rendered presentation can begin.");
    } else if (EngineRenderManager3D == nullptr) {
        throw std::runtime_error("Wii U 3D render manager must exist before rendered presentation can begin.");
    }

    Gx2Presenter->RenderSceneCubeFrame();
}
```

- [ ] **Step 3: Run the focused test and verify it passes**

Run:

```powershell
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesSceneCubePresenterPathForFirstCubeTestMeshBringUp -v minimal
```

Expected: PASS.

- [ ] **Step 4: Build the real Wii U `city` output**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform wiiu -Output C:\dev\helprojs\city\wiiu-build
```

Expected: EXIT `0` and fresh `helengine_wiiu.rpx` / `helengine_wiiu.wuhb` timestamps under `C:\dev\helprojs\city\wiiu-build`.

- [ ] **Step 5: Launch in Cemu and confirm the visible cube**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

Expected: one visible stable flat-colored cube on screen.

If the cube is not visible, debug in this order:

```text
1. Check the runtime trace for "Scene cube mesh configured" to confirm the model was captured.
2. Add one temporary runtime trace line that prints PositionData.size() and IndexData.size() before ConfigureSceneCubeMesh uploads buffers.
3. Add one temporary runtime trace line that prints SceneCubeIndexCount after buffer upload.
4. If indexed drawing is suspect, temporarily replace GX2DrawIndexedEx(...) with GX2DrawEx(..., SceneCubeIndexCount, 0, 1) after expanding indices into a non-indexed position buffer.
```

- [ ] **Step 6: Commit the routed cube path**

Run:

```powershell
rtk git add src/platform/wiiu/WiiUApplication.cpp
rtk git commit -m "feat: render first cube_test cube on Wii U"
```
