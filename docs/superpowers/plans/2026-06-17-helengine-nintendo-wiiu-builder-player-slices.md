# Nintendo Wii U Builder And Player Slices Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the first editor-CLI-driven Wii U build path, repo-local Cemu launcher, runtime seam progression, packaged bootstrap wiring, and final `cube_test` startup flow using the same incremental pattern already used by Wii and GameCube.

**Architecture:** Build Wii U in four slices. Slice 1 adds a minimal builder assembly plus shared editor registration so `build-platform.ps1` can emit a launchable `RPX` while the current `WiiUBootHost` still owns runtime behavior. Slice 2 introduces a Wii U runtime/application seam without changing the first visible result, Slice 3 connects staged builder output to a Wii U packaged scene bootstrap, and Slice 4 points that packaged bootstrap at the authored `cube_test` scene.

**Tech Stack:** C# / .NET 9 Windows builder assemblies, xUnit source-contract tests, PowerShell launcher scripts, shared Helengine editor CLI, Docker + devkitPro `wut`, C++17, Wii U OSScreen/WHB APIs, Cemu

---

## File Structure

- Create: `builder/helengine.wiiu.builder.csproj`
  Purpose: define the Wii U platform builder assembly loaded by the shared editor.
- Create: `builder/Program.cs`
  Purpose: expose the same low-level command-line entry pattern used by other platform builders.
- Create: `builder/WiiUPlatformAssetBuilder.cs`
  Purpose: implement `IPlatformAssetBuilder` for Wii U and route `BuildAsync(...)` into one Wii U workspace path.
- Create: `builder/WiiUPlatformDefinitionFactory.cs`
  Purpose: publish the first Wii U build profile and graphics/runtime metadata to the editor.
- Create: `builder/WiiUBuildWorkspace.cs`
  Purpose: stage editor payloads, invoke the native Wii U build, and copy the resulting `RPX` into the requested output root.
- Create: `builder/WiiUBuilderPaths.cs`
  Purpose: centralize well-known output file names and staging paths.
- Create: `builder/IWiiUNativeBuildExecutor.cs`
  Purpose: abstract the native Wii U build step for tests and for the Docker executor.
- Create: `builder/WiiUDockerNativeBuildExecutor.cs`
  Purpose: build the Wii U player through Docker and return the built `RPX`.
- Create: `builder.tests/helengine.wiiu.builder.tests.csproj`
  Purpose: host Wii U builder, launcher, README, and runtime source-contract tests.
- Create: `builder.tests/WiiUPlatformAssetBuilderTests.cs`
  Purpose: guard Wii U builder metadata and the first editor-build artifact flow.
- Create: `builder.tests/WiiUCemuLauncherScriptTests.cs`
  Purpose: guard the explicit Cemu launcher script and README contract.
- Create: `builder.tests/WiiURuntimeSourceTests.cs`
  Purpose: guard the Wii U runtime seam and packaged bootstrap source contract.
- Create: `builder.tests/WiiURuntimeSceneManifestWriterTests.cs`
  Purpose: guard the builder-emitted Wii U runtime scene manifest contract.
- Create: `scripts/launch_wiiu_rpx_in_cemu.ps1`
  Purpose: launch one explicit Wii U `RPX` in Cemu using a repo-local launch profile.
- Modify: `README.md`
  Purpose: document the editor CLI build flow, Cemu launcher flow, and later `cube_test` verification flow.
- Modify: `Makefile`
  Purpose: compile new Wii U runtime files as slices 2 and 3 land.
- Modify: `src/main.cpp`
  Purpose: keep a thin launcher while shifting runtime responsibility behind the Wii U host seam.
- Create: `src/platform/wiiu/WiiUBootPhase.hpp`
  Purpose: declare explicit Wii U boot-state milestones like the Wii and GameCube repos.
- Create: `src/platform/wiiu/WiiUApplication.hpp`
  Purpose: declare the first Wii U-owned runtime/application seam.
