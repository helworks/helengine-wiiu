namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the first Wii U runtime seam between the boot host entrypoint and the application loop.
/// </summary>
public sealed class WiiURuntimeSourceTests {
    /// <summary>
    /// Ensures the boot host delegates into a dedicated Wii U application boundary and the new source is visible in the build contract.
    /// </summary>
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

    /// <summary>
    /// Ensures the packaged bootstrap exposes explicit packaged scene helpers and the application consumes them through the scene bootstrap boundary.
    /// </summary>
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

    /// <summary>
    /// Ensures the packaged bootstrap fallback points at the authored cube_test scene and the README documents the final verification flow.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesCubeTestAsTheAuthoredStartupScene() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("Scenes/rendering/cube_test.helen", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("cooked/scenes/rendering/cube_test.hasset", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("cube_test", readmeSource, StringComparison.Ordinal);
        Assert.Contains("launch_in_emulator.ps1", readmeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U runtime seam advances the generated core each frame and owns explicit native bridge members.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AdvancesGeneratedCoreWithPlatformBridges() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("HELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION", makefileSource, StringComparison.Ordinal);
        Assert.Contains("bool UpdateEngineCore();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("bool DrawEngineCore();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"Core.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"PlatformInfo.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("RegisterGeneratedRuntimeModules(EngineCore);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!UpdateEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!DrawEngineCore()) {", applicationSource, StringComparison.Ordinal);
    }

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
    /// Ensures the Wii U host no longer keeps the old OSScreen pixel-conversion helper once steady-state rendered frames delegate to the GX2 presenter seam.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RemovesObsoleteOsScreenPixelConversionAfterGx2Presentation() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.DoesNotContain("std::uint32_t ConvertSurfacePixelToScreenColor(std::uint32_t surfacePixel) const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PresentSurface(OSScreenID screen, WiiUSoftwareSurface* surface);", applicationHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("OSScreenPutPixelEx(screen, x, y, ConvertSurfacePixelToScreenColor(pixels[pixelIndex]));", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("OSScreenPutPixelEx(screen, x, y, pixels[pixelIndex]);", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U 2D renderer no longer leaves menu draw requests as empty no-op stubs.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RasterizesMenu2DPrimitivesThroughWiiURenderManager2D() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.hpp"));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));

        Assert.Contains("void Draw() override;", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void AttachSurface(", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Surface->Clear(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitRoundedRect(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitSprite(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitText(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("/// Ignores one rounded-rectangle draw request until the Wii U renderer is implemented.", renderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures silent Wii U boot failures leave behind a host-readable runtime trace across the same initialization boundaries reported through OSReport.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AddsHostReadableRuntimeTraceForSilentBootFailures() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("void AppendRuntimeTrace(const char* format, ...);", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("constexpr const char* RuntimeTracePaths[] = {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("\"sd:/wiiu_runtime_trace.txt\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("\"wiiu_runtime_trace.txt\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("void WiiUApplication::AppendRuntimeTrace(const char* format, ...) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"\\n=== Wii U runtime session %s ===\\n\", BuildStamp);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] InitializeEngineCore begin.\\n\");", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] Packaged content root: %s\\n\", packagedContentRootPath.c_str());", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] Calling EngineCore->Initialize.\\n\");", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] Packaged startup scene queued.\\n\");", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] Engine core initialization threw std::exception stage=%s message=%s\\n\", initializationStage, exception.what());", applicationSource, StringComparison.Ordinal);
    }

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
}
