namespace helengine.wiiu.builder.tests;

/// <summary>
/// Guards the first Wii U runtime seam between the boot host entrypoint and the application loop.
/// </summary>
public sealed class WiiURuntimeSourceTests {
    /// <summary>
    /// Ensures the native host probes both a known scene payload and the shared StandardShader material before engine startup so Wii U bundle lookup failures identify the exact packaged entry.
    /// </summary>
    [Fact]
    public void RuntimeSeam_ProbesPackagedContentBeforeEngineStartup() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("bool ProbePackagedContent();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("bool WiiUApplication::ProbePackagedContent()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("cooked/scenes/helenofcodesplash.hasset", applicationSource, StringComparison.Ordinal);
        Assert.Contains("cooked/engine/materials/standard.hasset", applicationSource, StringComparison.Ordinal);
        Assert.Contains("wiiu_standard_material.hasset", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineContentStreamSource->OpenRead", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ProbePackagedContent", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures ordinary directional lights do not route scenes with no active shadow casters through the shadowed StandardShader path.
    /// </summary>
    [Fact]
    public void RuntimeSeam_OnlyEnablesDirectionalShadowsForActiveShadowCasters() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

        Assert.Contains("directionalShadowState.Strength <= 0.0f", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("shadowCasterSubmissions == nullptr || shadowCasterSubmissions->get_Count() == 0", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("CurrentFrame.SetDirectionalShadow(directionalShadowState);", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures a directional light whose authored shadow toggle is disabled stays on the unshadowed StandardShader route even when its strength and caster list are non-empty.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RespectsAuthoredDirectionalShadowEnablement() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderFrameSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

        Assert.Contains("bool ShadowsEnabled;", renderFrameSource, StringComparison.Ordinal);
        Assert.Contains("directionalLightState.ShadowsEnabled = light->get_ShadowsEnabled();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("if (!CurrentFrame.GetDirectionalLight().ShadowsEnabled", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures presentation failures are routed through the same visible runtime-failure path as update and draw failures.
    /// </summary>
    [Fact]
    public void RuntimeSeam_ReportsPresentationFailures() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("bool PresentFrame();", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("ShowBootFailure(\"PresentFrame\"", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the directional shadow pass uses a dedicated transform buffer instead of synchronizing shared receiver uniforms.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesDedicatedDirectionalShadowTransformBuffer() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("ShadowDepthTransformBuffer", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("RenderDirectionalShadowDepthPass(TvContextState, frame3D);\n            GX2DrawDone();", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures shadowed StandardShader draws establish opaque blend state instead of inheriting state from prior passes.
    /// </summary>
    [Fact]
    public void RuntimeSeam_SetsOpaqueBlendStateForShadowedStandardShaderDraws() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        int shadowedDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer", StringComparison.Ordinal);
        int genericDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer", StringComparison.Ordinal);

        Assert.True(shadowedDrawStart >= 0 && genericDrawStart > shadowedDrawStart, "Expected the shadowed StandardShader draw implementation before the generic 3D draw implementation.");
        string shadowedDrawSource = presenterSource.Substring(shadowedDrawStart, genericDrawStart - shadowedDrawStart);
        Assert.Contains("GX2SetBlendControl(", shadowedDrawSource, StringComparison.Ordinal);
        Assert.Contains("GX2_BLEND_MODE_ONE", shadowedDrawSource, StringComparison.Ordinal);
        Assert.Contains("GX2_BLEND_MODE_ZERO", shadowedDrawSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the shadowed StandardShader binds its four 2D textures in the generated shader's stable sampler-slot order.
    /// </summary>
    [Fact]
    public void RuntimeSeam_BindsShadowedStandardShaderTexturesByGeneratedSamplerSlot() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        int shadowedDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer", StringComparison.Ordinal);
        int genericDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer", StringComparison.Ordinal);

        Assert.True(shadowedDrawStart >= 0 && genericDrawStart > shadowedDrawStart, "Expected the shadowed StandardShader draw implementation before the generic 3D draw implementation.");
        string shadowedDrawSource = presenterSource.Substring(shadowedDrawStart, genericDrawStart - shadowedDrawStart);
        Assert.Contains("samplerVars[0]", shadowedDrawSource, StringComparison.Ordinal);
        Assert.Contains("samplerVars[1]", shadowedDrawSource, StringComparison.Ordinal);
        Assert.Contains("samplerVars[2]", shadowedDrawSource, StringComparison.Ordinal);
        Assert.Contains("samplerVars[3]", shadowedDrawSource, StringComparison.Ordinal);
        Assert.DoesNotContain("samplerVarCount -", shadowedDrawSource, StringComparison.Ordinal);
    }


    /// <summary>
    /// Ensures early Wii U boot failures become visible in Cemu with their failing initialization stage and exception text.
    /// </summary>
    [Fact]
    public void RuntimeSeam_ShowsInitializationFailureDetailsInCemu() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("void ShowBootFailure(const char* stage, const char* message);", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void WiiUApplication::ShowBootFailure(const char* stage, const char* message)", applicationSource, StringComparison.Ordinal);
        Assert.Contains("OSScreenPutFontEx", applicationSource, StringComparison.Ordinal);
        Assert.Contains("while (WHBProcIsRunning())", applicationSource, StringComparison.Ordinal);
        Assert.Contains("InitializeEngineCore", applicationSource, StringComparison.Ordinal);
        int initializePresenterStart = applicationSource.IndexOf("bool WiiUApplication::InitializeGx2Presenter()", StringComparison.Ordinal);
        int initializeCoreStart = applicationSource.IndexOf("bool WiiUApplication::InitializeEngineCore()", StringComparison.Ordinal);
        Assert.True(initializePresenterStart >= 0 && initializeCoreStart > initializePresenterStart, "Expected the GX2 presenter initialization boundary before generated-core initialization.");
        string presenterInitializationSource = applicationSource.Substring(initializePresenterStart, initializeCoreStart - initializePresenterStart);
        Assert.Contains("ShowBootFailure(\"InitializeGx2Presenter\"", presenterInitializationSource, StringComparison.Ordinal);
        Assert.Contains("ShowBootFailure(\"UpdateEngineCore\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ShowBootFailure(\"DrawEngineCore\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("void DrawBootFailureMessage(OSScreenID screen) const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void WiiUApplication::DrawBootFailureMessage(OSScreenID screen) const", applicationSource, StringComparison.Ordinal);
        Assert.Contains("LastRuntimeFailureMessage", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U runtime exposes the frame-owned shadow state and explicit two-pass presenter boundary required by the shared StandardShader directional-shadow path.
    /// </summary>
    [Fact]
    public void RuntimeSeam_DeclaresDirectionalShadowFrameAndPresenterContracts() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderFrameHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("WiiUGx23DDirectionalShadowState", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("LightViewProjection", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("ShadowCasterCommands", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RenderDirectionalShadowDepthPass", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RenderStandard3DDrawCommandToColorBuffer", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("ForwardStandardShadowedShaderGroup", presenterSource, StringComparison.Ordinal);
    }

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
    /// Ensures the Wii U host attaches a generated-core runtime diagnostics provider so managed update-stage callbacks are written into the persistent runtime trace during crash repros.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AttachesRuntimeDiagnosticsProviderToCoreInitialization() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string providerHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeDiagnosticsProvider.hpp");
        string providerSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeDiagnosticsProvider.cpp");
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.True(File.Exists(providerHeaderPath), "Expected WiiURuntimeDiagnosticsProvider.hpp to exist.");
        Assert.True(File.Exists(providerSourcePath), "Expected WiiURuntimeDiagnosticsProvider.cpp to exist.");

        string providerHeaderSource = File.ReadAllText(providerHeaderPath);
        string providerSource = File.ReadAllText(providerSourcePath);
        Assert.Contains("class WiiURuntimeDiagnosticsProvider final : public IRuntimeDiagnosticsProvider, public IRuntimeUpdateStageDiagnosticsProvider", providerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiURuntimeDiagnosticsProvider* EngineRuntimeDiagnosticsProvider;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wiiu/WiiURuntimeDiagnosticsProvider.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineRuntimeDiagnosticsProvider = new WiiURuntimeDiagnosticsProvider();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("initializationOptions->RuntimeDiagnosticsProvider = EngineRuntimeDiagnosticsProvider;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("Managed update stage:", providerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged bootstrap fallback keeps the authored textured_cube_grid seam and the README documents the verification flow.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_KeepsTexturedCubeGridFallbackMetadataVisible() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("Scenes/rendering/textured_cube_grid.helen", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("cooked/scenes/rendering/textured_cube_grid.hasset", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("textured_cube_grid", readmeSource, StringComparison.Ordinal);
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

        Assert.Contains("PresentBootPhaseFrame();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("PresentRenderedFrame();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!EngineInitialized) {", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("WiiUSoftwareSurface", applicationHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("TvSurface = new WiiUSoftwareSurface(", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrcSurface = new WiiUSoftwareSurface(", applicationSource, StringComparison.Ordinal);
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
    public void RuntimeSeam_CapturesMenu2DPrimitivesForPureGx2Presentation() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.hpp"));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));
        string frameHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2RenderFrame.hpp");

        Assert.True(File.Exists(frameHeaderPath), "Expected WiiUGx2RenderFrame.hpp to exist.");
        Assert.Contains("void Draw() override;", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const WiiUGx2RenderFrame& GetCurrentFrame() const;", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiUGx2RenderFrame CurrentFrame;", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitRoundedRect(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitSprite(", renderSource, StringComparison.Ordinal);
        Assert.Contains("SubmitText(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void AttachSurface(", renderHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Surface->Clear(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawSolidQuadToSurface(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawTexturedQuadToSurface(", renderSource, StringComparison.Ordinal);
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
    /// Ensures the Wii U runtime keeps the boot and failure trace path while dropping the temporary bring-up chatter that spammed per-frame and per-draw diagnostics.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RemovesTemporaryBringUpChatterWhileKeepingBootFailureTracing() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

        Assert.Contains("void AppendRuntimeTrace(const char* format, ...);", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("AppendRuntimeTrace(\"[WiiUFile] InitializeEngineCore begin.\\n\");", applicationSource, StringComparison.Ordinal);
        Assert.Contains("AppendInitializationTrace(\"[WiiUFile] GX2 initialize: completed.\\n\");", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("std::uint32_t UpdateFrameLogCount;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("std::uint32_t DrawFrameLogCount;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("OSReport(\"[WiiU] Engine update begin frame=%u\\n\", UpdateFrameLogCount);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("OSReport(\"[WiiU] Engine draw begin frame=%u\\n\", DrawFrameLogCount);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("AppendRuntimeTrace(\"[WiiUFile] Engine update completed frame=%u\\n\", UpdateFrameLogCount);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("AppendRuntimeTrace(\"[WiiUFile] Engine draw completed frame=%u\\n\", DrawFrameLogCount);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("std::uint32_t Scene3DDebugLogCount;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("GX2 presenter render begin drawCommands=%u quadCommands=%u hasCamera=%u", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Opaque draw setup target=%ux%u", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Opaque firstTriangle indices=(%u,%u,%u)", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("SceneOpaque light-block draw submitted target=%ux%u", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("diagnostic square load shader group begin", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("[WiiUModel] positions=%d indices=%u", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures visible Wii U output delegates to dedicated presenter-owned GX2 frame paths instead of issuing OSScreen per-pixel writes inline.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RoutesVisibleOutputThroughDedicatedPresenterOwnedGx2Path() {
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
        string presenterHeaderSource = File.ReadAllText(presenterHeaderPath);
        Assert.Contains("void RenderFrame(const WiiUGx2RenderFrame& frame);", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D);", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Gx2Presenter->RenderFrame(", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("OSScreenPutPixelEx(screen, x, y, ConvertSurfacePixelToScreenColor(pixels[pixelIndex]));", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U 2D bridge builds GPU-backed textures and samplers for steady-state GX2 submission instead of retaining software-renderer pixel buffers.
    /// </summary>
    [Fact]
    public void RuntimeSeam_BuildsGpuBackedTexturesForPureGx2UiRendering() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.hpp"));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));

        Assert.Contains("#include <gx2/texture.h>", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiUGx2TextureHandle Gx2TextureHandle;", renderHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2RCreateSurface(", renderSource, StringComparison.Ordinal);
        Assert.Contains("GX2InitTextureRegs(", renderSource, StringComparison.Ordinal);
        Assert.Contains("GX2InitSampler(", renderSource, StringComparison.Ordinal);
        Assert.Contains("GX2RLockSurfaceEx(", renderSource, StringComparison.Ordinal);
        Assert.Contains("GX2RUnlockSurfaceEx(", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("std::vector<std::uint32_t> Pixels;", renderHeaderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U texture uploader rotates decoded ARGB words into one GX2 RGBA word layout so sampled alpha does not come from the authored red channel.
    /// </summary>
    [Fact]
    public void RuntimeSeam_RotatesDecodedArgbPixelsIntoGx2RgbaWordOrderDuringTextureUpload() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));

        Assert.Contains("textureHandle->Texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;", renderSource, StringComparison.Ordinal);
        Assert.Contains("destinationPixels[(row * destinationPitch) + column] = (sourcePixel << 8U)", renderSource, StringComparison.Ordinal);
        Assert.Contains("| ((sourcePixel >> 24U) & 0x000000FFU);", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("destinationPixels[(row * destinationPitch) + column] = (sourcePixel & 0xFF00FF00U)", renderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the GX2 presenter follows the official context-state initialization path before copying color buffers to the TV and DRC scan buffers.
    /// </summary>
    [Fact]
    public void RuntimeSeam_InitializesGx2CommandBufferAndContextStateForPresentation() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("void* CommandBufferPool;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2ContextState* TvContextState;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2ContextState* DrcContextState;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2_INIT_CMD_BUF_BASE", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2_INIT_CMD_BUF_POOL_SIZE", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetupContextStateEx(TvContextState, TRUE);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetupContextStateEx(DrcContextState, TRUE);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetContextState(TvContextState);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetContextState(DrcContextState);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetColorBuffer(&TvColorBuffer, GX2_RENDER_TARGET_0);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetColorBuffer(&DrcColorBuffer, GX2_RENDER_TARGET_0);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2CopyColorBufferToScanBuffer(&TvColorBuffer, GX2_SCAN_TARGET_TV);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2CopyColorBufferToScanBuffer(&DrcColorBuffer, GX2_SCAN_TARGET_DRC);", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the earlier clear-only bring-up slice remains available as a presenter-owned GX2 diagnostic step even though the active frame path now uses captured 3D plus 2D frames.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesPresenterOwnedPureGx2ClearFrameForVisibleOutput() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("void RenderDiagnosticClearFrame();", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2ClearColor(&TvColorBuffer", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2ClearColor(&DrcColorBuffer", presenterSource, StringComparison.Ordinal);
        Assert.Contains("Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->RenderDiagnosticClearFrame();", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->Present(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (quadCommands.empty()) {", presenterSource, StringComparison.Ordinal);
        Assert.Contains("EnsureUiQuadBufferCapacity(totalVertexCount);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("static_cast<float>(clearColor.Red) / 255.0f", presenterSource, StringComparison.Ordinal);
        Assert.Contains("RenderQuadCommandToColorBuffer(quadCommands[commandIndex], static_cast<std::uint32_t>(commandIndex)", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures steady-state menu presentation uses the authored demo-disc menu background clear color instead of a temporary diagnostic color.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesAuthoredMenuBackgroundClearColorForCapturedMenuFrames() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));

        Assert.Contains("CurrentFrame.SetClearColor(WiiUGx2Color { 30U, 17U, 41U, 255U });", renderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("CurrentFrame.SetClearColor(WiiUGx2Color { 0U, 255U, 0U, 255U });", renderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the second pure GX2 bring-up slice renders a presenter-owned centered square through one real GX2 draw call instead of software-surface upload.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesPresenterOwnedPureGx2SquareFrameForVisibleOutput() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("void RenderDiagnosticSquareFrame();", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void RenderFrame(const WiiUGx2RenderFrame& frame);", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WHBGfxLoadGFDShaderGroup(&DiagnosticSquareShaderGroup, 0, diagnostic_square_shader_bin)", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RCreateBuffer(buffer)", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RSetAttributeBuffer(&DiagnosticSquarePositionBuffer, 0, DiagnosticSquarePositionBuffer.elemSize, 0);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RSetAttributeBuffer(&DiagnosticSquareColorBuffer, 1, DiagnosticSquareColorBuffer.elemSize, 0);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticSquareVertexCount, 0, 1);", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->RenderDiagnosticSquareFrame();", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U host returns to the full engine frame loop after the diagnostic GX2 bring-up slices have landed.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesFullEngineFrameLoopAfter3dShaderBringUp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("enum class DiagnosticFrameLoopMode {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("FullEngine", applicationSource, StringComparison.Ordinal);
        Assert.Contains("constexpr DiagnosticFrameLoopMode DiagnosticFrameLoopModeValue = DiagnosticFrameLoopMode::FullEngine;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::PresentOnly) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::DrawOnly) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!DrawEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!PresentFrame())", applicationSource, StringComparison.Ordinal);
        Assert.Contains("continue;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!UpdateEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("OSSleepTicks(OSMillisecondsToTicks(16));", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the draw-only diagnostic toggle is restored so steady-state builds still submit both Wii U render managers.
    /// </summary>
    [Fact]
    public void RuntimeSeam_FullEngineLoopDoesNotKeepSkipping2DRendererSubmission() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("constexpr bool RunDiagnosticRenderManager2DDrawInDrawOnlyMode = true;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->Draw();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (DiagnosticFrameLoopModeValue != DiagnosticFrameLoopMode::DrawOnly || RunDiagnosticRenderManager2DDrawInDrawOnlyMode) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->Draw();", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U runtime owns a dedicated input backend and passes it into the generated core instead of leaving gameplay with a null input seam.
    /// </summary>
    [Fact]
    public void RuntimeSeam_WiresDedicatedWiiUInputBackendIntoGeneratedCore() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
        string inputHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUInputBackend.hpp");
        string inputSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUInputBackend.cpp");

        Assert.True(File.Exists(inputHeaderPath), "Expected WiiUInputBackend.hpp to exist.");
        Assert.True(File.Exists(inputSourcePath), "Expected WiiUInputBackend.cpp to exist.");
        Assert.Contains("WiiUInputBackend.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("class WiiUInputBackend;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiUInputBackend* EngineInputBackend;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wiiu/WiiUInputBackend.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineInputBackend = new WiiUInputBackend();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, initializationOptions);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, nullptr, EnginePlatformInfo, initializationOptions);", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U host assigns one generated-core content stream source into initialization options instead of using the removed content-root-path seam.
    /// </summary>
    [Fact]
    public void RuntimeSeam_WiresHostFileSystemContentStreamSourceIntoCoreInitialization() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("class HostFileSystemContentStreamSource;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("HostFileSystemContentStreamSource* EngineContentStreamSource;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"HostFileSystemContentStreamSource.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("CoreInitializationOptions* initializationOptions = new CoreInitializationOptions();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineContentStreamSource = new HostFileSystemContentStreamSource(packagedContentRootPath);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("initializationOptions->ContentStreamSource = EngineContentStreamSource;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore = new Core(initializationOptions);", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("EngineCore = new Core();", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("EngineCore->get_InitializationOptions()", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("initializationOptions->ContentRootPath = packagedContentRootPath;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("delete EngineContentStreamSource;", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures each Wii U input capture returns a fresh gamepad snapshot so previous-frame edge detection does not alias the current-frame state.
    /// </summary>
    [Fact]
    public void RuntimeSeam_CapturesFreshGamepadSnapshotsForButtonEdgeDetection() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string inputHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUInputBackend.hpp"));
        string inputSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUInputBackend.cpp"));

        Assert.DoesNotContain("::InputFrameState FrameState;", inputHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Array<::InputGamepadState>* GamepadStates;", inputHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr std::uint32_t SnapshotBufferCount = 2U;", inputHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::uint32_t ActiveSnapshotIndex;", inputHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<::InputGamepadState>* GamepadStateSnapshots[SnapshotBufferCount];", inputHeaderSource, StringComparison.Ordinal);
        Assert.Contains("InputFrameState frameState {};", inputSource, StringComparison.Ordinal);
        Assert.Contains("ActiveSnapshotIndex = (ActiveSnapshotIndex + 1U) % SnapshotBufferCount;", inputSource, StringComparison.Ordinal);
        Assert.Contains("Array<InputGamepadState>* gamepadStates = GamepadStateSnapshots[ActiveSnapshotIndex];", inputSource, StringComparison.Ordinal);
        Assert.Contains("frameState.set_Gamepads(gamepadStates);", inputSource, StringComparison.Ordinal);
        Assert.Contains("(*gamepadStates)[0] = gamepadState;", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.set_LeftStickX(static_cast<int16_t>(status.leftStick.x * 32767.0f));", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.set_LeftStickY(static_cast<int16_t>(-status.leftStick.y * 32767.0f));", inputSource, StringComparison.Ordinal);
        Assert.DoesNotContain("FrameState.set_Gamepads(GamepadStates);", inputSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(*GamepadStates)[0] = gamepadState;", inputSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U host loads the generated standard-platform input manifest and assigns it into core initialization so Accept and Return actions reach gameplay code.
    /// </summary>
    [Fact]
    public void RuntimeSeam_LoadsGeneratedStandardPlatformInputConfigurationIntoCoreInitialization() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("StandardPlatformInputConfiguration* CreateStandardPlatformInputConfiguration() const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformAction.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformActionBinding.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformInputConfiguration.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputControlId.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputControlKind.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputDeviceKind.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/native_list.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_standard_platform_input_manifest.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("initializationOptions->StandardPlatformInputConfiguration = CreateStandardPlatformInputConfiguration();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("StandardPlatformInputConfiguration* WiiUApplication::CreateStandardPlatformInputConfiguration() const {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("const HERuntimeStandardPlatformActionEntry* actionEntries = he_runtime_standard_platform_action_entries(&actionEntryCount);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("List<StandardPlatformActionBinding*>* bindings = new List<StandardPlatformActionBinding*>();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("bindings->Add(new StandardPlatformActionBinding(", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the first Wii U 3D bring-up slice still exists as an optional presenter-owned diagnostic triangle path through offline-compiled GX2 shaders.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleFrameForFirst3dShaderBringUp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
        string shaderVertexPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_triangle.vs");
        string shaderPixelPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_triangle.ps");
        string shaderBinaryPath = Path.Combine(repositoryRootPath, "data", "diagnostic_triangle_shader.bin");

        Assert.True(File.Exists(shaderVertexPath), "Expected diagnostic_triangle.vs to exist.");
        Assert.True(File.Exists(shaderPixelPath), "Expected diagnostic_triangle.ps to exist.");
        Assert.True(File.Exists(shaderBinaryPath), "Expected diagnostic_triangle_shader.bin to exist.");
        Assert.Contains("void RenderDiagnosticTriangleFrame();", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"diagnostic_triangle_shader_bin.h\"", presenterSource, StringComparison.Ordinal);
        Assert.Contains("WHBGfxLoadGFDShaderGroup(&DiagnosticTriangleShaderGroup, 0, diagnostic_triangle_shader_bin)", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticTriangleVertexCount, 0, 1);", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->RenderDiagnosticTriangleFrame();", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the next Wii U 3D bring-up slice can keep the diagnostic triangle translated through dedicated presenter-owned shader and vertex resources.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesPresenterOwnedDiagnosticTriangleTransformBufferForTranslated3dBringUp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string shaderVertexSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_triangle.vs"));

        Assert.Contains("gl_Position = aPosition;", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("0.25f, 0.85f, 0.0f, 1.0f", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RBuffer DiagnosticTriangleTransformBuffer;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("InitializeDiagnosticTriangleTransformBuffer", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RSetVertexUniformBlock", presenterSource, StringComparison.Ordinal);
        Assert.Contains("DiagnosticTriangleTransformBuffer", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the steady-state Wii U runtime presents one captured 3D frame plus the captured 2D overlay instead of calling the presenter-owned scene cube shortcut.
    /// </summary>
    [Fact]
    public void RuntimeSeam_PresentsCapturedSceneDriven3dFrameThroughGenericGx2PresenterPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
        string renderFrameHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));

        Assert.Contains("void RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D);", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const WiiUGx23DRenderFrame& GetCurrentFrame() const;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void Draw() override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("class WiiUGx23DRenderFrame {", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel);", presenterHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void RenderSceneCubeFrame();", presenterHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->RenderSceneCubeFrame();", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->ConfigureSceneCubeMesh(", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("scene_cube_flat_color_shader_bin.h", presenterSource, StringComparison.Ordinal);
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

    /// <summary>
    /// Ensures the generated-core Wii U startup scene comes from the generated runtime manifest instead of one hardcoded bootstrap scene id.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesGeneratedRuntimeManifestStartupSceneId() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));

        Assert.Contains("return he_get_runtime_wiiu_startup_scene_id();", bootstrapSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return \"textured_cube_grid\";", bootstrapSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the steady-state Wii U 3D presenter computes one scene-driven perspective transform and binds the opaque-scene uniform blocks before drawing.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesSceneDrivenPerspectiveCameraForCaptured3dFrames() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string shaderVertexSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.vs"));
        string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps"));

        Assert.Contains("constexpr double SceneDrivenFieldOfViewRadians =", presenterSource, StringComparison.Ordinal);
        Assert.Contains("Render3DDrawCommandToColorBuffer(", presenterSource, StringComparison.Ordinal);
        Assert.Contains("float4x4::CreatePerspectiveFieldOfView__out4(", presenterSource, StringComparison.Ordinal);
        Assert.Contains("UploadSceneOpaqueMeshClipSpace(*drawCommand.RuntimeModel, worldMatrix, worldViewProjectionMatrix);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelUniformBlock(", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("UploadSceneCubeMesh(*drawCommand.RuntimeModel, worldViewProjectionMatrix);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2DrawDone();", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("uniform TransformBlock", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("gl_Position = aPosition;", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("uniform LightBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("vec4 sampledBaseColor = texture(BaseColorTexture, VertexTexCoord);", shaderPixelSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures normal Wii U builds regenerate runtime shader blobs from checked-in GLSL sources instead of depending on hand-managed prebuilt binaries.
    /// </summary>
    [Fact]
    public void RuntimeSeam_BuildsWiiUShadersFromAuthoritativeGlslSources() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string diagnosticSquareVertexShaderPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_square.vs");
        string diagnosticSquarePixelShaderPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "diagnostic_square.ps");
        string sceneOpaqueLitVertexShaderPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.vs");
        string sceneOpaqueLitPixelShaderPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps");

        Assert.Contains("tools/wiiu-shaders", makefileSource, StringComparison.Ordinal);
        Assert.Contains("scene_opaque_lit_shader.bin", makefileSource, StringComparison.Ordinal);
        Assert.Contains("diagnostic_square_shader.bin", makefileSource, StringComparison.Ordinal);
        Assert.DoesNotContain("scene_cube_flat_color_shader.bin", makefileSource, StringComparison.Ordinal);
        Assert.True(File.Exists(diagnosticSquareVertexShaderPath), "Expected diagnostic_square.vs to exist.");
        Assert.True(File.Exists(diagnosticSquarePixelShaderPath), "Expected diagnostic_square.ps to exist.");
        Assert.True(File.Exists(sceneOpaqueLitVertexShaderPath), "Expected scene_opaque_lit.vs to exist.");
        Assert.True(File.Exists(sceneOpaqueLitPixelShaderPath), "Expected scene_opaque_lit.ps to exist.");
    }

    /// <summary>
    /// Ensures the Wii U scene-capture bridge carries concrete runtime materials, copied normals plus UVs, and frame-level ambient plus directional light state.
    /// </summary>
    [Fact]
    public void RuntimeSeam_CapturesOpaqueMaterialsAndSceneLightsIntoWiiU3dFrame() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string runtimeModelHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeModel.hpp"));
        string renderFrameHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx23DRenderFrame.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
        string runtimeMaterialHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeMaterial.hpp");

        Assert.True(File.Exists(runtimeMaterialHeaderPath), "Expected WiiURuntimeMaterial.hpp to exist.");
        string runtimeMaterialHeaderSource = File.ReadAllText(runtimeMaterialHeaderPath);

        Assert.Contains("class WiiURuntimeMaterial final : public ::RuntimeMaterial", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void SetGeometry(std::vector<float> positionData, std::vector<float> normalData, std::vector<float> texCoordData, std::vector<std::uint16_t> indexData);", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const std::vector<float>& GetNormalData() const;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const std::vector<float>& GetTexCoordData() const;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wiiu/WiiUGx2TextureHandle.hpp\"", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const WiiUGx2TextureHandle* GetBaseColorTextureHandle() const", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const WiiURuntimeMaterial* RuntimeMaterial;", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("float4 AmbientLightColor;", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("bool HasDirectionalLightState;", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("struct WiiUGx23DDirectionalLightState {", renderFrameHeaderSource, StringComparison.Ordinal);
        Assert.Contains("submission->get_Material()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("drawCommand.RuntimeMaterial = runtimeMaterial;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("data->TexCoords", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("texCoordData.push_back(texCoord.X);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("texCoordData.push_back(texCoord.Y);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("CurrentFrame.SetAmbientLightColor(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("CurrentFrame.SetDirectionalLight(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("CaptureSceneLighting(frame);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("frame->get_LightSubmissions()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("lightSubmission->get_Importance()", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures cooked Wii U materials resolve one base-color texture path into a GX2 texture handle owned by the runtime material.
    /// </summary>
    [Fact]
    public void RuntimeSeam_BuildsBaseColorTextureHandlesForCookedOpaqueMaterials() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

        Assert.Contains("void ReleaseMaterial(::RuntimeMaterial* material) override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("TextureAsset* textureAsset = he_cpp_try_cast<TextureAsset>(asset);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("materialAsset->TextureRelativePath", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->SetBaseColorTextureHandle(textureHandle);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("BuildTextureHandleFromCooked(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ReleaseTransientTextureAsset(textureAsset);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("DestroyTextureHandle(runtimeMaterial->GetBaseColorTextureHandleStorage());", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current incremental Wii U opaque-scene shader slice binds GPU material and light blocks for opaque Lambert-lit draws.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesMaterialAndLightBlocksForCurrentOpaqueSceneSlice() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps"));

        Assert.Contains("InitializeSceneOpaqueMaterialBuffer();", presenterSource, StringComparison.Ordinal);
        Assert.Contains("InitializeSceneOpaqueLightBuffer();", presenterSource, StringComparison.Ordinal);
        Assert.Contains("drawCommand.RuntimeMaterial", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelUniformBlock(", presenterSource, StringComparison.Ordinal);
        Assert.Contains("frame.GetAmbientLightColor()", presenterSource, StringComparison.Ordinal);
        Assert.Contains("frame.GetHasDirectionalLight()", presenterSource, StringComparison.Ordinal);
        Assert.Contains("uniform MaterialBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("uniform LightBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("layout(std140, binding = 0) uniform MaterialBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("layout(std140, binding = 1) uniform LightBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("BaseColor", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("DirectionalLightColor", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("DirectionalLightDirection", shaderPixelSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current opaque-scene GX2 slice binds texcoords and one sampled base-color texture before drawing textured cubes.
    /// </summary>
    [Fact]
    public void RuntimeSeam_BindsTexCoordsAndBaseColorTextureForCurrentOpaqueSceneSlice() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string shaderVertexSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.vs"));
        string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps"));

        Assert.Contains("GX2RBuffer SceneOpaqueTexCoordBuffer;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WHBGfxInitShaderAttribute(&SceneOpaqueShaderGroup, \"aTexCoord\", 2, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)", presenterSource, StringComparison.Ordinal);
        Assert.Contains("const std::vector<float>& sourceTexCoordData = runtimeModel.GetTexCoordData();", presenterSource, StringComparison.Ordinal);
        Assert.Contains("expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 0U]);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 1U]);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RSetAttributeBuffer(&SceneOpaqueTexCoordBuffer, 2, SceneOpaqueTexCoordBuffer.elemSize, 0);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelTexture(&baseColorTextureHandle->Texture", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelSampler(&baseColorTextureHandle->Sampler", presenterSource, StringComparison.Ordinal);
        Assert.Contains("layout(location = 2) in vec2 aTexCoord;", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("layout(location = 1) out vec2 VertexTexCoord;", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("VertexTexCoord = aTexCoord;", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("layout(binding = 0) uniform sampler2D BaseColorTexture;", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("layout(location = 1) in vec2 VertexTexCoord;", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("vec4 sampledBaseColor = texture(BaseColorTexture, VertexTexCoord);", shaderPixelSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current Wii U light-block upload stores float payloads in explicit little-endian byte order for GX2 uniform consumption.
    /// </summary>
    [Fact]
    public void RuntimeSeam_StoresSceneOpaqueLightBlockInLittleEndianByteOrder() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("StoreFloatArrayAsLittleEndian(", presenterSource, StringComparison.Ordinal);
        Assert.Contains("StoreFloatArrayAsLittleEndian(lightUploadBuffer, lightData, sizeof(lightData) / sizeof(lightData[0]));", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelUniformBlock(", presenterSource, StringComparison.Ordinal);
        Assert.Contains("StoreFloatArrayAsLittleEndian(materialUploadBuffer, materialData, sizeof(materialData) / sizeof(materialData[0]));", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current opaque-scene clip-space upload fence waits for prior GX2 draws before recycling shared geometry buffers across multiple draw commands.
    /// </summary>
    [Fact]
    public void RuntimeSeam_WaitsForPriorOpaqueDrawBeforeRecyclingSharedGeometryBuffers() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        int functionStart = presenterSource.IndexOf("void WiiUGx2Presenter::UploadSceneOpaqueMeshClipSpace(", StringComparison.Ordinal);
        int nextFunctionStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderQuadCommandToColorBuffer(", StringComparison.Ordinal);

        Assert.True(functionStart >= 0, "Expected the clip-space opaque upload function to exist.");
        Assert.True(nextFunctionStart > functionStart, "Expected the next presenter helper to appear after the clip-space opaque upload function.");

        string clipSpaceUploadSource = presenterSource.Substring(functionStart, nextFunctionStart - functionStart);
        Assert.Contains("if (SceneOpaquePositionBuffer.buffer != nullptr) {", clipSpaceUploadSource, StringComparison.Ordinal);
        Assert.Contains("GX2DrawDone();", clipSpaceUploadSource, StringComparison.Ordinal);
        Assert.Contains("GX2RDestroyBufferEx(&SceneOpaquePositionBuffer, NoGx2rResourceFlags);", clipSpaceUploadSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures StandardShader and directional-shadow draws finish using shared model-space geometry buffers before the next mesh upload destroys those buffers.
    /// </summary>
    [Fact]
    public void RuntimeSeam_WaitsForPriorStandardShaderDrawBeforeRecyclingSharedGeometryBuffers() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        int functionStart = presenterSource.IndexOf("void WiiUGx2Presenter::UploadSceneOpaqueMesh(", StringComparison.Ordinal);
        int nextFunctionStart = presenterSource.IndexOf("void WiiUGx2Presenter::UploadSceneOpaqueMeshClipSpace(", StringComparison.Ordinal);

        Assert.True(functionStart >= 0, "Expected the model-space opaque upload function to exist.");
        Assert.True(nextFunctionStart > functionStart, "Expected the clip-space uploader to appear after the model-space uploader.");

        string modelSpaceUploadSource = presenterSource.Substring(functionStart, nextFunctionStart - functionStart);
        int drawFenceIndex = modelSpaceUploadSource.IndexOf("GX2DrawDone();", StringComparison.Ordinal);
        int firstDestroyIndex = modelSpaceUploadSource.IndexOf("GX2RDestroyBufferEx(&SceneOpaquePositionBuffer, NoGx2rResourceFlags);", StringComparison.Ordinal);

        Assert.True(drawFenceIndex >= 0, "Expected model-space geometry recycling to wait for prior GX2 draws.");
        Assert.True(firstDestroyIndex > drawFenceIndex, "Expected the GX2 draw fence before the first shared geometry buffer is destroyed.");
    }

    /// <summary>
    /// Ensures the current opaque-scene draw path waits for prior GX2 draws before rewriting the shared material and light uniform blocks across multiple draw commands.
    /// </summary>
    [Fact]
    public void RuntimeSeam_WaitsForPriorOpaqueDrawBeforeRewritingSharedMaterialAndLightBlocks() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        int functionStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer(", StringComparison.Ordinal);
        int nextFunctionStart = presenterSource.IndexOf("void WiiUGx2Presenter::UploadSceneOpaqueMesh(", StringComparison.Ordinal);

        Assert.True(functionStart >= 0, "Expected the 3D opaque draw function to exist.");
        Assert.True(nextFunctionStart > functionStart, "Expected the next opaque upload helper to appear after the 3D opaque draw function.");

        string drawCommandSource = presenterSource.Substring(functionStart, nextFunctionStart - functionStart);
        Assert.Contains("GX2RLockBufferEx(&SceneOpaqueMaterialBuffer, NoGx2rResourceFlags);", drawCommandSource, StringComparison.Ordinal);
        Assert.Contains("GX2RLockBufferEx(&SceneOpaqueLightBuffer, NoGx2rResourceFlags);", drawCommandSource, StringComparison.Ordinal);
        Assert.Contains("GX2DrawDone();", drawCommandSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current opaque-scene shader uses the working light block for ambient plus Lambert directional lighting instead of a flat diagnostic color.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesAmbientAndDirectionalLambertLightingForCurrentOpaqueSceneSlice() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string shaderPixelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps"));

        Assert.Contains("normalize(VertexNormal)", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("normalize(-DirectionalLightDirection.xyz)", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("dot(surfaceNormal, lightDirection)", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("AmbientLightColor.xyz", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("BaseColor.rgb * litColor", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("DirectionalLightColor.xyz * diffuseStrength", shaderPixelSource, StringComparison.Ordinal);
        Assert.DoesNotContain("FragColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);", shaderPixelSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the current Wii U material upload path uses the runtime material base color instead of a temporary debug override.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesRuntimeMaterialBaseColorForMaterialUpload() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("runtimeMaterial.GetBaseColor().X, runtimeMaterial.GetBaseColor().Y, runtimeMaterial.GetBaseColor().Z, runtimeMaterial.GetBaseColor().W", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("const float4 debugTintColor(0.2f, 0.8f, 0.35f, 1.0f);", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U presenter binds one GPU transform path plus material and light uniform blocks instead of expanding captured scene geometry to clip space on the CPU.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesGpuTransformsAndOpaqueLitUniformUploadsForSceneDraws() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        string shaderVertexPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.vs");
        string shaderPixelPath = Path.Combine(repositoryRootPath, "tools", "wiiu-shaders", "scene_opaque_lit.ps");

        Assert.True(File.Exists(shaderVertexPath), "Expected scene_opaque_lit.vs to exist.");
        Assert.True(File.Exists(shaderPixelPath), "Expected scene_opaque_lit.ps to exist.");
        string shaderVertexSource = File.ReadAllText(shaderVertexPath);
        string shaderPixelSource = File.ReadAllText(shaderPixelPath);

        Assert.Contains("#include \"scene_opaque_lit_shader_bin.h\"", presenterSource, StringComparison.Ordinal);
        Assert.Contains("GX2RBuffer SceneOpaqueTransformBuffer;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2RBuffer SceneOpaqueMaterialBuffer;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2RBuffer SceneOpaqueLightBuffer;", presenterHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelUniformBlock", presenterSource, StringComparison.Ordinal);
        Assert.Contains("drawCommand.RuntimeMaterial", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("UploadSceneCubeMesh(*drawCommand.RuntimeModel, worldViewProjectionMatrix);", presenterSource, StringComparison.Ordinal);
        Assert.Contains("expandedPositionData.push_back(clipX);", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("uniform TransformBlock", shaderVertexSource, StringComparison.Ordinal);
        Assert.Contains("uniform MaterialBlock", shaderPixelSource, StringComparison.Ordinal);
        Assert.Contains("uniform LightBlock", shaderPixelSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii U renderer bridge exposes the current content-stream-based cooked asset signatures while retaining the legacy path-based seam for stale generated-core workspaces.
    /// </summary>
    [Fact]
    public void RuntimeSeam_AcceptsContentStreamSourceAcrossCookedWiiURuntimeAssetBuilds() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManager2DHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.hpp"));
        string renderManager2DSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager2D.cpp"));
        string renderManager3DHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.hpp"));
        string renderManager3DSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));

        Assert.Contains("class IContentStreamSource;", renderManager2DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);", renderManager2DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeTexture* WiiURenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {", renderManager2DSource, StringComparison.Ordinal);
        Assert.Contains("::Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", renderManager2DSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return BuildTextureFromCooked(cookedAssetPath);", renderManager2DSource, StringComparison.Ordinal);
        Assert.Contains("class IContentStreamSource;", renderManager3DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);", renderManager3DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath);", renderManager3DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);", renderManager3DHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("::Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("Asset* asset = AssetSerializer::Deserialize(stream);", renderManager3DSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return BuildMaterialFromCooked(cookedAssetPath);", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath) {", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("return BuildMaterialFromRawAsset(assetContentManager, materialAssetPath);", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeModel* WiiURenderManager3D::BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {", renderManager3DSource, StringComparison.Ordinal);
        Assert.Contains("::Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", renderManager3DSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return BuildModelFromCooked(cookedAssetPath);", renderManager3DSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the native Wii U renderer recognizes the versioned StandardShader material payload before reopening legacy cooked assets for generated-core deserialization.
    /// </summary>
    [Fact]
    public void RuntimeSeam_ReadsVersionedStandardShaderMaterialBeforeLegacyFallback() {
        string repositoryRootPath = WiiUTestSourcePaths.ResolveRepositoryRootPath();
        string readerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUStandardShaderMaterialReader.hpp"));
        string readerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUStandardShaderMaterialReader.cpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
        int pathReaderMethodStart = readerSource.IndexOf("bool WiiUStandardShaderMaterialReader::TryRead(const std::string& path", StringComparison.Ordinal);
        int streamReaderMethodStart = readerSource.IndexOf("bool WiiUStandardShaderMaterialReader::TryRead(::Stream* stream", StringComparison.Ordinal);
        int readerHelperStart = readerSource.IndexOf("bool WiiUStandardShaderMaterialReader::TryReadExact", StringComparison.Ordinal);
        int pathMaterialMethodStart = renderManagerSource.IndexOf("::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {", StringComparison.Ordinal);
        int streamMaterialMethodStart = renderManagerSource.IndexOf("::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {", StringComparison.Ordinal);
        int rawMaterialMethodStart = renderManagerSource.IndexOf("::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset", streamMaterialMethodStart, StringComparison.Ordinal);

        Assert.True(pathReaderMethodStart >= 0 && streamReaderMethodStart > pathReaderMethodStart, "Expected the path reader before the stream reader.");
        Assert.True(readerHelperStart > streamReaderMethodStart, "Expected the complete stream reader before its primitive helpers.");
        Assert.True(pathMaterialMethodStart >= 0 && streamMaterialMethodStart > pathMaterialMethodStart, "Expected distinct path and content-stream material overloads.");
        Assert.True(rawMaterialMethodStart > streamMaterialMethodStart, "Expected the content-stream material overload before raw material loading.");
        string pathReaderMethodSource = readerSource.Substring(pathReaderMethodStart, streamReaderMethodStart - pathReaderMethodStart);
        string streamReaderMethodSource = readerSource.Substring(streamReaderMethodStart, readerHelperStart - streamReaderMethodStart);
        string pathMaterialMethodSource = renderManagerSource.Substring(pathMaterialMethodStart, streamMaterialMethodStart - pathMaterialMethodStart);
        string streamMaterialMethodSource = renderManagerSource.Substring(streamMaterialMethodStart, rawMaterialMethodStart - streamMaterialMethodStart);

        Assert.Contains("struct WiiUStandardShaderMaterial final", readerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool TryRead(::Stream* stream, WiiUStandardShaderMaterial& material);", readerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("{ 'W', 'U', 'M', 'T' }", readerSource, StringComparison.Ordinal);
        Assert.Contains("MaterialPayloadVersion = 1U", readerSource, StringComparison.Ordinal);
        Assert.Contains("TryReadFloat", readerSource, StringComparison.Ordinal);
        Assert.Contains("FileStream* stream = File::OpenRead(path.c_str());", pathReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("for (std::size_t magicIndex = 0U; magicIndex < sizeof(MaterialPayloadMagic); magicIndex++)", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("TryReadExact(stream, &magicByte, 1U)", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("magicByte != MaterialPayloadMagic[magicIndex]", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("The Wii U StandardShader material signature is truncated.", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("if (TryReadExact(stream, &trailingByte, 1U))", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("The Wii U StandardShader material payload contains trailing data.", streamReaderMethodSource, StringComparison.Ordinal);
        Assert.Contains("WiiUStandardShaderMaterialReader::TryRead(cookedAssetPath, standardShaderMaterial)", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("FileStream* stream = File::OpenRead(cookedAssetPath.c_str());", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("Asset* asset = AssetSerializer::Deserialize(stream);", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("auto runtimeMaterialGuard = he_cpp_make_scope_exit", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("ReleaseMaterial(runtimeMaterial);", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial = nullptr;", pathMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("::Stream* probeStream = contentStreamSource->OpenRead(cookedAssetPath);", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("WiiUStandardShaderMaterialReader::TryRead(probeStream, standardShaderMaterial)", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("::Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("Asset* asset = AssetSerializer::Deserialize(stream);", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("auto runtimeMaterialGuard = he_cpp_make_scope_exit", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("ReleaseMaterial(runtimeMaterial);", streamMaterialMethodSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial = nullptr;", streamMaterialMethodSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that authored StandardShader surface parameters flow through the Wii U runtime material and material manager into the reflected presenter uniform blocks.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UploadsAuthoredStandardShaderMaterialParameters() {
        string repositoryRootPath = WiiUTestSourcePaths.ResolveRepositoryRootPath();
        string runtimeMaterialHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURuntimeMaterial.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiURenderManager3D.cpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        int standardDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer", StringComparison.Ordinal);
        int genericDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer", StringComparison.Ordinal);

        Assert.True(standardDrawStart >= 0 && genericDrawStart > standardDrawStart, "Expected the generated StandardShader draw before the generic 3D draw implementation.");
        string standardDrawSource = presenterSource.Substring(standardDrawStart, genericDrawStart - standardDrawStart);

        Assert.Contains("void SetRoughness(float roughness)", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("float GetRoughness() const", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void SetMetallic(float metallic)", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("float GetMetallic() const", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void SetSpecular(float specular)", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("float GetSpecular() const", runtimeMaterialHeaderSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->SetRoughness(roughness);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->SetMetallic(metallic);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->SetSpecular(specular);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("const float roughnessData[] = { runtimeMaterial.GetRoughness(), 0.0f, 0.0f, 0.0f };", standardDrawSource, StringComparison.Ordinal);
        Assert.Contains("const float metallicData[] = { runtimeMaterial.GetMetallic(), 0.0f, 0.0f, 0.0f };", standardDrawSource, StringComparison.Ordinal);
        Assert.Contains("const float specularData[] = { runtimeMaterial.GetSpecular(), 0.0f, 0.0f, 0.0f };", standardDrawSource, StringComparison.Ordinal);
        Assert.DoesNotContain("const float roughnessData[] = { 1.0f", standardDrawSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that frames without directional shadows use the generated unshadowed StandardShader without binding the directional depth texture.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesGeneratedStandardShaderForNonShadowedFrames() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));
        int dispatchStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DFrameToColorBuffer", StringComparison.Ordinal);
        int quadRouteStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderQuadCommandsToColorBuffer", StringComparison.Ordinal);
        int standardDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer", StringComparison.Ordinal);
        int legacyDrawStart = presenterSource.IndexOf("void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer", StringComparison.Ordinal);

        Assert.True(dispatchStart >= 0 && quadRouteStart > dispatchStart, "Expected the 3D frame dispatcher before the 2D route.");
        Assert.True(standardDrawStart >= 0 && legacyDrawStart > standardDrawStart, "Expected the generated StandardShader draw before the legacy diagnostic draw.");
        string dispatchSource = presenterSource.Substring(dispatchStart, quadRouteStart - dispatchStart);
        string standardDrawSource = presenterSource.Substring(standardDrawStart, legacyDrawStart - standardDrawStart);
        Assert.Contains("&ForwardStandardShaderGroup", dispatchSource, StringComparison.Ordinal);
        Assert.Contains("&ForwardStandardShadowedShaderGroup", dispatchSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Render3DDrawCommandToColorBuffer(drawCommands[commandIndex]", dispatchSource, StringComparison.Ordinal);
        Assert.Contains("directionalShadowsEnabled ? 1.0f : 0.0f", standardDrawSource, StringComparison.Ordinal);
        Assert.Contains("if (directionalShadowsEnabled)", standardDrawSource, StringComparison.Ordinal);
        Assert.Contains("GX2SetPixelTexture(&DirectionalShadowTexture", standardDrawSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the Wii U StandardShader payload follows the transposed matrix layout shared by DirectX and Vulkan renderers.
    /// </summary>
    [Fact]
    public void RuntimeSeam_TransposesStandardShaderMatricesAndKeepsDirectionalShadowsEnabled() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("drawCommand.WorldMatrix.M11, drawCommand.WorldMatrix.M21, drawCommand.WorldMatrix.M31, drawCommand.WorldMatrix.M41", presenterSource, StringComparison.Ordinal);
        Assert.Contains("worldViewProjectionMatrix.M11, worldViewProjectionMatrix.M21, worldViewProjectionMatrix.M31, worldViewProjectionMatrix.M41", presenterSource, StringComparison.Ordinal);
        Assert.Contains("lightWorldViewProjection.M11, lightWorldViewProjection.M21, lightWorldViewProjection.M31, lightWorldViewProjection.M41", presenterSource, StringComparison.Ordinal);
        Assert.Contains("shadowMatrix.M11, shadowMatrix.M21, shadowMatrix.M31, shadowMatrix.M41", presenterSource, StringComparison.Ordinal);
        Assert.Contains("float shadowData[100] = {};", presenterSource, StringComparison.Ordinal);
        Assert.Contains("const float enabledShadowData[] = { directionalShadowsEnabled ? 1.0f : 0.0f,", presenterSource, StringComparison.Ordinal);
        Assert.Contains("if (frame3D.GetHasDirectionalShadow())", presenterSource, StringComparison.Ordinal);
        Assert.Contains("WHBGfxShaderGroup* standardShaderGroup = frame.GetHasDirectionalShadow()", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("UseDirectionalShadowRendering", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the StandardShader pixel path uploads the runtime emissive material value after fragment diagnostics are removed.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesRuntimeEmissiveOutputAfterFragmentDiagnosis() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.Contains("runtimeMaterial.GetEmissiveColor().X, runtimeMaterial.GetEmissiveColor().Y, runtimeMaterial.GetEmissiveColor().Z, runtimeMaterial.GetEmissiveColor().W", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("const float emissiveData[] = { 1.0f, 1.0f, 1.0f, 1.0f };", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the directional-only StandardShader path does not retain point-shadow cube fallback resources or bindings.
    /// </summary>
    [Fact]
    public void RuntimeSeam_DirectionalStandardShaderDoesNotRetainPointShadowCubeFallbackResources() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string presenterHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.hpp"));
        string presenterSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUGx2Presenter.cpp"));

        Assert.DoesNotContain("InitializeStandardShaderFallbackCubeTexture", presenterHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("StandardShaderFallbackCubeTextureHandle", presenterHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("GX2_SURFACE_DIM_TEXTURE_CUBE", presenterSource, StringComparison.Ordinal);
        Assert.DoesNotContain("GX2_SAMPLER_VAR_TYPE_SAMPLER_CUBE", presenterSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the Wii U directional-shadow StandardShader variant excludes point-shadow cube sampling while preserving the shared directional atlas path.
    /// </summary>
    [Fact]
    public void RuntimeSeam_CompilesDirectionalStandardShaderWithoutPointShadowCubeSampling() {
        string repositoryRootPath = WiiUTestSourcePaths.ResolveRepositoryRootPath();
        string cookerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "builder", "WiiUShaderArtifactCooker.cs"));
        string sharedShaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "..", "helengine", "engine", "helengine.editor", "shaders", "builtin", "ForwardStandardShader.hlsl"));

        Assert.Contains("new ShaderDefine(\"HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY\", \"1\")", cookerSource, StringComparison.Ordinal);
        Assert.Contains("#if !defined(HELENGINE_WIIU_DIRECTIONAL_SHADOWS_ONLY)", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("float3 EvaluateWiiUDirectionalLight(", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("color += EvaluateWiiUDirectionalLight(", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("TextureCube pointShadowTexture0 : register(t2);", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("else if (shadowSlotMetadata.x > 0.5f && shadowSlotMetadata.z > 1.5f && lightType == 1)", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("Texture2D shadowAtlasTexture : register(t1);", sharedShaderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the unshadowed Wii U StandardShader reaches the shared material and directional-light calculation after temporary fragment-output controls are removed.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UnshadowedStandardShaderUsesSharedLightingOutput() {
        string repositoryRootPath = WiiUTestSourcePaths.ResolveRepositoryRootPath();
        string sharedShaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "..", "helengine", "engine", "helengine.editor", "shaders", "builtin", "ForwardStandardShader.hlsl"));

        Assert.DoesNotContain("return float4(1.0f, sampledDiffuseTexture.r, baseColor.b, 1.0f);", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("color += EvaluateWiiUDirectionalLight(", sharedShaderSource, StringComparison.Ordinal);
        Assert.Contains("return float4(saturate(color), sampledBaseColor.a);", sharedShaderSource, StringComparison.Ordinal);
    }
}
