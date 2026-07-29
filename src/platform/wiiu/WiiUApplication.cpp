#include "platform/wiiu/WiiUApplication.hpp"
#include "platform/wiiu/WiiUGx2Presenter.hpp"
#include "platform/wiiu/WiiUInputBackend.hpp"
#include "platform/wiiu/WiiURuntimeDiagnosticsProvider.hpp"
#include "platform/wiiu/WiiUSceneBootstrap.hpp"

#include <cstdarg>
#include <cstdio>
#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "HostFileSystemContentStreamSource.hpp"
#include "InputControlId.hpp"
#include "InputControlKind.hpp"
#include "InputDeviceKind.hpp"
#if HELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION
#include "GeneratedRuntimeModuleRegistration.hpp"
#endif
#include "platform/wiiu/audio/WiiUAudioBackend.hpp"
#include "PlatformInfo.hpp"
#include "SceneLoadMode.hpp"
#include "SceneManager.hpp"
#include "StandardPlatformAction.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "runtime/native_list.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#include "platform/wiiu/WiiURenderManager2D.hpp"
#include "platform/wiiu/WiiURenderManager3D.hpp"
#endif

#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <whb/proc.h>

namespace helengine::wiiu {
    namespace {
        constexpr std::uint32_t StartupClearColor = 0xFF000000;
        constexpr std::uint32_t TvSurfaceWidth = 1280U;
        constexpr std::uint32_t TvSurfaceHeight = 720U;
        constexpr std::uint32_t DrcSurfaceWidth = 854U;
        constexpr std::uint32_t DrcSurfaceHeight = 480U;
        constexpr const char* RuntimeTracePaths[] = {
            "sd:/wiiu_runtime_trace.txt",
            "wiiu_runtime_trace.txt"
        };
        constexpr const char* BuildStamp = __DATE__ " " __TIME__;
        enum class DiagnosticFrameLoopMode {
            PresentOnly,
            UpdateOnly,
            DrawOnly,
            FullEngine
        };
        constexpr DiagnosticFrameLoopMode DiagnosticFrameLoopModeValue = DiagnosticFrameLoopMode::FullEngine;
        constexpr bool RunDiagnosticRenderManager2DDrawInDrawOnlyMode = true;
    }

#if defined(HELENGINE_WIIU_RUNTIME_STAGE_TRACE)
#define HELENGINE_WIIU_FRAME_TRACE(...) AppendRuntimeTrace(__VA_ARGS__)
#else
#define HELENGINE_WIIU_FRAME_TRACE(...) do { } while (false)
#endif

