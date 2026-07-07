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
    /// Ensures visible Wii U output delegates to a dedicated presenter-owned GX2 path instead of issuing OSScreen per-pixel writes inline.
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
        Assert.Contains("void RenderFrame(const WiiUGx2RenderFrame& frame);", File.ReadAllText(presenterHeaderPath), StringComparison.Ordinal);
        Assert.Contains("void RenderDiagnosticTriangleFrame();", File.ReadAllText(presenterHeaderPath), StringComparison.Ordinal);
        Assert.Contains("Gx2Presenter->RenderDiagnosticTriangleFrame();", applicationSource, StringComparison.Ordinal);
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
    /// Ensures the earlier clear-only bring-up slice remains available as a presenter-owned GX2 diagnostic step even though the active frame path now uses the square draw.
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
        Assert.Contains("Gx2Presenter->RenderFrame(EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
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
    /// Ensures the first Wii U 3D bring-up slice uses the present-only diagnostic loop so GX2 verification does not depend on scene draw stability.
    /// </summary>
    [Fact]
    public void RuntimeSeam_UsesPresentOnlyDiagnosticFrameLoopForFirst3dShaderBringUp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("enum class DiagnosticFrameLoopMode {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("FullEngine", applicationSource, StringComparison.Ordinal);
        Assert.Contains("constexpr DiagnosticFrameLoopMode DiagnosticFrameLoopModeValue = DiagnosticFrameLoopMode::PresentOnly;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::PresentOnly) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::DrawOnly) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!DrawEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("PresentFrame();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("continue;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!UpdateEngineCore()) {", applicationSource, StringComparison.Ordinal);
        Assert.Contains("OSSleepTicks(OSMillisecondsToTicks(16));", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the draw-only diagnostic toggle is restored so steady-state builds still run the Wii U 2D render manager.
    /// </summary>
    [Fact]
    public void RuntimeSeam_FullEngineLoopDoesNotKeepSkipping2DRendererSubmission() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("constexpr bool RunDiagnosticRenderManager2DDrawInDrawOnlyMode = true;", applicationSource, StringComparison.Ordinal);
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
    /// Ensures the first Wii U 3D bring-up slice renders one presenter-owned diagnostic triangle through offline-compiled GX2 shaders.
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
        Assert.Contains("Gx2Presenter->RenderDiagnosticTriangleFrame();", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Gx2Presenter->RenderFrame(EngineRenderManager2D->GetCurrentFrame());", applicationSource, StringComparison.Ordinal);
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

    /// <summary>
    /// Ensures the startup scene warm path commits deferred scene loading through one draw before the host configures the presenter-owned scene cube mesh.
    /// </summary>
    [Fact]
    public void RuntimeSeam_CommitsDeferredStartupSceneLoadBeforeConfiguringSceneCubeMesh() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUApplication.cpp"));

        Assert.Contains("Warming startup scene through one engine draw.", applicationSource, StringComparison.Ordinal);
        Assert.Contains("if (!DrawEngineCore()) {", applicationSource, StringComparison.Ordinal);

        int warmDrawIndex = applicationSource.IndexOf("Warming startup scene through one engine draw.", StringComparison.Ordinal);
        int configureSceneCubeMeshIndex = applicationSource.IndexOf("Gx2Presenter->ConfigureSceneCubeMesh(*EngineRenderManager3D->GetLatestRuntimeModel());", StringComparison.Ordinal);

        Assert.True(warmDrawIndex >= 0, "Expected the startup scene warm draw trace to exist.");
        Assert.True(configureSceneCubeMeshIndex > warmDrawIndex, "Expected scene cube mesh configuration to happen after the warm draw.");
    }

    /// <summary>
    /// Ensures the generated-core Wii U startup scene stays pinned to cube_test while the first visible cube bring-up path depends on one real 3D scene at boot.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesCubeTestAsGeneratedCoreStartupSceneForCubeBringUp() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wiiu", "WiiUSceneBootstrap.cpp"));

        Assert.Contains("return \"cube_test\";", bootstrapSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return he_get_runtime_wiiu_startup_scene_id();", bootstrapSource, StringComparison.Ordinal);
    }

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
}