- Create: `src/platform/wiiu/WiiUApplication.cpp`
  Purpose: own the Wii U steady-state boot, update, and present loop beyond the proof-of-life host.
- Create: `src/platform/wiiu/WiiUSceneBootstrap.hpp`
  Purpose: declare the staged and packaged scene bootstrap helpers consumed by the Wii U runtime.
- Create: `src/platform/wiiu/WiiUSceneBootstrap.cpp`
  Purpose: implement the Wii U staged and packaged scene bootstrap path and startup scene selection.
- Modify: `src/platform/wiiu/WiiUBootHost.hpp`
  Purpose: keep the top-level Wii U host explicit while delegating later slices into `WiiUApplication`.
- Modify: `src/platform/wiiu/WiiUBootHost.cpp`
  Purpose: preserve the host entrypoint while routing later runtime work to `WiiUApplication`.
- Create: `builder/WiiURuntimeSceneManifestWriter.cs`
  Purpose: emit the Wii U packaged runtime scene manifest in the same style as the Wii and GameCube builders.
- Modify: `C:\dev\helworks\helengine\user_settings\platforms.json`
  Purpose: register the Wii U builder assembly so the shared editor CLI can resolve `--build wiiu`.

### Task 1: Add The Wii U Builder Assembly And Editor Registration

**Files:**
- Create: `builder/helengine.wiiu.builder.csproj`
- Create: `builder/Program.cs`
- Create: `builder/WiiUPlatformAssetBuilder.cs`
- Create: `builder/WiiUPlatformDefinitionFactory.cs`
- Create: `builder/WiiUBuildWorkspace.cs`
- Create: `builder/WiiUBuilderPaths.cs`
- Create: `builder/IWiiUNativeBuildExecutor.cs`
- Create: `builder/WiiUDockerNativeBuildExecutor.cs`
- Create: `builder.tests/helengine.wiiu.builder.tests.csproj`
- Create: `builder.tests/WiiUPlatformAssetBuilderTests.cs`
- Modify: `C:\dev\helworks\helengine\user_settings\platforms.json`

- [ ] **Step 1: Write the failing Wii U builder metadata and artifact-flow tests**

```csharp
namespace helengine.wiiu.builder.tests;

public sealed class WiiUPlatformAssetBuilderTests {
    [Fact]
    public void DescriptorAndDefinition_ExposeExpectedWiiUMetadata() {
        WiiUPlatformAssetBuilder builder = new();

        Assert.Equal("helengine.wiiu.builder", builder.Descriptor.BuilderId);
        Assert.Equal("wiiu", builder.Descriptor.TargetPlatformId);
        Assert.Contains("wiiu", builder.Descriptor.SupportedRuntimeBackendIds);
        Assert.Equal("wiiu", builder.Definition.PlatformId);
        Assert.Contains(builder.Definition.BuildProfiles, profile => profile.ProfileId == "wiiu-default");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "wiiu-default");
    }

    [Fact]
    public async Task BuildAsync_WhenUsingDefaultFlow_WritesRpxIntoOutputRoot() {
        RecordingWiiUNativeBuildExecutor nativeBuildExecutor = new();
        WiiUPlatformAssetBuilder builder = new(nativeBuildExecutor);
        PlatformBuildRequest request = WiiUTestBuildRequestFactory.CreateDefault();
        RecordingProgressReporter progressReporter = new();
        RecordingDiagnosticReporter diagnosticReporter = new();

        PlatformBuildReport report = await builder.BuildAsync(
            request,
            progressReporter,
            diagnosticReporter,
            CancellationToken.None);

        Assert.True(report.Succeeded);
        Assert.True(File.Exists(Path.Combine(request.OutputRootPath, "helengine_wiiu.rpx")));
        Assert.Contains("helengine_wiiu.rpx", nativeBuildExecutor.LastProducedArtifactPath, StringComparison.Ordinal);
    }

    sealed class RecordingWiiUNativeBuildExecutor : IWiiUNativeBuildExecutor {
        public string LastProducedArtifactPath { get; private set; } = string.Empty;

        public Task<string> BuildAsync(
            PlatformBuildRequest request,
            IPlatformBuildDiagnosticReporter diagnosticReporter,
            CancellationToken cancellationToken) {
            string artifactPath = Path.Combine(request.IntermediateRootPath, "native-build", "helengine_wiiu.rpx");
            Directory.CreateDirectory(Path.GetDirectoryName(artifactPath)!);
            File.WriteAllText(artifactPath, "fake-rpx");
            LastProducedArtifactPath = artifactPath;
            return Task.FromResult(artifactPath);
        }
    }

    static class WiiUTestBuildRequestFactory {
        public static PlatformBuildRequest CreateDefault() {
            string workingRootPath = Path.Combine(Path.GetTempPath(), "wiiu-builder-tests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(workingRootPath);
            return new PlatformBuildRequest(
                new PlatformBuildManifest(1, "project", "1.0.0", "1.0.0", "wiiu", "1.0.0", [], [], [], [], [], new PlatformContainerWritePlan("default", [])),
                [new PlatformBuildTargetVariant("wiiu-default", "wiiu", "wiiu", "wiiu-default")],
                [new PlatformCookProfile("wiiu-default", "Wii U Default", new PlatformCookProfileCapabilities("wiiu", "raw", "rgba", "wiiu-scene-v1", PlatformSerializationEndianness.LittleEndian))],
                Path.Combine(workingRootPath, "out"),
                Path.Combine(workingRootPath, "tmp"));
        }
    }
}
```