    /// Creates the Wii U application with no allocated screen buffers and the startup clear color.
    WiiUApplication::WiiUApplication()
        : TvBuffer(nullptr)
        , DrcBuffer(nullptr)
        , BootPhase(WiiUBootPhase::NativeStartup)
        , ClearColor(StartupClearColor)
        , BootFailureMessage()
        , LastRuntimeFailureMessage()
        , Gx2Presenter(nullptr)
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        , EngineInitialized(false)
        , EngineCore(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputBackend(nullptr)
        , EngineAudioBackend(nullptr)
        , EnginePlatformInfo(nullptr)
        , EngineRuntimeDiagnosticsProvider(nullptr)
        , EngineContentStreamSource(nullptr)
#endif
    {
    }

    /// Releases generated-core bridge objects and native screen buffers after the application loop finishes.
    WiiUApplication::~WiiUApplication() {
        delete Gx2Presenter;
        Gx2Presenter = nullptr;

#if HELENGINE_WIIU_HAS_GENERATED_CORE
        delete EngineRenderManager2D;
        delete EngineRenderManager3D;
        delete EngineInputBackend;
        delete EngineAudioBackend;
        delete EnginePlatformInfo;
        delete EngineCore;
        delete EngineRuntimeDiagnosticsProvider;
        delete EngineContentStreamSource;
#endif

        if (TvBuffer != nullptr) {
            MEMFreeToDefaultHeap(TvBuffer);
            TvBuffer = nullptr;
        }

        if (DrcBuffer != nullptr) {
            MEMFreeToDefaultHeap(DrcBuffer);
            DrcBuffer = nullptr;
        }
    }

    /// Runs the current Wii U proof-of-life application loop until the process shuts down.
    int WiiUApplication::Run() {
        WHBProcInit();
        AppendRuntimeTrace("\n=== Wii U application run begin ===\n");

        SetBootPhase(WiiUBootPhase::VideoInitialization, StartupClearColor);
        AppendRuntimeTrace("[WiiUFile] InitializeVideo begin.\n");
        if (!InitializeVideo()) {
            AppendRuntimeTrace("[WiiUFile] InitializeVideo failed.\n");
            ShowBootFailure("InitializeVideo", "Could not allocate or initialize OSScreen buffers.");
            SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
            WHBProcShutdown();
            return 101;
        }
        AppendRuntimeTrace("[WiiUFile] InitializeVideo completed.\n");
        AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter begin.\n");
        if (!InitializeGx2Presenter()) {
            AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter failed.\n");
            ShowBootFailure("InitializeGx2Presenter", "GX2 presenter initialization returned false.");
            SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
            OSScreenShutdown();
            WHBProcShutdown();
            return 102;
        }
        AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter completed.\n");
        if (!InitializeEngineCore()) {
            AppendRuntimeTrace("[WiiUFile] InitializeEngineCore failed.\n");
            ShowBootFailure("InitializeEngineCore", "Generated runtime initialization returned false without an exception.");
            SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
            OSScreenShutdown();
            WHBProcShutdown();
            return 103;
        }
        SetBootPhase(WiiUBootPhase::Running, StartupClearColor);
        while (WHBProcIsRunning()) {
            if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::PresentOnly) {
                if (!PresentFrame()) {
                    ShowBootFailure("PresentFrame", LastRuntimeFailureMessage.c_str());
                    return 106;
                }
                continue;
            }

            if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::UpdateOnly) {
                if (!UpdateEngineCore()) {
                    ShowBootFailure("UpdateEngineCore", LastRuntimeFailureMessage.c_str());
                    SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
                    OSScreenShutdown();
                    WHBProcShutdown();
                    return 104;
                }

                if (!PresentFrame()) {
                    ShowBootFailure("PresentFrame", LastRuntimeFailureMessage.c_str());
                    return 106;
                }
                continue;
            }

            if (DiagnosticFrameLoopModeValue == DiagnosticFrameLoopMode::DrawOnly) {
                if (!DrawEngineCore()) {
                    ShowBootFailure("DrawEngineCore", LastRuntimeFailureMessage.c_str());
                    SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
                    OSScreenShutdown();
                    WHBProcShutdown();
                    return 105;
                }

                if (!PresentFrame()) {
                    ShowBootFailure("PresentFrame", LastRuntimeFailureMessage.c_str());
                    return 106;
                }
                continue;
            }

            if (!UpdateEngineCore()) {
                ShowBootFailure("UpdateEngineCore", LastRuntimeFailureMessage.c_str());
                SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
                OSScreenShutdown();
                WHBProcShutdown();
                return 104;
            }

            if (!DrawEngineCore()) {
                ShowBootFailure("DrawEngineCore", LastRuntimeFailureMessage.c_str());
                SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
                OSScreenShutdown();
                WHBProcShutdown();
                return 105;
            }

            if (!PresentFrame()) {
                ShowBootFailure("PresentFrame", LastRuntimeFailureMessage.c_str());
                return 106;
            }
            OSSleepTicks(OSMillisecondsToTicks(16));
        }

