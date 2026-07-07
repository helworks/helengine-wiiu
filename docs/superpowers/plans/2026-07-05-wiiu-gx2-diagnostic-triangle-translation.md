# Wii U GX2 Diagnostic Triangle Translation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add one presenter-owned vertex-uniform transform path so the diagnostic Wii U GX2 triangle renders at a fixed upper-right translated position.

**Architecture:** Keep the existing `RenderDiagnosticTriangleFrame()` runtime seam and extend only the diagnostic triangle path. Update the vertex shader to consume one transform uniform, add one presenter-owned GX2 uniform buffer that stores a fixed translation matrix, bind it during triangle rendering, and verify the visible result in Cemu.

**Tech Stack:** C++20, Wii U GX2/GX2R, WHB GFD shader loading, CafeGLSL shader compilation, xUnit source-contract tests, PowerShell build/launch scripts, Cemu

---

## File Map

- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
  Purpose: lock the uniform-driven translation seam with a focused source-contract test.
- Modify: `tools/wiiu-shaders/diagnostic_triangle.vs`
  Purpose: consume one transform uniform and multiply it against the triangle position.
- Modify: `data/diagnostic_triangle_shader.bin`
  Purpose: updated embedded shader blob compiled from the revised shader sources.
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
  Purpose: declare the diagnostic triangle transform buffer and related initialization/render helpers.
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
  Purpose: allocate, upload, bind, and destroy the fixed translation uniform buffer in the diagnostic triangle path.

### Task 1: Lock The Uniform Translation Contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add one new test named `RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp`.

Assertions:
- `tools/wiiu-shaders/diagnostic_triangle.vs` contains `uTransform`
- `tools/wiiu-shaders/diagnostic_triangle.vs` contains `gl_Position = uTransform * aPosition;`
- `src/platform/wiiu/WiiUGx2Presenter.hpp` contains `GX2RBuffer DiagnosticTriangleTransformBuffer;`
- `src/platform/wiiu/WiiUGx2Presenter.cpp` contains `InitializeDiagnosticTriangleTransformBuffer`
- `src/platform/wiiu/WiiUGx2Presenter.cpp` contains `GX2RSetVertexUniformBlock`
- `src/platform/wiiu/WiiUGx2Presenter.cpp` contains `DiagnosticTriangleTransformBuffer`

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp -v minimal`

Expected: FAIL because the current shader and presenter source do not yet contain the transform uniform/buffer path.

- [ ] **Step 3: Commit the red test**

Run:
```bash
git add builder.tests/WiiURuntimeSourceTests.cs
git commit -m "test: lock Wii U triangle transform buffer contract"
```

### Task 2: Add The Shader Uniform Path

**Files:**
- Modify: `tools/wiiu-shaders/diagnostic_triangle.vs`
- Modify: `data/diagnostic_triangle_shader.bin`

- [ ] **Step 1: Update the vertex shader**

Change `tools/wiiu-shaders/diagnostic_triangle.vs` so it:
- declares one transform uniform named `uTransform`
- preserves the existing color pass-through
- computes `gl_Position = uTransform * aPosition;`

- [ ] **Step 2: Run the focused test to confirm it still fails for the presenter-side gap**

Run: `rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp -v minimal`

Expected: FAIL because the presenter still does not own or bind the transform buffer.

- [ ] **Step 3: Rebuild the embedded diagnostic shader blob**

Run: `rtk wsl bash -lc "cd /mnt/c/dev/helworks/helengine-wiiu && tools/cafeglsl/glslcompiler.elf -ps tools/wiiu-shaders/diagnostic_triangle.ps -vs tools/wiiu-shaders/diagnostic_triangle.vs -o data/diagnostic_triangle_shader.bin"`

Expected: shader compilation succeeds and refreshes `data/diagnostic_triangle_shader.bin`.

- [ ] **Step 4: Commit the shader update**

Run:
```bash
git add tools/wiiu-shaders/diagnostic_triangle.vs data/diagnostic_triangle_shader.bin
git commit -m "feat: add Wii U triangle transform shader uniform"
```

### Task 3: Implement The Presenter-Owned Transform Buffer

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.hpp`
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Add the presenter declarations**

In `WiiUGx2Presenter.hpp`, add:
- one `GX2RBuffer DiagnosticTriangleTransformBuffer;`
- one initialization helper for the transform buffer
- one upload helper or fixed-matrix initialization helper if needed

Keep the public seam unchanged.

- [ ] **Step 2: Implement the transform buffer lifecycle**

In `WiiUGx2Presenter.cpp`:
- define one fixed 4x4 translation matrix that moves the triangle into the upper-right quadrant
- allocate the transform buffer during diagnostic triangle resource initialization
- upload the fixed matrix into the buffer
- invalidate the buffer after CPU writes
- destroy the buffer during diagnostic triangle shutdown