- [ ] **Step 2: Run the focused builder tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiUPlatformAssetBuilderTests" --no-restore
```

Expected: FAIL because the Wii U builder project, test project, and `WiiUPlatformAssetBuilder` types do not exist yet.

- [ ] **Step 3: Create the minimal builder assembly and default workspace flow**

```csharp
// builder/WiiUPlatformAssetBuilder.cs
using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.wiiu.builder;

public sealed class WiiUPlatformAssetBuilder : IPlatformAssetBuilder {
    readonly IWiiUNativeBuildExecutor NativeBuildExecutor;

    public WiiUPlatformAssetBuilder()
        : this(new WiiUDockerNativeBuildExecutor()) {
    }

    public WiiUPlatformAssetBuilder(IWiiUNativeBuildExecutor nativeBuildExecutor) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.wiiu.builder",
            "1.0.0",
            "wiiu",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 2),
            ["wiiu"],
            ["wiiu-default"]);
        Definition = WiiUPlatformDefinitionFactory.Create();
    }

    public PlatformBuilderDescriptor Descriptor { get; }

    public PlatformDefinition Definition { get; }

    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        throw new InvalidOperationException("The first Wii U slice does not add Wii U material cooking yet.");
    }

    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        return WiiUBuildWorkspace.BuildAsync(request, progressReporter, diagnosticReporter, cancellationToken, NativeBuildExecutor);
    }
}
```

```csharp
// builder/WiiUBuildWorkspace.cs
namespace helengine.wiiu.builder;

public static class WiiUBuildWorkspace {
    public static async Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken,
        IWiiUNativeBuildExecutor nativeBuildExecutor) {
        Directory.CreateDirectory(request.OutputRootPath);
        string producedArtifactPath = await nativeBuildExecutor.BuildAsync(request, diagnosticReporter, cancellationToken);
        File.Copy(producedArtifactPath, Path.Combine(request.OutputRootPath, WiiUBuilderPaths.RpxFileName), true);
        return PlatformBuildReport.Success(request.Manifest.PlatformId, request.OutputRootPath);
    }
}
```

```json
// C:\dev\helworks\helengine\user_settings\platforms.json
{
  "engineVersion": "1.0.0+13db86b8a91031015e3d0475799b6e6b1a56b309",
  "platformId": "wiiu",
  "displayName": "Nintendo Wii U",
  "builderAssemblyPath": "../../helengine-wiiu/builder/bin/Debug/net9.0-windows/helengine.wiiu.builder.dll",
  "playerSourceRootPath": "../../helengine-wiiu",
  "generatedCoreCppRootPath": "../tmp/helengine-core-cpp-regenerated",
  "codegenToolPath": "../../csharpcodegen/codegen/bin/Debug/net9.0/codegen.exe"
}
```

- [ ] **Step 4: Run the focused builder tests again**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiUPlatformAssetBuilderTests" --no-restore
```

