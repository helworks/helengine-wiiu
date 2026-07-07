# Wii U Fixed Host Perspective Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the temporary orthographic-looking cube with one fixed host-side perspective camera transform while preserving the proven no-uniform GX2 scene-cube shader path.

**Architecture:** Keep the scene-cube shader on direct clip-space passthrough and compute the full world-view-projection result on the CPU during scene-cube geometry upload. Restore the real runtime cube geometry, apply a fixed yaw/pitch plus a fixed camera/projection transform, and upload the resulting homogeneous clip-space positions to the existing scene-cube GX2 path.

**Tech Stack:** C++20, GX2/GX2R, CafeGLSL, Wii U host runtime seam tests, PowerShell, Docker devkitPPC build, Cemu

---

## File Map

- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
  Purpose: lock the fixed host-side perspective seam and the no-uniform shader contract.
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
  Purpose: replace the temporary CPU orthographic rotation with a CPU-computed homogeneous perspective transform for real runtime cube geometry.
- Modify: `tools/wiiu-shaders/scene_cube_flat_color.vs`
  Purpose: keep the proven no-uniform clip-space passthrough contract.
- Modify: `tools/wiiu-shaders/scene_cube_flat_color.ps`
  Purpose: preserve the flat-color output contract.
- Modify: `data/scene_cube_flat_color_shader.bin`
  Purpose: refresh the compiled GX2 shader blob from the current GLSL sources.

### Task 1: Lock The Fixed Host Perspective Seam

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Add this test near the current scene-cube seam tests:

```csharp
/// <summary>
/// Ensures the first Wii U perspective slice keeps the no-uniform scene-cube shader contract and computes one fixed host-side camera transform in the presenter upload path.
/// </summary>
[Fact]
public void RuntimeSeam_UsesFixedHostPerspectiveCameraForSceneCubeBringUp() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
    string shaderVertexSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_cube_flat_color.vs"));
    string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_cube_flat_color.ps"));

    Assert.Contains("constexpr double SceneCubeFieldOfViewRadians =", presenterSource, StringComparison.Ordinal);
    Assert.Contains("constexpr double SceneCubeCameraDistance =", presenterSource, StringComparison.Ordinal);
    Assert.Contains("const double clipW = -viewZ;", presenterSource, StringComparison.Ordinal);
    Assert.Contains("expandedPositionData.push_back(static_cast<float>(clipX));", presenterSource, StringComparison.Ordinal);
    Assert.Contains("expandedPositionData.push_back(static_cast<float>(clipY));", presenterSource, StringComparison.Ordinal);
    Assert.Contains("expandedPositionData.push_back(static_cast<float>(clipZ));", presenterSource, StringComparison.Ordinal);
    Assert.Contains("expandedPositionData.push_back(static_cast<float>(clipW));", presenterSource, StringComparison.Ordinal);
    Assert.Contains("gl_Position = aPosition;", shaderVertexSource, StringComparison.Ordinal);
    Assert.DoesNotContain("TransformBlock", shaderVertexSource, StringComparison.Ordinal);
    Assert.Contains("FragColor = VertexColor;", shaderPixelSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesFixedHostPerspectiveCameraForSceneCubeBringUp -v minimal
```

Expected: FAIL because the presenter does not yet contain the named fixed-perspective constants and clip-space projection statements.

- [ ] **Step 3: Write minimal implementation**

Update `src/platform/wiiu/WiiUGx2Presenter.cpp` so `ConfigureSceneCubeMesh`:

```cpp
constexpr double SceneCubeYawRadians = 0.65;
constexpr double SceneCubePitchRadians = -0.55;
constexpr double SceneCubeFieldOfViewRadians = 1.0;
constexpr double SceneCubeCameraDistance = 5.0;
constexpr double SceneCubeNearPlane = 0.1;
constexpr double SceneCubeFarPlane = 64.0;
constexpr double SceneCubeAspectRatio = 1280.0 / 720.0;
```

and computes per-vertex clip-space output like:

```cpp
const double viewZ = rotatedZ - SceneCubeCameraDistance;
const double projectionScaleY = 1.0 / std::tan(SceneCubeFieldOfViewRadians * 0.5);
const double projectionScaleX = projectionScaleY / SceneCubeAspectRatio;
const double clipX = rotatedX * projectionScaleX;
const double clipY = rotatedY * projectionScaleY;
const double clipZ =
    ((SceneCubeFarPlane + SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane)) * viewZ +
    ((2.0 * SceneCubeFarPlane * SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane));
const double clipW = -viewZ;
```

Push those four values into `expandedPositionData`, restoring real runtime cube geometry rather than the hardcoded triangle.

Keep `tools/wiiu-shaders/scene_cube_flat_color.vs` on:

```glsl
gl_Position = aPosition;
```

with no `TransformBlock`.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
rtk dotnet test builder.tests\helengine.wiiu.builder.tests.csproj --filter FullyQualifiedName~RuntimeSeam_UsesFixedHostPerspectiveCameraForSceneCubeBringUp -v minimal
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
rtk git add builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUGx2Presenter.cpp tools/wiiu-shaders/scene_cube_flat_color.vs tools/wiiu-shaders/scene_cube_flat_color.ps
rtk git commit -m "feat: add Wii U fixed host perspective camera"
```

### Task 2: Refresh Shader Blob And Verify In Cemu

**Files:**
- Modify: `data/scene_cube_flat_color_shader.bin`
- Build: `build/helengine_wiiu.wuhb`

- [ ] **Step 1: Recompile the scene-cube shader blob**

Run:

```bash
rtk proxy wsl bash -lc "cd /mnt/c/dev/helworks/helengine-wiiu && tools/cafeglsl/glslcompiler.elf -ps tools/wiiu-shaders/scene_cube_flat_color.ps -vs tools/wiiu-shaders/scene_cube_flat_color.vs -o data/scene_cube_flat_color_shader.bin"
```

Expected: PASS and update `data/scene_cube_flat_color_shader.bin`.

- [ ] **Step 2: Rebuild the Wii U title**

Run:

```bash
rtk proxy docker run --rm -e HELENGINE_CORE_CPP_ROOT=/helengine-generated-core -v ${PWD}:/workspace -v C:\Users\Helena\AppData\Local\Temp\helengine-builds\a1520f01edd0e0ae710746d92aa1d694\wiiu\workspace\cc1e48505c644ffbbbf1f7c2332aded5\generated-core:/helengine-generated-core -v C:\Users\Helena\AppData\Local\Temp\helengine-builds\a1520f01edd0e0ae710746d92aa1d694\wiiu\workspace\cc1e48505c644ffbbbf1f7c2332aded5\package:/workspace/content helengine-wiiu:latest /bin/bash -lc "cd /workspace && make CONTENT=/workspace/content APP_CONTENT=/workspace/content"
```

Expected: PASS and produce `build/helengine_wiiu.wuhb`.

- [ ] **Step 3: Launch in Cemu**

Run:

```bash
rtk proxy powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helworks\helengine-wiiu\build\helengine_wiiu.wuhb
```

Expected: PASS and start a fresh Cemu session with the rebuilt artifact.

- [ ] **Step 4: Verify runtime boot stays healthy**

Run:

```bash
Get-Content -Tail 30 C:\Users\Helena\AppData\Roaming\Cemu\sdcard\wiiu_runtime_trace.txt
```

Expected: the trace still reaches `Scene cube mesh configured from latest runtime model.`

- [ ] **Step 5: Commit**

```bash
rtk git add data/scene_cube_flat_color_shader.bin
rtk git commit -m "build: refresh Wii U scene cube shader blob"
```
