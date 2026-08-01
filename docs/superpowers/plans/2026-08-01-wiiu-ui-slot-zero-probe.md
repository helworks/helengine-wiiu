# Wii U UI Slot-Zero Hardware Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a real-hardware WUHB that renders one opaque cyan full-screen quad through the existing Wii U textured UI pipeline with vertex offset zero.

**Architecture:** Add one temporary application presentation mode and one dedicated presenter entry point. The presenter uploads six known vertices into the existing UI buffers once per frame, renders them to the TV and GamePad through the existing UI shader, white texture, sampler, and blend state, then uses the established scan-buffer presentation path.

**Tech Stack:** C++17, WUT GX2/GX2R/libwhb, C# xUnit source-contract tests, HelEngine Wii U build pipeline.

---

### Task 1: Specify the diagnostic branch and draw contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing source-contract test**

Add a test named `RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe` that reads `WiiUApplication.cpp`, `WiiUGx2Presenter.hpp`, and `WiiUGx2Presenter.cpp`. Require all of the following exact contracts:

```csharp
Assert.Contains("UiSlotZeroProbe", applicationSource, StringComparison.Ordinal);
Assert.Contains("constexpr DiagnosticPresentationMode DiagnosticPresentationModeValue = DiagnosticPresentationMode::UiSlotZeroProbe;", applicationSource, StringComparison.Ordinal);
Assert.Contains("Gx2Presenter->RenderDiagnosticUiSlotZeroFrame();", applicationSource, StringComparison.Ordinal);
Assert.Contains("void RenderDiagnosticUiSlotZeroFrame();", presenterHeaderSource, StringComparison.Ordinal);
Assert.Contains("void RenderDiagnosticUiSlotZeroToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);", presenterHeaderSource, StringComparison.Ordinal);
Assert.Contains("GX2SetPixelTexture(&UiSolidWhiteTextureHandle.Texture", presenterSource, StringComparison.Ordinal);
Assert.Contains("GX2SetPixelSampler(&UiSolidWhiteTextureHandle.Sampler", presenterSource, StringComparison.Ordinal);
Assert.Contains("GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, UiQuadVertexCount, 0, 1);", presenterSource, StringComparison.Ordinal);
```

Update the existing captured-presentation test so it continues to require the captured-frame branch and `RenderFrame(frame3D, frame2D)`, but no longer requires `CapturedFrame` to be the currently selected diagnostic constant.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe|FullyQualifiedName~RuntimeSeam_SelectsCapturedPresentationAfterForegroundLifecycleRepair" -v minimal
```

Expected: FAIL because `UiSlotZeroProbe`, `RenderDiagnosticUiSlotZeroFrame`, and `RenderDiagnosticUiSlotZeroToColorBuffer` do not exist.

### Task 2: Implement the slot-zero UI probe

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Declare the diagnostic presenter methods**

Add these declarations with substantive `///` comments:

```cpp
/// Renders one opaque cyan full-screen quad through the textured UI pipeline with vertex offset zero.
void RenderDiagnosticUiSlotZeroFrame();

/// Renders the known slot-zero UI probe into one target color buffer using the existing UI shader and white texture.
void RenderDiagnosticUiSlotZeroToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);
```

- [ ] **Step 2: Implement the public TV/GamePad diagnostic entry point**

Validate `IsInitialized`, `AreForegroundResourcesAcquired`, and `AreUiQuadResourcesInitialized`, upload the six full-screen vertices into the existing UI position, texture-coordinate, and color buffers, invalidate all three buffers after CPU writes, draw to both targets, and present:

```cpp
const float positionData[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f, -1.0f,
     1.0f,  1.0f,
    -1.0f,  1.0f
};
const float texCoordData[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f
};
const float colorData[] = {
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 1.0f, 1.0f
};
```

Use `EnsureUiQuadBufferCapacity(UiQuadVertexCount)`, `GX2RLockBufferEx`, `std::memcpy`, `GX2RUnlockBufferEx`, and `GX2RInvalidateBuffer(..., GX2R_RESOURCE_USAGE_CPU_WRITE)` exactly as the captured UI upload path does. Then call:

```cpp
RenderDiagnosticUiSlotZeroToColorBuffer(TvContextState, &TvColorBuffer);
RenderDiagnosticUiSlotZeroToColorBuffer(DrcContextState, &DrcColorBuffer);
PresentScanBuffers();
```

- [ ] **Step 3: Implement the per-target textured UI draw**

Bind the supplied context, target, full viewport and scissor, purple diagnostic clear, existing UI fetch/vertex/pixel shaders, uniform-block shader mode, disabled depth, copy color control, the existing alpha blend state, and all target channel masks. Bind all three UI attribute buffers at byte offset zero, then bind the presenter-owned white texture and sampler at `UiQuadShaderGroup.pixelShader->samplerVars[0].location`. Submit exactly:

```cpp
GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, UiQuadVertexCount, 0, 1);
```

- [ ] **Step 4: Select the temporary diagnostic mode**

Extend `DiagnosticPresentationMode` with:

```cpp
/// Presents one known textured UI quad from vertex slot zero on both display targets.
UiSlotZeroProbe,
```

Set `DiagnosticPresentationModeValue` to `DiagnosticPresentationMode::UiSlotZeroProbe`. In `PresentRenderedFrame`, preserve clear-only first, then add:

```cpp
if (DiagnosticPresentationModeValue == DiagnosticPresentationMode::UiSlotZeroProbe) {
    Gx2Presenter->RenderDiagnosticUiSlotZeroFrame();
    HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame UI slot-zero probe completed.\n");
    return;
}
```

- [ ] **Step 5: Run the focused tests and verify GREEN**

Run the same filtered `dotnet test` command from Task 1.

Expected: PASS for both source-contract tests.

### Task 3: Validate and package the hardware probe

**Files:**
- Verify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Verify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Verify: `src/platform/wiiu/WiiUApplication.cpp`
- Produce: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`

- [ ] **Step 1: Run the focused Wii U source-test subset**

Run:

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe|FullyQualifiedName~RuntimeSeam_SelectsCapturedPresentationAfterForegroundLifecycleRepair|FullyQualifiedName~RuntimeSeam_SelectsBatchedUiQuadsWithBaseVertex|FullyQualifiedName~RuntimeSeam_PlacesHardwarePresentationResourcesInRequiredHeaps" -v minimal
```

Expected: PASS with no failed tests.

- [ ] **Step 2: Build the Demo Disc WUHB**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build
```

Expected: `Build completed for platform 'wiiu': C:\dev\helprojs\demodisc\wiiu-build` and a freshly timestamped `helengine_wiiu.wuhb`.

- [ ] **Step 3: Record artifact identity**

Run:

```powershell
Get-Item C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb | Select-Object FullName,Length,LastWriteTime
Get-FileHash C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb -Algorithm SHA256
```

Expected: nonzero file size, current build timestamp, and a SHA-256 hash suitable for confirming the deployed artifact.

- [ ] **Step 4: Perform the real-hardware observation**

Deploy the WUHB and report TV and GamePad output without taking a screenshot. Cyan on both targets validates the slot-zero textured UI pipeline; purple on both localizes the defect inside that common path; different colors localize it to target context or presentation state.