Expected: PASS with the descriptor/definition test green and the default artifact-flow test green.

- [ ] **Step 5: Commit**

```bash
git add builder builder.tests
git commit -m "feat: add wiiu editor build integration"

git -C C:\dev\helworks\helengine add user_settings/platforms.json
git -C C:\dev\helworks\helengine commit -m "feat: register wiiu platform builder"
```

### Task 2: Add The Repo-Local Cemu Launcher And README Workflow

**Files:**
- Create: `scripts/launch_wiiu_rpx_in_cemu.ps1`
- Create: `builder.tests/WiiUCemuLauncherScriptTests.cs`
- Modify: `README.md`

- [ ] **Step 1: Write the failing launcher and README contract tests**

```csharp
namespace helengine.wiiu.builder.tests;

public sealed class WiiUCemuLauncherScriptTests {
    [Fact]
    public void CemuLauncher_KeepsExplicitRpxPathAndDedicatedProfileContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_wiiu_rpx_in_cemu.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_wiiu_rpx_in_cemu.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", scriptSource, StringComparison.Ordinal);
        Assert.Contains("[string]$RpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Cemu'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Item -LiteralPath $resolvedRpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("LastWriteTime", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Cemu.exe", scriptSource, StringComparison.Ordinal);
        Assert.Contains("tmp\\cemu-launcher-user", scriptSource, StringComparison.Ordinal);
        Assert.Contains("'-g', $resolvedRpxPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
    }

    [Fact]
    public void Readme_DocumentsEditorCliBuildAndCemuLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("build-platform.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-Platform wiiu", readmeSource, StringComparison.Ordinal);
        Assert.Contains("launch_wiiu_rpx_in_cemu.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-RpxPath", readmeSource, StringComparison.Ordinal);
        Assert.Contains("Cemu", readmeSource, StringComparison.Ordinal);
        Assert.Contains("process id", readmeSource, StringComparison.OrdinalIgnoreCase);
    }
}
```

- [ ] **Step 2: Run the launcher contract tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiUCemuLauncherScriptTests" --no-restore
```

Expected: FAIL because the Wii U launcher script and README workflow do not exist yet.

- [ ] **Step 3: Add the launcher script and README workflow**

```powershell
# scripts/launch_wiiu_rpx_in_cemu.ps1
param(
    [Parameter(Mandatory = $true)]
    [string]$RpxPath
)

$ErrorActionPreference = 'Stop'

$resolvedRpxPath = [System.IO.Path]::GetFullPath($RpxPath)
if (-not (Test-Path -LiteralPath $resolvedRpxPath)) {
    throw "Wii U RPX was not found: $resolvedRpxPath"
}

$repositoryRootPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cemuPath = 'C:\dev\helworks\emus\cemu-2.6-windows-x64\Cemu_2.6\Cemu.exe'
$userDir = Join-Path $repositoryRootPath 'tmp\cemu-launcher-user'

if (-not (Test-Path -LiteralPath $cemuPath)) {
    throw "Cemu executable was not found: $cemuPath"
}

$existingCemuProcesses = @(Get-Process -Name 'Cemu' -ErrorAction SilentlyContinue)
foreach ($process in $existingCemuProcesses) {
    Stop-Process -Id $process.Id -Force
}

New-Item -ItemType Directory -Force -Path $userDir | Out-Null
$rpxItem = Get-Item -LiteralPath $resolvedRpxPath

Write-Output ("RPX=" + $resolvedRpxPath)
Write-Output ("RPX_LAST_WRITE_TIME=" + $rpxItem.LastWriteTime.ToString('O'))
Write-Output ("CEMU=" + $cemuPath)
Write-Output ("USER_DIR=" + $userDir)