        OSScreenShutdown();
        WHBProcShutdown();
        return 0;
    }

    /// Initializes the current OSScreen video path and allocates the display work buffers.
    bool WiiUApplication::InitializeVideo() {
        OSReport("[WiiU] InitializeVideo begin.\n");
        OSScreenInit();

        TvBuffer = AllocateScreenBuffer(SCREEN_TV);
        DrcBuffer = AllocateScreenBuffer(SCREEN_DRC);
        if (TvBuffer == nullptr || DrcBuffer == nullptr) {
            OSReport("[WiiU] InitializeVideo buffer allocation failed.\n");
            return false;
        }

        OSScreenSetBufferEx(SCREEN_TV, TvBuffer);
        OSScreenSetBufferEx(SCREEN_DRC, DrcBuffer);
        OSScreenEnableEx(SCREEN_TV, true);
        OSScreenEnableEx(SCREEN_DRC, true);
        OSReport("[WiiU] InitializeVideo completed.\n");
        return true;
    }

    /// Initializes the Wii U GX2 presenter used for steady-state rendered output.
    bool WiiUApplication::InitializeGx2Presenter() {
        if (Gx2Presenter != nullptr) {
            return true;
        }

        try {
            OSReport("[WiiU] InitializeGx2Presenter construct presenter.\n");
            Gx2Presenter = new WiiUGx2Presenter();
            OSReport("[WiiU] InitializeGx2Presenter initialize presenter begin.\n");
            bool initialized = Gx2Presenter->Initialize();
            OSReport("[WiiU] InitializeGx2Presenter initialize presenter completed result=%d.\n", initialized ? 1 : 0);
            return initialized;
        }
        catch (const std::exception& exception) {
            OSReport("[WiiU] InitializeGx2Presenter threw: %s\n", exception.what());
            AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter threw: %s\n", exception.what());
            ShowBootFailure("InitializeGx2Presenter", exception.what());
            return false;
        }
        catch (...) {
            OSReport("[WiiU] InitializeGx2Presenter threw an unknown exception.\n");
            AppendRuntimeTrace("[WiiUFile] InitializeGx2Presenter threw an unknown exception.\n");
            ShowBootFailure("InitializeGx2Presenter", "Unknown exception.");
            return false;
        }
    }

    /// Initializes the Wii U generated core and queues the packaged startup scene.
    bool WiiUApplication::InitializeEngineCore() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        const char* initializationStage = "Begin";
        try {
            AppendRuntimeTrace("\n=== Wii U runtime session %s ===\n", BuildStamp);
            OSReport("[WiiU] InitializeEngineCore begin.\n");
            AppendRuntimeTrace("[WiiUFile] InitializeEngineCore begin.\n");
            SetBootPhase(WiiUBootPhase::Running, 0xFF804000);

            initializationStage = "ResolvePackagedContentRoot";
            std::string packagedContentRootPath = WiiUSceneBootstrap::GetPackagedContentRootPath();
            OSReport("[WiiU] Packaged content root: %s\n", packagedContentRootPath.c_str());
            AppendRuntimeTrace("[WiiUFile] Packaged content root: %s\n", packagedContentRootPath.c_str());

            initializationStage = "CreatePackagedSceneCatalog";
            RuntimeSceneCatalog* packagedCatalog = WiiUSceneBootstrap::CreatePackagedSceneCatalog();

            initializationStage = "ResolveStartupSceneId";
            std::string packagedStartupSceneId = WiiUSceneBootstrap::GetPackagedStartupSceneId();
            OSReport("[WiiU] Packaged startup scene id: %s\n", packagedStartupSceneId.c_str());
            AppendRuntimeTrace("[WiiUFile] Packaged startup scene id: %s\n", packagedStartupSceneId.c_str());
            if (packagedCatalog == nullptr || packagedContentRootPath.empty() || packagedStartupSceneId.empty()) {
                OSReport("[WiiU] Packaged scene bootstrap data was incomplete.\n");
                AppendRuntimeTrace("[WiiUFile] Packaged scene bootstrap data was incomplete.\n");
                return false;
            }

            initializationStage = "ConstructInitializationOptions";
            CoreInitializationOptions* initializationOptions = new CoreInitializationOptions();
            EngineContentStreamSource = new HostFileSystemContentStreamSource(packagedContentRootPath);
            EngineRuntimeDiagnosticsProvider = new WiiURuntimeDiagnosticsProvider();

            initializationStage = "AssignInitializationOptions";
            initializationOptions->ContentStreamSource = EngineContentStreamSource;
            initializationOptions->SceneCatalog = packagedCatalog;
            initializationOptions->UpdateOrderLayers = 4;
            initializationOptions->RenderOrderLayers3D = 4;
            initializationOptions->UpdateListInitialCapacity = 64;
            initializationOptions->RenderList2DInitialCapacity = 64;
            initializationOptions->RenderList3DInitialCapacity = 64;
            initializationOptions->RuntimeDiagnosticsProvider = EngineRuntimeDiagnosticsProvider;
            initializationOptions->StandardPlatformInputConfiguration = CreateStandardPlatformInputConfiguration();

            initializationStage = "ProbePackagedContent";
            if (!ProbePackagedContent()) {
                return false;
            }

            initializationStage = "ConstructCore";
            EngineCore = new Core(initializationOptions);

            initializationStage = "ConstructBridgeServices";
            EngineRenderManager3D = new WiiURenderManager3D();
            EngineRenderManager2D = new WiiURenderManager2D();
            EngineInputBackend = new WiiUInputBackend();
            EngineAudioBackend = new WiiUAudioBackend();
            EnginePlatformInfo = new PlatformInfo("wiiu", "1.0");

            initializationStage = "AddPrimaryWindow";
            EngineRenderManager3D->AddWindow(0, TvSurfaceWidth, TvSurfaceHeight);

            initializationStage = "InitializeCore";
            OSReport("[WiiU] Calling EngineCore->Initialize.\n");
            AppendRuntimeTrace("[WiiUFile] Calling EngineCore->Initialize.\n");
            EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, initializationOptions);
            EngineCore->SetAudioBackend(EngineAudioBackend);
            OSReport("[WiiU] Engine core initialized.\n");
            AppendRuntimeTrace("[WiiUFile] Engine core initialized.\n");