- [ ] **Step 3: Bind the transform buffer during diagnostic triangle rendering**

In `RenderDiagnosticTriangleToColorBuffer()`:
- keep the existing clear and attribute bind flow
- bind the transform buffer with `GX2RSetVertexUniformBlock` or the required GX2 vertex uniform block call for the compiled shader
- preserve the existing draw call

- [ ] **Step 4: Run the focused test to verify it passes**

Run: `rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp -v minimal`

Expected: PASS.

- [ ] **Step 5: Run the related diagnostic triangle seam tests**

Run: `rtk dotnet test C:\dev\helworks\helengine-wiiu\builder.tests\helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~RuntimeSeam_RoutesVisibleOutputThroughDedicatedPresenterOwnedGx2Path|FullyQualifiedName~RuntimeSeam_UsesPresentOnlyDiagnosticFrameLoopForFirst3dShaderBringUp|FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp|FullyQualifiedName~RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp|FullyQualifiedName~RuntimeSeam_FullEngineLoopDoesNotKeepSkipping2DRendererSubmission" -v minimal`

Expected: PASS.

- [ ] **Step 6: Commit the presenter implementation**

Run:
```bash
git add src/platform/wiiu/WiiUGx2Presenter.hpp src/platform/wiiu/WiiUGx2Presenter.cpp builder.tests/WiiURuntimeSourceTests.cs
git commit -m "feat: add Wii U translated diagnostic triangle transform buffer"
```

### Task 4: Rebuild And Verify In Cemu

**Files:**
- Verify: `build/helengine_wiiu.wuhb`
- Verify: `C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`
- Verify: `C:\Users\Helena\AppData\Roaming\Cemu\log.txt`
- Verify: `C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt`

- [ ] **Step 1: Rebuild the native Wii U host**

Use the same direct native build path that currently bypasses the editor-side scene payload blocker:

Run: `rtk docker run --rm -v C:/dev/helworks/helengine-wiiu:/workspace -v C:/Users/Helena/AppData/Local/Temp/helengine-builds/a1520f01edd0e0ae710746d92aa1d694/wiiu/workspace/07774d8c4caf4afe8228f3dda47dc105/generated-core:/helengine-generated-core -v C:/Users/Helena/AppData/Local/Temp/helengine-builds/a1520f01edd0e0ae710746d92aa1d694/wiiu/workspace/07774d8c4caf4afe8228f3dda47dc105/builder/package-source:/workspace/content -w /workspace -e HELENGINE_CORE_CPP_ROOT=/helengine-generated-core helengine-wiiu sh -lc "make CONTENT=/workspace/content APP_CONTENT=/workspace/content"`

Expected: `build/helengine_wiiu.wuhb` and `build/helengine_wiiu.rpx` refresh successfully.

- [ ] **Step 2: Stage the refreshed runtime artifact to the normal launch output**

Run: `rtk powershell -NoProfile -Command "Copy-Item -LiteralPath 'C:\dev\helworks\helengine-wiiu\build\helengine_wiiu.wuhb' -Destination 'C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb' -Force; Copy-Item -LiteralPath 'C:\dev\helworks\helengine-wiiu\build\helengine_wiiu.rpx' -Destination 'C:\dev\helprojs\city\wiiu-build\helengine_wiiu.rpx' -Force"`

Expected: the packaged output root points at the fresh translated-triangle binary.

- [ ] **Step 3: Launch the refreshed WUHB in Cemu**

Run: `rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wiiu\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\wiiu-build\helengine_wiiu.wuhb`

Expected: Cemu launches and prints `PROCESS_ID=`.

- [ ] **Step 4: Verify runtime logs stay alive**

Run:
- `rtk powershell -NoProfile -Command "Get-Content 'C:\Users\Helena\AppData\Roaming\Cemu\log.txt' -Tail 80 | Out-String"`
- `rtk powershell -NoProfile -Command "Get-Content 'C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt' -Tail 50 | Out-String"`

Expected:
- no immediate `coreinit.exit(1)` termination for the translated-triangle launch
- runtime trace reaches the new session start and remains in the diagnostic present loop

- [ ] **Step 5: Verify visually in Cemu**

Confirm in Cemu that:
- the triangle is no longer centered
- it appears in the upper-right portion of the screen
- colors and clear color remain correct

- [ ] **Step 6: Commit if any final source adjustment was required during runtime verification**

Run:
```bash
git add <only the source files changed during runtime verification>
git commit -m "fix: finalize Wii U translated diagnostic triangle verification"
```

If no final source adjustment was required, skip this step.