$process = Start-Process -FilePath $cemuPath -ArgumentList '-g', $resolvedRpxPath -WorkingDirectory $userDir -PassThru
Write-Output ("PROCESS_ID=" + $process.Id)
```

```markdown
## Editor CLI build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

## Launching in Cemu

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_wiiu_rpx_in_cemu.ps1 `
  -RpxPath ..\helprojs\city\wiiu-build\helengine_wiiu.rpx
```
```

- [ ] **Step 4: Run the launcher contract tests again**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiUCemuLauncherScriptTests" --no-restore
```

Expected: PASS with the launcher script and README workflow contract green.

- [ ] **Step 5: Commit**

```bash
git add scripts/launch_wiiu_rpx_in_cemu.ps1 builder.tests/WiiUCemuLauncherScriptTests.cs README.md
git commit -m "feat: add wiiu cemu launcher workflow"
```

### Task 3: Introduce The Wii U Runtime Application Seam

**Files:**
- Create: `src/platform/wiiu/WiiUBootPhase.hpp`
- Create: `src/platform/wiiu/WiiUApplication.hpp`
- Create: `src/platform/wiiu/WiiUApplication.cpp`
- Modify: `src/platform/wiiu/WiiUBootHost.hpp`
- Modify: `src/platform/wiiu/WiiUBootHost.cpp`
- Modify: `src/main.cpp`
- Modify: `Makefile`
- Create: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing Wii U runtime source-contract tests**

```csharp
namespace helengine.wiiu.builder.tests;

public sealed class WiiURuntimeSourceTests {
    [Fact]
    public void RuntimeSeam_AddsApplicationBoundaryBehindBootHost() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string bootHostSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUBootHost.cpp"));
        string applicationHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp");
        string applicationSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp");

        Assert.True(File.Exists(applicationHeaderPath), "Expected WiiUApplication.hpp to exist.");
        Assert.True(File.Exists(applicationSourcePath), "Expected WiiUApplication.cpp to exist.");
        Assert.Contains("WiiUApplication.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wiiu/WiiUApplication.hpp\"", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("WiiUApplication application {};", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("return application.Run();", bootHostSource, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 2: Run the runtime source-contract test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: FAIL because the Wii U application seam files and Makefile references do not exist yet.

- [ ] **Step 3: Add the minimal Wii U application seam while preserving the red-screen milestone**

```cpp
// src/platform/wiiu/WiiUBootPhase.hpp
#pragma once

namespace helengine::wiiu {
    enum class WiiUBootPhase {
        NativeStartup,
        VideoInitialization,
        Running,
        Failed
    };
}
```

```cpp
// src/platform/wiiu/WiiUApplication.hpp
#pragma once

#include <cstdint>

#include <coreinit/screen.h>

#include "platform/wiiu/WiiUBootPhase.hpp"

namespace helengine::wiiu {
    class WiiUApplication {
    public:
        WiiUApplication();
        int Run();

    private:
        bool InitializeVideo();
        bool InitializeEngineCore();
        void PresentFrame();
        void SetBootPhase(WiiUBootPhase phase, std::uint32_t color);
    };
}
```

```cpp
// src/platform/wiiu/WiiUBootHost.cpp
#include "platform/wiiu/WiiUBootHost.hpp"
#include "platform/wiiu/WiiUApplication.hpp"

namespace helengine::wiiu {
    int WiiUBootHost::Run() {
        WiiUApplication application {};
        return application.Run();
    }
}
```

- [ ] **Step 4: Run the runtime source-contract test again**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: PASS with the new runtime boundary present and the host still acting as the explicit entry seam.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp src/platform/wiiu/WiiUBootHost.hpp src/platform/wiiu/WiiUBootHost.cpp src/platform/wiiu/WiiUBootPhase.hpp src/platform/wiiu/WiiUApplication.hpp src/platform/wiiu/WiiUApplication.cpp Makefile builder.tests/WiiURuntimeSourceTests.cs
git commit -m "feat: add wiiu runtime application seam"
```