#if HELENGINE_WIIU_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION
            initializationStage = "RegisterGeneratedRuntimeModules";
            RegisterGeneratedRuntimeModules(EngineCore);
            OSReport("[WiiU] Generated runtime modules registered.\n");
            AppendRuntimeTrace("[WiiUFile] Generated runtime modules registered.\n");
#endif

            initializationStage = "AcquireSceneManager";
            if (EngineCore->get_SceneManager() == nullptr) {
                OSReport("[WiiU] Scene manager was null after initialization.\n");
                AppendRuntimeTrace("[WiiUFile] Scene manager was null after initialization.\n");
                return false;
            }

            initializationStage = "QueueStartupScene";
            OSReport("[WiiU] Queueing packaged startup scene.\n");
            AppendRuntimeTrace("[WiiUFile] Queueing packaged startup scene.\n");
            EngineCore->get_SceneManager()->LoadScene(packagedStartupSceneId, SceneLoadMode::Single);
            OSReport("[WiiU] Packaged startup scene queued.\n");
            AppendRuntimeTrace("[WiiUFile] Packaged startup scene queued.\n");
            EngineInitialized = true;
            initializationStage = "WarmStartupScene";
            OSReport("[WiiU] Warming startup scene through one engine update.\n");
            AppendRuntimeTrace("[WiiUFile] Warming startup scene through one engine update.\n");
            if (!UpdateEngineCore()) {
                OSReport("[WiiU] Startup scene warm update failed.\n");
                AppendRuntimeTrace("[WiiUFile] Startup scene warm update failed.\n");
                return false;
            }

            initializationStage = "WarmStartupSceneDraw";
            OSReport("[WiiU] Warming startup scene through one engine draw.\n");
            AppendRuntimeTrace("[WiiUFile] Warming startup scene through one engine draw.\n");
            if (!DrawEngineCore()) {
                OSReport("[WiiU] Startup scene warm draw failed.\n");
                AppendRuntimeTrace("[WiiUFile] Startup scene warm draw failed.\n");
                return false;
            }
            SetBootPhase(WiiUBootPhase::Running, 0xFF004000);
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            OSReport("[WiiU] Engine core initialization threw Exception* stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiUFile] Engine core initialization threw Exception* stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            ShowBootFailure(initializationStage, exception != nullptr ? exception->what() : "<null generated exception>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            OSReport("[WiiU] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            AppendRuntimeTrace("[WiiUFile] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            ShowBootFailure(initializationStage, exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            OSReport("[WiiU] Engine core initialization threw stage=%s.\n", initializationStage);
            AppendRuntimeTrace("[WiiUFile] Engine core initialization threw stage=%s.\n", initializationStage);
            ShowBootFailure(initializationStage, "Unknown exception.");
            return false;
        }
#else
        return true;
#endif
    }

    /// Opens representative packaged payloads before engine startup so bundle filesystem failures identify the missing entry directly.
    bool WiiUApplication::ProbePackagedContent() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        if (EngineContentStreamSource == nullptr) {
            throw std::logic_error("The packaged content stream source was not initialized.");
        }

        constexpr const char* ContentPaths[] = {
            "cooked/scenes/helenofcodesplash.hasset",
            "wiiu_standard_material.hasset",
            "cooked/engine/materials/standard.hasset"
        };
        for (const char* contentPath : ContentPaths) {
            try {
                Stream* stream = EngineContentStreamSource->OpenRead(contentPath);
                delete stream;
                AppendRuntimeTrace("[WiiUFile] Packaged content probe opened: %s\n", contentPath);
            }
            catch (Exception* exception) {
                const char* message = exception != nullptr ? exception->what() : "<null generated exception>";
                AppendRuntimeTrace("[WiiUFile] Packaged content probe failed: %s message=%s\n", contentPath, message);
                delete exception;
                ShowBootFailure("ProbePackagedContent", message);
                return false;
            }
            catch (const std::exception& exception) {
                AppendRuntimeTrace("[WiiUFile] Packaged content probe failed: %s message=%s\n", contentPath, exception.what());
                ShowBootFailure("ProbePackagedContent", exception.what());
                return false;
            }
        }
        return true;
#else
        return true;
#endif
    }

    /// Builds the generated standard-platform input configuration emitted by the Wii U builder so packaged Accept and Return actions reach gameplay code.
    StandardPlatformInputConfiguration* WiiUApplication::CreateStandardPlatformInputConfiguration() const {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        std::size_t actionEntryCount = 0;
        const HERuntimeStandardPlatformActionEntry* actionEntries = he_runtime_standard_platform_action_entries(&actionEntryCount);
        if (actionEntries == nullptr && actionEntryCount != 0U) {
            throw std::runtime_error("Standard platform input manifest reported bindings without entries.");
        }

        List<StandardPlatformActionBinding*>* bindings = new List<StandardPlatformActionBinding*>();
        for (std::size_t index = 0; index < actionEntryCount; index++) {
            const HERuntimeStandardPlatformActionEntry& actionEntry = actionEntries[index];
            bindings->Add(new StandardPlatformActionBinding(
                static_cast<StandardPlatformAction>(actionEntry.ActionId),
                InputControlId(
                    static_cast<InputDeviceKind>(actionEntry.DeviceKind),
                    static_cast<InputControlKind>(actionEntry.ControlKind),
                    actionEntry.DeviceIndex,
                    actionEntry.ControlIndex)));
        }

        return new StandardPlatformInputConfiguration(bindings);
#else
        return nullptr;
#endif
    }

    /// Advances one generated-core update tick for the packaged Wii U runtime.
    bool WiiUApplication::UpdateEngineCore() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager2D == nullptr || EngineRenderManager3D == nullptr) {
            LastRuntimeFailureMessage = "Generated runtime update prerequisites were not initialized.";
            return false;
        }

        try {
            SetBootPhase(WiiUBootPhase::Running, 0xFF006000);
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineCore->Update begin.\n");
            EngineCore->Update();
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineCore->Update completed.\n");
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineRenderManager2D->FlushReleasedTextures begin.\n");
            EngineRenderManager2D->FlushReleasedTextures();
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineRenderManager2D->FlushReleasedTextures completed.\n");
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineRenderManager3D->FlushReleasedAssets begin.\n");
            EngineRenderManager3D->FlushReleasedAssets();
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine update stage: EngineRenderManager3D->FlushReleasedAssets completed.\n");
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = exception != nullptr ? exception->what() : "<null generated exception>";
            OSReport("[WiiU] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiUFile] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = exception.what();
            OSReport("[WiiU] Engine update threw std::exception: %s\n", exception.what());
            AppendRuntimeTrace("[WiiUFile] Engine update threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = "Unknown exception.";
            OSReport("[WiiU] Engine update threw.\n");
            AppendRuntimeTrace("[WiiUFile] Engine update threw.\n");
            return false;
        }
#else
        return true;
#endif
    }

    /// Draws one generated-core frame for the packaged Wii U runtime.
    bool WiiUApplication::DrawEngineCore() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager2D == nullptr || EngineRenderManager3D == nullptr) {
            LastRuntimeFailureMessage = "Generated runtime draw prerequisites were not initialized.";
            return false;
        }

        try {
            SetBootPhase(WiiUBootPhase::Running, 0xFF008000);
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineCore->Draw begin.\n");
            EngineCore->Draw();
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineCore->Draw completed.\n");
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineRenderManager3D->Draw begin.\n");
            EngineRenderManager3D->Draw();
            HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineRenderManager3D->Draw completed.\n");
            if (DiagnosticFrameLoopModeValue != DiagnosticFrameLoopMode::DrawOnly || RunDiagnosticRenderManager2DDrawInDrawOnlyMode) {
                HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineRenderManager2D->Draw begin.\n");
                EngineRenderManager2D->Draw();
                HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] Engine draw stage: EngineRenderManager2D->Draw completed.\n");
            }
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = exception != nullptr ? exception->what() : "<null generated exception>";
            OSReport("[WiiU] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiUFile] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = exception.what();
            OSReport("[WiiU] Engine draw threw std::exception: %s\n", exception.what());
            AppendRuntimeTrace("[WiiUFile] Engine draw threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            LastRuntimeFailureMessage = "Unknown exception.";
            OSReport("[WiiU] Engine draw threw.\n");
            AppendRuntimeTrace("[WiiUFile] Engine draw threw.\n");
            return false;
        }
#else
        return true;
#endif
    }

    /// Presents one frame using either the diagnostic boot clear color or the renderer-owned output surfaces.
    bool WiiUApplication::PresentFrame() {
        try {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
            if (!EngineInitialized) {
                PresentBootPhaseFrame();
                return true;
            }

            PresentRenderedFrame();
#else
            PresentBootPhaseFrame();
#endif
            return true;
        }
        catch (Exception* exception) {
            LastRuntimeFailureMessage = exception != nullptr ? exception->what() : "<null generated exception>";
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            LastRuntimeFailureMessage = exception.what();
            return false;
        }
        catch (...) {
            LastRuntimeFailureMessage = "Unknown exception.";
            return false;
        }
    }

    /// Presents one boot-phase frame using the current diagnostic clear color on both displays.
    void WiiUApplication::PresentBootPhaseFrame() {
        OSScreenClearBufferEx(SCREEN_TV, ClearColor);
        OSScreenClearBufferEx(SCREEN_DRC, ClearColor);
        if (!BootFailureMessage.empty()) {
            DrawBootFailureMessage(SCREEN_TV);
            DrawBootFailureMessage(SCREEN_DRC);
        }
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    /// Draws the terminal boot failure as separate rows because OSScreen font output does not lay out embedded line breaks.
    void WiiUApplication::DrawBootFailureMessage(OSScreenID screen) const {
        std::size_t lineStart = 0U;
        std::uint32_t lineY = 2U;
        while (lineStart < BootFailureMessage.length()) {
            std::size_t lineEnd = BootFailureMessage.find('\n', lineStart);
            std::string line = BootFailureMessage.substr(lineStart, lineEnd - lineStart);
            std::size_t characterStart = 0U;
            while (characterStart < line.length()) {
                std::string wrappedLine = line.substr(characterStart, 38U);
                OSScreenPutFontEx(screen, 2U, lineY, wrappedLine.c_str());
                lineY += 2U;
                characterStart += wrappedLine.length();
            }

            if (lineEnd == std::string::npos) {
                break;
            }

            lineStart = lineEnd + 1U;
        }
    }

    /// Presents one renderer-owned frame after the generated core has initialized.
    void WiiUApplication::PresentRenderedFrame() {
        if (Gx2Presenter == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter must exist before rendered presentation can begin.");
        } else if (EngineRenderManager3D == nullptr) {
            throw std::runtime_error("Wii U 3D render manager must exist before rendered presentation can begin.");
        } else if (EngineRenderManager2D == nullptr) {
            throw std::runtime_error("Wii U 2D render manager must exist before rendered presentation can begin.");
        }

        HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame begin.\n");
        Gx2Presenter->RenderFrame(EngineRenderManager3D->GetCurrentFrame(), EngineRenderManager2D->GetCurrentFrame());
        HELENGINE_WIIU_FRAME_TRACE("[WiiUFile] PresentRenderedFrame completed.\n");
    }

    /// Appends one host-readable Wii U runtime trace line to every supported trace sink.
    void WiiUApplication::AppendRuntimeTrace(const char* format, ...) {
        char buffer[2048];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);

        for (const char* runtimeTracePath : RuntimeTracePaths) {
            std::FILE* file = std::fopen(runtimeTracePath, "ab");
            if (file == nullptr) {
                continue;
            }

            std::fputs(buffer, file);
            std::fflush(file);
            std::fclose(file);
        }
    }

    /// Displays one terminal boot failure through the persistent OSScreen boot path and preserves it in the runtime trace.
    void WiiUApplication::ShowBootFailure(const char* stage, const char* message) {
        const char* resolvedStage = stage != nullptr ? stage : "UnknownStage";
        const char* resolvedMessage = message != nullptr ? message : "No diagnostic message.";
        char formattedMessage[2048];
        std::snprintf(formattedMessage, sizeof(formattedMessage), "HelEngine Wii U boot failure\nStage: %s\n%s", resolvedStage, resolvedMessage);
        BootFailureMessage = formattedMessage;
        EngineInitialized = false;
        SetBootPhase(WiiUBootPhase::Failed, 0xFF000080);
        AppendRuntimeTrace("[WiiUFile] Cemu-visible boot failure stage=%s message=%s\n", resolvedStage, resolvedMessage);
        while (WHBProcIsRunning()) {
            PresentBootPhaseFrame();
            OSSleepTicks(OSMillisecondsToTicks(16));
        }
    }

    /// Stores the active boot phase and clear color used by the present loop.
    void WiiUApplication::SetBootPhase(WiiUBootPhase phase, std::uint32_t color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Allocates an aligned OSScreen backing buffer for the requested display.
    void* WiiUApplication::AllocateScreenBuffer(OSScreenID screen) {
        const std::uint32_t bufferSize = OSScreenGetBufferSizeEx(screen);
        void* buffer = MEMAllocFromDefaultHeapEx(bufferSize, 0x100);
        if (buffer != nullptr) {
            std::memset(buffer, 0, bufferSize);
        }

        return buffer;
    }
}

#undef HELENGINE_WIIU_FRAME_TRACE