### Task 4: Connect Builder Output To The Wii U Packaged Scene Bootstrap

**Files:**
- Create: `builder/WiiURuntimeSceneManifestWriter.cs`
- Create: `builder.tests/WiiURuntimeSceneManifestWriterTests.cs`
- Create: `src/platform/wiiu/WiiUSceneBootstrap.hpp`
- Create: `src/platform/wiiu/WiiUSceneBootstrap.cpp`
- Modify: `src/platform/wiiu/WiiUApplication.hpp`
- Modify: `src/platform/wiiu/WiiUApplication.cpp`
- Modify: `builder/WiiUBuildWorkspace.cs`
- Modify: `Makefile`
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing manifest-writer and packaged-bootstrap source tests**

```csharp
namespace helengine.wiiu.builder.tests;

public sealed class WiiURuntimeSceneManifestWriterTests {
    [Fact]
    public void Write_EmitsStartupSceneIdAndCookedRelativePaths() {
        string outputRootPath = Path.Combine(Path.GetTempPath(), "wiiu-runtime-manifest-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(outputRootPath);

        try {
            string manifestPath = Path.Combine(outputRootPath, "WiiURuntimeSceneManifest.generated.cpp");
            WiiURuntimeSceneManifestWriter writer = new();

            writer.Write(
                manifestPath,
                "Scenes/rendering/cube_test.helen",
                [new KeyValuePair<string, string>("Scenes/rendering/cube_test.helen", "cooked/scenes/rendering/cube_test.hasset")]);

            string source = File.ReadAllText(manifestPath);
            Assert.Contains("Scenes/rendering/cube_test.helen", source, StringComparison.Ordinal);
            Assert.Contains("cooked/scenes/rendering/cube_test.hasset", source, StringComparison.Ordinal);
        } finally {
            Directory.Delete(outputRootPath, true);
        }
    }
}
```

```csharp
// append to builder.tests/WiiURuntimeSourceTests.cs
[Fact]
public void PackagedBootstrap_DeclaresPackagedSceneHelpersAndRuntimeCalls() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string bootstrapHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.hpp");
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

    Assert.True(File.Exists(bootstrapHeaderPath), "Expected WiiUSceneBootstrap.hpp to exist.");

    string bootstrapHeaderSource = File.ReadAllText(bootstrapHeaderPath);
    Assert.Contains("static std::string GetPackagedContentRootPath();", bootstrapHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static RuntimeSceneCatalog* CreatePackagedSceneCatalog();", bootstrapHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static std::string GetPackagedStartupSceneId();", bootstrapHeaderSource, StringComparison.Ordinal);
    Assert.Contains("WiiUSceneBootstrap::GetPackagedContentRootPath()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("WiiUSceneBootstrap::CreatePackagedSceneCatalog()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("WiiUSceneBootstrap::GetPackagedStartupSceneId()", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the packaged-bootstrap tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSceneManifestWriterTests|FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: FAIL because the Wii U runtime manifest writer and scene bootstrap contract do not exist yet.

- [ ] **Step 3: Add the manifest writer, scene bootstrap, and application wiring**

```csharp
// builder/WiiURuntimeSceneManifestWriter.cs
namespace helengine.wiiu.builder;

public sealed class WiiURuntimeSceneManifestWriter {
    public void Write(string outputPath, string startupSceneId, IReadOnlyList<KeyValuePair<string, string>> sceneEntries) {
        string source = $$"""
        #include <string>

        namespace helengine::wiiu {
            const char* WiiUStartupSceneId = "{{startupSceneId}}";
            const char* WiiUStartupSceneCookedPath = "{{sceneEntries[0].Value}}";
        }
        """;
        File.WriteAllText(outputPath, source);
    }
}
```

```cpp
// src/platform/wiiu/WiiUSceneBootstrap.hpp
#pragma once

#include <string>

class RuntimeSceneCatalog;

namespace helengine::wiiu {
    class WiiUSceneBootstrap {
    public:
        static std::string GetPackagedContentRootPath();
        static RuntimeSceneCatalog* CreatePackagedSceneCatalog();
        static std::string GetPackagedStartupSceneId();
    };
}
```

```cpp
// src/platform/wiiu/WiiUApplication.cpp
#include "platform/wiiu/WiiUSceneBootstrap.hpp"

// src/platform/wiiu/WiiUApplication.cpp
bool WiiUApplication::InitializeEngineCore() {
    std::string packagedContentRootPath = WiiUSceneBootstrap::GetPackagedContentRootPath();
    RuntimeSceneCatalog* packagedCatalog = WiiUSceneBootstrap::CreatePackagedSceneCatalog();
    std::string packagedStartupSceneId = WiiUSceneBootstrap::GetPackagedStartupSceneId();
    return packagedCatalog != nullptr && !packagedContentRootPath.empty() && !packagedStartupSceneId.empty();
}
```

- [ ] **Step 4: Run the packaged-bootstrap tests again**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSceneManifestWriterTests|FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: PASS with the builder-emitted manifest and the Wii U packaged bootstrap source contract green.

- [ ] **Step 5: Commit**

```bash
git add builder/WiiURuntimeSceneManifestWriter.cs builder/WiiUBuildWorkspace.cs builder.tests/WiiURuntimeSceneManifestWriterTests.cs builder.tests/WiiURuntimeSourceTests.cs src/platform/wiiu/WiiUSceneBootstrap.hpp src/platform/wiiu/WiiUSceneBootstrap.cpp src/platform/wiiu/WiiUApplication.hpp src/platform/wiiu/WiiUApplication.cpp Makefile
git commit -m "feat: add wiiu packaged bootstrap wiring"
```

### Task 5: Point The Wii U Bootstrap At The Authored `cube_test` Scene

**Files:**
- Modify: `src/platform/wiiu/WiiUSceneBootstrap.cpp`
- Modify: `README.md`
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing `cube_test` startup contract test**

```csharp
// append to builder.tests/WiiURuntimeSourceTests.cs
[Fact]
public void PackagedBootstrap_UsesCubeTestAsTheAuthoredStartupScene() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));
    string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

    Assert.Contains("Scenes/rendering/cube_test.helen", bootstrapSource, StringComparison.Ordinal);
    Assert.Contains("cooked/scenes/rendering/cube_test.hasset", bootstrapSource, StringComparison.Ordinal);
    Assert.Contains("cube_test", readmeSource, StringComparison.Ordinal);
    Assert.Contains("launch_wiiu_rpx_in_cemu.ps1", readmeSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the `cube_test` startup contract test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: FAIL because the Wii U bootstrap has not yet been pointed at the authored `cube_test` scene.

- [ ] **Step 3: Set the Wii U packaged startup path to `cube_test` and document the final verification flow**

```cpp
// src/platform/wiiu/WiiUSceneBootstrap.cpp
namespace helengine::wiiu {
    namespace {
        std::string StartupSceneId = "Scenes/rendering/cube_test.helen";
        std::string StartupSceneCookedRelativePath = "cooked/scenes/rendering/cube_test.hasset";
    }
}
```

```markdown
## Cube Test Verification

Build the `city` project through the shared editor CLI:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

Launch the resulting RPX in Cemu:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_wiiu_rpx_in_cemu.ps1 `
  -RpxPath ..\helprojs\city\wiiu-build\helengine_wiiu.rpx
```
```

- [ ] **Step 4: Run the `cube_test` startup contract test again**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --filter "FullyQualifiedName~WiiURuntimeSourceTests" --no-restore
```

Expected: PASS with the authored `cube_test` startup contract and README verification flow green.

- [ ] **Step 5: Commit**

```bash
git add src/platform/wiiu/WiiUSceneBootstrap.cpp README.md builder.tests/WiiURuntimeSourceTests.cs
git commit -m "feat: boot wiiu cube test scene"
```
