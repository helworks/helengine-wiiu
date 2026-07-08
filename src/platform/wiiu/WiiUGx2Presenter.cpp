#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include "diagnostic_triangle_shader_bin.h"
#include "diagnostic_square_shader_bin.h"
#include "scene_opaque_lit_shader_bin.h"
#include "scene_cube_flat_color_shader_bin.h"
#include "ui_quad_shader_bin.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cstdarg>

#include "platform/wiiu/WiiURuntimeMaterial.hpp"
#include "platform/wiiu/WiiURuntimeModel.hpp"

#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <gfd.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <gx2/temp.h>
#include <gx2/utils.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <gx2r/resource.h>
#include <gx2r/surface.h>
#include <whb/gfx.h>

namespace helengine::wiiu {
    namespace {
        constexpr std::uint32_t TvSurfaceWidth = 1280U;
        constexpr std::uint32_t TvSurfaceHeight = 720U;
        constexpr std::uint32_t DrcSurfaceWidth = 854U;
        constexpr std::uint32_t DrcSurfaceHeight = 480U;
        constexpr std::uint32_t LogicalFrameWidth = 1280U;
        constexpr std::uint32_t LogicalFrameHeight = 720U;
        constexpr GX2SurfaceFormat PresentationSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        constexpr GX2TVRenderMode PresentationTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
        constexpr std::uint32_t CommandBufferPoolSize = 0x400000U;
        constexpr GX2RResourceFlags NoGx2rResourceFlags = static_cast<GX2RResourceFlags>(0);
        constexpr GX2RResourceFlags DiagnosticVertexBufferFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        constexpr GX2RResourceFlags DiagnosticIndexBufferFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_INDEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        constexpr GX2RResourceFlags DiagnosticUniformBufferFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_UNIFORM_BLOCK | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        constexpr GX2RResourceFlags TextureSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        constexpr float DiagnosticClearRed = 0.5294118f;
        constexpr float DiagnosticClearGreen = 0.36862746f;
        constexpr float DiagnosticClearBlue = 0.6392157f;
        constexpr float DiagnosticClearAlpha = 1.0f;
        constexpr float DiagnosticSquareRed = 1.0f;
        constexpr float DiagnosticSquareGreen = 0.92156863f;
        constexpr float DiagnosticSquareBlue = 0.0f;
        constexpr float DiagnosticSquareAlpha = 1.0f;
        constexpr float DiagnosticTriangleTopRed = 1.0f;
        constexpr float DiagnosticTriangleTopGreen = 0.40392157f;
        constexpr float DiagnosticTriangleTopBlue = 0.7058824f;
        constexpr float DiagnosticTriangleTopAlpha = 1.0f;
        constexpr float DiagnosticTriangleLeftRed = 0.40392157f;
        constexpr float DiagnosticTriangleLeftGreen = 0.83137256f;
        constexpr float DiagnosticTriangleLeftBlue = 1.0f;
        constexpr float DiagnosticTriangleLeftAlpha = 1.0f;
        constexpr float DiagnosticTriangleRightRed = 1.0f;
        constexpr float DiagnosticTriangleRightGreen = 0.972549f;
        constexpr float DiagnosticTriangleRightBlue = 0.47843137f;
        constexpr float DiagnosticTriangleRightAlpha = 1.0f;
        constexpr std::uint32_t DiagnosticSquareVertexCount = 6U;
        constexpr std::uint32_t DiagnosticSquareVertexElementSize = 4U * sizeof(float);
        constexpr std::uint32_t DiagnosticTriangleVertexCount = 3U;
        constexpr std::uint32_t DiagnosticTriangleVertexElementSize = 4U * sizeof(float);
        constexpr std::uint32_t DiagnosticTriangleTransformSizeInBytes = 16U * sizeof(float);
        constexpr std::uint32_t SceneOpaquePositionElementSize = 4U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueNormalElementSize = 3U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueIndexElementSize = sizeof(std::uint16_t);
        constexpr std::uint32_t SceneOpaqueTransformSizeInBytes = 48U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueMaterialSizeInBytes = 8U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueLightSizeInBytes = 12U * sizeof(float);
        constexpr std::uint32_t SceneCubeVertexElementSize = 4U * sizeof(float);
        constexpr std::uint32_t SceneCubeIndexElementSize = sizeof(std::uint16_t);
        constexpr std::uint32_t SceneCubeTransformSizeInBytes = 16U * sizeof(float);
        constexpr double SceneDrivenFieldOfViewRadians = 1.0;
        constexpr std::uint32_t UiQuadVertexCount = 6U;
        constexpr std::uint32_t UiQuadPositionElementSize = 2U * sizeof(float);
        constexpr std::uint32_t UiQuadTexCoordElementSize = 2U * sizeof(float);
        constexpr std::uint32_t UiQuadColorElementSize = 4U * sizeof(float);
        constexpr std::uint32_t SolidWhitePixel = 0xFFFFFFFFU;
        constexpr const char* RuntimeTracePaths[] = {
            "sd:/wiiu_runtime_trace.txt",
            "wiiu_runtime_trace.txt"
        };
        const float DiagnosticSquarePositionData[] = {
            -0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.0f, 1.0f
        };
        const float DiagnosticSquareColorData[] = {
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha,
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha,
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha,
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha,
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha,
            DiagnosticSquareRed, DiagnosticSquareGreen, DiagnosticSquareBlue, DiagnosticSquareAlpha
        };
        const float DiagnosticTrianglePositionData[] = {
            0.25f, 0.85f, 0.0f, 1.0f,
            -0.40f, -0.35f, 0.0f, 1.0f,
            0.90f, -0.35f, 0.0f, 1.0f
        };
        const float DiagnosticTriangleColorData[] = {
            DiagnosticTriangleTopRed, DiagnosticTriangleTopGreen, DiagnosticTriangleTopBlue, DiagnosticTriangleTopAlpha,
            DiagnosticTriangleLeftRed, DiagnosticTriangleLeftGreen, DiagnosticTriangleLeftBlue, DiagnosticTriangleLeftAlpha,
            DiagnosticTriangleRightRed, DiagnosticTriangleRightGreen, DiagnosticTriangleRightBlue, DiagnosticTriangleRightAlpha
        };
        const float DiagnosticTriangleTransformData[] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.25f, 0.20f, 0.0f, 1.0f
        };
        const float SceneCubeTransformData[] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        void StoreFloat32LittleEndian(void* destination, float value) {
            std::uint32_t bits = 0U;
            std::memcpy(&bits, &value, sizeof(bits));
            std::uint8_t* destinationBytes = static_cast<std::uint8_t*>(destination);
            destinationBytes[0] = static_cast<std::uint8_t>(bits & 0xFFU);
            destinationBytes[1] = static_cast<std::uint8_t>((bits >> 8U) & 0xFFU);
            destinationBytes[2] = static_cast<std::uint8_t>((bits >> 16U) & 0xFFU);
            destinationBytes[3] = static_cast<std::uint8_t>((bits >> 24U) & 0xFFU);
        }

        void StoreFloatArrayAsLittleEndian(void* destination, const float* source, std::size_t valueCount) {
            std::uint8_t* destinationBytes = static_cast<std::uint8_t*>(destination);
            for (std::size_t valueIndex = 0; valueIndex < valueCount; valueIndex++) {
                StoreFloat32LittleEndian(destinationBytes + (valueIndex * sizeof(float)), source[valueIndex]);
            }
        }
    }

    /// Creates one uninitialized GX2 presenter.
    WiiUGx2Presenter::WiiUGx2Presenter()
        : IsInitialized(false)
        , TvScanBuffer(nullptr)
        , DrcScanBuffer(nullptr)
        , CommandBufferPool(nullptr)
        , TvColorBuffer()
        , DrcColorBuffer()
        , TvDepthBuffer()
        , DrcDepthBuffer()
        , TvContextState(nullptr)
        , DrcContextState(nullptr)
        , AreDiagnosticSquareResourcesInitialized(false)
        , DiagnosticSquareShaderGroup()
        , DiagnosticSquarePositionBuffer()
        , DiagnosticSquareColorBuffer()
        , AreDiagnosticTriangleResourcesInitialized(false)
        , DiagnosticTriangleShaderGroup()
        , DiagnosticTrianglePositionBuffer()
        , DiagnosticTriangleColorBuffer()
        , DiagnosticTriangleTransformBuffer()
        , AreSceneOpaqueResourcesInitialized(false)
        , SceneOpaqueShaderGroup()
        , SceneOpaquePositionBuffer()
        , SceneOpaqueNormalBuffer()
        , SceneOpaqueIndexBuffer()
        , SceneOpaqueTransformBuffer()
        , SceneOpaqueMaterialBuffer()
        , SceneOpaqueLightBuffer()
        , SceneOpaqueVertexCount(0U)
        , AreSceneCubeResourcesInitialized(false)
        , IsSceneCubeMeshConfigured(false)
        , SceneCubeShaderGroup()
        , SceneCubePositionBuffer()
        , SceneCubeIndexBuffer()
        , SceneCubeTransformBuffer()
        , SceneCubeIndexCount(0U)
        , AreUiQuadResourcesInitialized(false)
        , UiQuadShaderGroup()
        , UiQuadPositionBuffer()
        , UiQuadTexCoordBuffer()
        , UiQuadColorBuffer()
        , UiSolidWhiteTextureHandle()
        , TvScanBufferSize(0U)
        , DrcScanBufferSize(0U)
        , Scene3DDebugLogCount(0U) {
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
        std::memset(&TvDepthBuffer, 0, sizeof(TvDepthBuffer));
        std::memset(&DrcDepthBuffer, 0, sizeof(DrcDepthBuffer));
        std::memset(&DiagnosticSquareShaderGroup, 0, sizeof(DiagnosticSquareShaderGroup));
        std::memset(&DiagnosticSquarePositionBuffer, 0, sizeof(DiagnosticSquarePositionBuffer));
        std::memset(&DiagnosticSquareColorBuffer, 0, sizeof(DiagnosticSquareColorBuffer));
        std::memset(&DiagnosticTriangleShaderGroup, 0, sizeof(DiagnosticTriangleShaderGroup));
        std::memset(&DiagnosticTrianglePositionBuffer, 0, sizeof(DiagnosticTrianglePositionBuffer));
        std::memset(&DiagnosticTriangleColorBuffer, 0, sizeof(DiagnosticTriangleColorBuffer));
        std::memset(&DiagnosticTriangleTransformBuffer, 0, sizeof(DiagnosticTriangleTransformBuffer));
        std::memset(&SceneOpaqueShaderGroup, 0, sizeof(SceneOpaqueShaderGroup));
        std::memset(&SceneOpaquePositionBuffer, 0, sizeof(SceneOpaquePositionBuffer));
        std::memset(&SceneOpaqueNormalBuffer, 0, sizeof(SceneOpaqueNormalBuffer));
        std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        std::memset(&SceneOpaqueTransformBuffer, 0, sizeof(SceneOpaqueTransformBuffer));
        std::memset(&SceneOpaqueMaterialBuffer, 0, sizeof(SceneOpaqueMaterialBuffer));
        std::memset(&SceneOpaqueLightBuffer, 0, sizeof(SceneOpaqueLightBuffer));
        std::memset(&SceneCubeShaderGroup, 0, sizeof(SceneCubeShaderGroup));
        std::memset(&SceneCubePositionBuffer, 0, sizeof(SceneCubePositionBuffer));
        std::memset(&SceneCubeIndexBuffer, 0, sizeof(SceneCubeIndexBuffer));
        std::memset(&SceneCubeTransformBuffer, 0, sizeof(SceneCubeTransformBuffer));
        std::memset(&UiQuadShaderGroup, 0, sizeof(UiQuadShaderGroup));
        std::memset(&UiQuadPositionBuffer, 0, sizeof(UiQuadPositionBuffer));
        std::memset(&UiQuadTexCoordBuffer, 0, sizeof(UiQuadTexCoordBuffer));
        std::memset(&UiQuadColorBuffer, 0, sizeof(UiQuadColorBuffer));
    }

    /// Releases all GX2-owned presentation resources.
    WiiUGx2Presenter::~WiiUGx2Presenter() {
        Shutdown();
    }

    /// Initializes the GX2 presentation path for TV and DRC output.
    bool WiiUGx2Presenter::Initialize() {
        if (IsInitialized) {
            return true;
        }

        AppendInitializationTrace("[WiiUFile] GX2 initialize: allocate command buffer pool.\n");
        CommandBufferPool = MEMAllocFromDefaultHeapEx(CommandBufferPoolSize, GX2_COMMAND_BUFFER_ALIGNMENT);
        if (CommandBufferPool == nullptr) {
            Shutdown();
            return false;
        }

        std::uint32_t initAttributes[] = {
            GX2_INIT_CMD_BUF_BASE, reinterpret_cast<std::uintptr_t>(CommandBufferPool),
            GX2_INIT_CMD_BUF_POOL_SIZE, CommandBufferPoolSize,
            GX2_INIT_ARGC, 0,
            GX2_INIT_ARGV, 0,
            GX2_INIT_END
        };
        AppendInitializationTrace("[WiiUFile] GX2 initialize: call GX2Init.\n");
        GX2Init(initAttributes);

        AppendInitializationTrace("[WiiUFile] GX2 initialize: calculate scan buffer sizes.\n");
        GX2DrcRenderMode drcRenderMode = GX2GetSystemDRCMode();
        std::uint32_t unusedSize = 0U;
        GX2CalcTVSize(PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &TvScanBufferSize, &unusedSize);
        GX2CalcDRCSize(drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &DrcScanBufferSize, &unusedSize);

        AppendInitializationTrace("[WiiUFile] GX2 initialize: allocate scan buffers.\n");
        TvScanBuffer = MEMAllocFromDefaultHeapEx(TvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        DrcScanBuffer = MEMAllocFromDefaultHeapEx(DrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        if (TvScanBuffer == nullptr || DrcScanBuffer == nullptr) {
            Shutdown();
            return false;
        }

        AppendInitializationTrace("[WiiUFile] GX2 initialize: bind scan buffers.\n");
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvScanBuffer, TvScanBufferSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, DrcScanBuffer, DrcScanBufferSize);
        GX2SetTVBuffer(TvScanBuffer, TvScanBufferSize, PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
        GX2SetDRCBuffer(DrcScanBuffer, DrcScanBufferSize, drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

        AppendInitializationTrace("[WiiUFile] GX2 initialize: create color and depth buffers.\n");
        InitializeTvColorBuffer();
        InitializeDrcColorBuffer();
        InitializeTvDepthBuffer();
        InitializeDrcDepthBuffer();
        TvContextState = static_cast<GX2ContextState*>(MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
        DrcContextState = static_cast<GX2ContextState*>(MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
        if (TvContextState == nullptr || DrcContextState == nullptr) {
            Shutdown();
            return false;
        }

        AppendInitializationTrace("[WiiUFile] GX2 initialize: setup TV context state.\n");
        GX2SetupContextStateEx(TvContextState, TRUE);
        GX2SetContextState(TvContextState);
        GX2SetColorBuffer(&TvColorBuffer, GX2_RENDER_TARGET_0);
        GX2SetDepthBuffer(&TvDepthBuffer);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(TvColorBuffer.surface.width), static_cast<float>(TvColorBuffer.surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, TvColorBuffer.surface.width, TvColorBuffer.surface.height);

        AppendInitializationTrace("[WiiUFile] GX2 initialize: setup DRC context state.\n");
        GX2SetupContextStateEx(DrcContextState, TRUE);
        GX2SetContextState(DrcContextState);
        GX2SetColorBuffer(&DrcColorBuffer, GX2_RENDER_TARGET_0);
        GX2SetDepthBuffer(&DrcDepthBuffer);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(DrcColorBuffer.surface.width), static_cast<float>(DrcColorBuffer.surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, DrcColorBuffer.surface.width, DrcColorBuffer.surface.height);
        AppendInitializationTrace("[WiiUFile] GX2 initialize: initialize presenter resources.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square resources begin.\n");
        InitializeDiagnosticSquareResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square resources completed.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic triangle resources begin.\n");
        InitializeDiagnosticTriangleResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic triangle resources completed.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: scene opaque resources begin.\n");
        InitializeSceneOpaqueResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: scene opaque resources completed.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: scene cube resources begin.\n");
        InitializeSceneCubeResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: scene cube resources completed.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: UI quad resources begin.\n");
        InitializeUiQuadResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: UI quad resources completed.\n");
        AppendInitializationTrace("[WiiUFile] GX2 initialize: finalize scales and swap interval.\n");
        GX2SetTVScale(TvSurfaceWidth, TvSurfaceHeight);
        GX2SetDRCScale(DrcSurfaceWidth, DrcSurfaceHeight);
        GX2SetSwapInterval(1);
        IsInitialized = true;
        AppendInitializationTrace("[WiiUFile] GX2 initialize: completed.\n");
        return true;
    }

    /// Appends one presenter trace message to the shared Wii U runtime trace file.
    void WiiUGx2Presenter::AppendInitializationTrace(const char* format, ...) {
        char buffer[2048];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);

        for (const char* tracePath : RuntimeTracePaths) {
            std::FILE* traceFile = std::fopen(tracePath, "a");
            if (traceFile == nullptr) {
                continue;
            }

            std::fputs(buffer, traceFile);
            std::fflush(traceFile);
            std::fclose(traceFile);
            return;
        }
    }

    /// Renders and presents one captured Wii U 2D frame.
    void WiiUGx2Presenter::RenderFrame(const WiiUGx2RenderFrame& frame) {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderFrame.");
        } else if (!AreUiQuadResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize UI quad resources before RenderFrame.");
        }

        RenderFrameToColorBuffer(TvContextState, &TvColorBuffer, frame, TvSurfaceWidth, TvSurfaceHeight);
        RenderFrameToColorBuffer(DrcContextState, &DrcColorBuffer, frame, DrcSurfaceWidth, DrcSurfaceHeight);
        PresentScanBuffers();
    }

    /// Renders and presents one captured Wii U 3D frame plus the captured 2D overlay.
    void WiiUGx2Presenter::RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D) {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderFrame.");
        } else if (!AreSceneOpaqueResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize opaque scene resources before 3D frame rendering.");
        } else if (!AreUiQuadResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize UI quad resources before 2D frame rendering.");
        }

        if (Scene3DDebugLogCount < 12U) {
            AppendInitializationTrace("[WiiUFile] GX2 presenter render begin drawCommands=%u quadCommands=%u hasCamera=%u\n",
                static_cast<unsigned>(frame3D.GetDrawCommands().size()),
                static_cast<unsigned>(frame2D.GetQuadCommands().size()),
                frame3D.GetHasCamera() ? 1U : 0U);
        }
        Render3DFrameToColorBuffer(TvContextState, &TvColorBuffer, &TvDepthBuffer, frame3D, TvSurfaceWidth, TvSurfaceHeight);
        RenderQuadCommandsToColorBuffer(frame2D, TvSurfaceWidth, TvSurfaceHeight);
        GX2DrawDone();
        Render3DFrameToColorBuffer(DrcContextState, &DrcColorBuffer, &DrcDepthBuffer, frame3D, DrcSurfaceWidth, DrcSurfaceHeight);
        RenderQuadCommandsToColorBuffer(frame2D, DrcSurfaceWidth, DrcSurfaceHeight);
        if (Scene3DDebugLogCount < 12U) {
            AppendInitializationTrace("[WiiUFile] GX2 presenter render completed drawCommands=%u quadCommands=%u hasCamera=%u\n",
                static_cast<unsigned>(frame3D.GetDrawCommands().size()),
                static_cast<unsigned>(frame2D.GetQuadCommands().size()),
                frame3D.GetHasCamera() ? 1U : 0U);
        }
        PresentScanBuffers();
    }

    /// Renders one presenter-owned pure GX2 clear-only frame for early bring-up verification.
    void WiiUGx2Presenter::RenderDiagnosticClearFrame() {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticClearFrame.");
        }

        GX2SetContextState(TvContextState);
        GX2ClearColor(&TvColorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
        GX2SetContextState(DrcContextState);
        GX2ClearColor(&DrcColorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
        PresentScanBuffers();
    }

    /// Renders one presenter-owned pure GX2 clear-plus-square frame for bring-up verification.
    void WiiUGx2Presenter::RenderDiagnosticSquareFrame() {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticSquareFrame.");
        }

        RenderDiagnosticSquareToColorBuffer(TvContextState, &TvColorBuffer);
        RenderDiagnosticSquareToColorBuffer(DrcContextState, &DrcColorBuffer);
        PresentScanBuffers();
    }

    /// Renders one presenter-owned pure GX2 clear-plus-triangle frame for first 3D shader verification.
    void WiiUGx2Presenter::RenderDiagnosticTriangleFrame() {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderDiagnosticTriangleFrame.");
        }

        RenderDiagnosticTriangleToColorBuffer(TvContextState, &TvColorBuffer);
        RenderDiagnosticTriangleToColorBuffer(DrcContextState, &DrcColorBuffer);
        PresentScanBuffers();
    }

    /// Uploads one runtime model into the temporary scene-cube GX2 mesh path.
    void WiiUGx2Presenter::ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel) {
        constexpr double SceneCubeYawRadians = 0.65;
        constexpr double SceneCubePitchRadians = 0.55;
        constexpr double SceneCubeFieldOfViewRadians = 1.0;
        constexpr double SceneCubeCameraDistance = 5.0;
        constexpr double SceneCubeNearPlane = 0.1;
        constexpr double SceneCubeFarPlane = 64.0;
        constexpr double SceneCubeAspectRatio = 1280.0 / 720.0;
        const double cosYaw = std::cos(SceneCubeYawRadians);
        const double sinYaw = std::sin(SceneCubeYawRadians);
        const double cosPitch = std::cos(SceneCubePitchRadians);
        const double sinPitch = std::sin(SceneCubePitchRadians);
        const double projectionScaleY = 1.0 / std::tan(SceneCubeFieldOfViewRadians * 0.5);
        const double projectionScaleX = projectionScaleY / SceneCubeAspectRatio;
        const double depthProjectionScale = (SceneCubeFarPlane + SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane);
        const double depthProjectionOffset = (2.0 * SceneCubeFarPlane * SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane);

        float4x4 worldViewProjectionMatrix;
        worldViewProjectionMatrix.M11 = static_cast<float>(projectionScaleX * cosYaw);
        worldViewProjectionMatrix.M21 = 0.0f;
        worldViewProjectionMatrix.M31 = static_cast<float>(projectionScaleX * sinYaw);
        worldViewProjectionMatrix.M41 = 0.0f;
        worldViewProjectionMatrix.M12 = static_cast<float>(projectionScaleY * sinYaw * sinPitch);
        worldViewProjectionMatrix.M22 = static_cast<float>(projectionScaleY * cosPitch);
        worldViewProjectionMatrix.M32 = static_cast<float>(-projectionScaleY * cosYaw * sinPitch);
        worldViewProjectionMatrix.M42 = 0.0f;
        worldViewProjectionMatrix.M13 = static_cast<float>(-depthProjectionScale * sinYaw * cosPitch);
        worldViewProjectionMatrix.M23 = static_cast<float>(depthProjectionScale * sinPitch);
        worldViewProjectionMatrix.M33 = static_cast<float>(depthProjectionScale * cosYaw * cosPitch);
        worldViewProjectionMatrix.M43 = static_cast<float>(-depthProjectionScale * SceneCubeCameraDistance + depthProjectionOffset);
        worldViewProjectionMatrix.M14 = static_cast<float>(sinYaw * cosPitch);
        worldViewProjectionMatrix.M24 = static_cast<float>(-sinPitch);
        worldViewProjectionMatrix.M34 = static_cast<float>(-cosYaw * cosPitch);
        worldViewProjectionMatrix.M44 = static_cast<float>(SceneCubeCameraDistance);

        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneCubeResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize scene cube resources before uploading geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube position data.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube index data.");
        }

        std::vector<float> expandedPositionData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t sourceOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            if (sourceOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one scene cube index outside the uploaded position range.");
            }

            const double sourceX = sourcePositionData[sourceOffset + 0U];
            const double sourceY = sourcePositionData[sourceOffset + 1U];
            const double sourceZ = sourcePositionData[sourceOffset + 2U];
            const double yawX = sourceX * cosYaw + sourceZ * sinYaw;
            const double yawZ = sourceZ * cosYaw - sourceX * sinYaw;
            const double pitchY = sourceY * cosPitch - yawZ * sinPitch;
            const double rotatedZ = sourceY * sinPitch + yawZ * cosPitch;
            const double viewZ = rotatedZ - SceneCubeCameraDistance;
            const double clipX = yawX * projectionScaleX;
            const double clipY = pitchY * projectionScaleY;
            const double clipZ =
                depthProjectionScale * viewZ +
                depthProjectionOffset;
            const double clipW = -viewZ;

            expandedPositionData.push_back(static_cast<float>(clipX));
            expandedPositionData.push_back(static_cast<float>(clipY));
            expandedPositionData.push_back(static_cast<float>(clipZ));
            expandedPositionData.push_back(static_cast<float>(clipW));
        }

        if (SceneCubePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubePositionBuffer, 0, sizeof(SceneCubePositionBuffer));
        }

        if (SceneCubeIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubeIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubeIndexBuffer, 0, sizeof(SceneCubeIndexBuffer));
        }

        InitializeSceneCubeVertexBuffer(&SceneCubePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()));
        InitializeSceneCubeIndexBuffer(&SceneCubeIndexBuffer, indexData.data(), static_cast<std::uint32_t>(indexData.size()));
        SceneCubeIndexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
        IsSceneCubeMeshConfigured = true;
    }

    /// Renders the configured scene-cube mesh to both displays.
    void WiiUGx2Presenter::RenderSceneCubeFrame() {
        if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before RenderSceneCubeFrame.");
        } else if (!AreSceneCubeResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize scene cube resources before RenderSceneCubeFrame.");
        } else if (!IsSceneCubeMeshConfigured) {
            throw std::runtime_error("Wii U GX2 presenter must upload scene cube geometry before RenderSceneCubeFrame.");
        }

        RenderSceneCubeToColorBuffer(TvContextState, &TvColorBuffer);
        RenderSceneCubeToColorBuffer(DrcContextState, &DrcColorBuffer);
        PresentScanBuffers();
    }

    /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
    void WiiUGx2Presenter::Shutdown() {
        DestroyUiQuadResources();
        DestroySceneOpaqueResources();
        DestroySceneCubeResources();
        DestroyDiagnosticTriangleResources();
        DestroyDiagnosticSquareResources();

        if (TvColorBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&TvColorBuffer.surface, NoGx2rResourceFlags);
            TvColorBuffer.surface.image = nullptr;
        }

        if (DrcColorBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&DrcColorBuffer.surface, NoGx2rResourceFlags);
            DrcColorBuffer.surface.image = nullptr;
        }

        if (TvDepthBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&TvDepthBuffer.surface, NoGx2rResourceFlags);
            TvDepthBuffer.surface.image = nullptr;
        }

        if (DrcDepthBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&DrcDepthBuffer.surface, NoGx2rResourceFlags);
            DrcDepthBuffer.surface.image = nullptr;
        }

        if (TvContextState != nullptr) {
            MEMFreeToDefaultHeap(TvContextState);
        }

        if (DrcContextState != nullptr) {
            MEMFreeToDefaultHeap(DrcContextState);
        }

        if (TvScanBuffer != nullptr) {
            MEMFreeToDefaultHeap(TvScanBuffer);
        }

        if (DrcScanBuffer != nullptr) {
            MEMFreeToDefaultHeap(DrcScanBuffer);
        }

        if (CommandBufferPool != nullptr) {
            MEMFreeToDefaultHeap(CommandBufferPool);
        }

        if (IsInitialized) {
            GX2Shutdown();
        }

        IsInitialized = false;
        TvScanBuffer = nullptr;
        DrcScanBuffer = nullptr;
        CommandBufferPool = nullptr;
        TvContextState = nullptr;
        DrcContextState = nullptr;
        TvScanBufferSize = 0U;
        DrcScanBufferSize = 0U;
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
        std::memset(&TvDepthBuffer, 0, sizeof(TvDepthBuffer));
        std::memset(&DrcDepthBuffer, 0, sizeof(DrcDepthBuffer));
    }

    /// Initializes the presenter-owned shader and vertex buffers used by the diagnostic GX2 square path.
    void WiiUGx2Presenter::InitializeDiagnosticSquareResources() {
        if (AreDiagnosticSquareResourcesInitialized) {
            return;
        }

        try {
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square load shader group begin.\n");
            if (!WHBGfxLoadGFDShaderGroup(&DiagnosticSquareShaderGroup, 0, diagnostic_square_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded diagnostic shader group.");
            }
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square load shader group completed.\n");

            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square bind position begin.\n");
            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic position shader attribute.");
            }
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square bind position completed.\n");

            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square bind color begin.\n");
            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aColor", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic color shader attribute.");
            }
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square bind color completed.\n");

            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square fetch shader begin.\n");
            if (!WHBGfxInitFetchShader(&DiagnosticSquareShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the diagnostic fetch shader.");
            }
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square fetch shader completed.\n");

            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square invalidate shaders begin.\n");
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.vertexShader->program, DiagnosticSquareShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.pixelShader->program, DiagnosticSquareShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.fetchShader.program, DiagnosticSquareShaderGroup.fetchShader.size);
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square invalidate shaders completed.\n");

            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square position buffer begin.\n");
            InitializeDiagnosticSquareBuffer(&DiagnosticSquarePositionBuffer, DiagnosticSquarePositionData, DiagnosticSquareVertexCount);
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square position buffer completed.\n");
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square color buffer begin.\n");
            InitializeDiagnosticSquareBuffer(&DiagnosticSquareColorBuffer, DiagnosticSquareColorData, DiagnosticSquareVertexCount);
            AppendInitializationTrace("[WiiUFile] GX2 initialize: diagnostic square color buffer completed.\n");
            AreDiagnosticSquareResourcesInitialized = true;
        } catch (...) {
            DestroyDiagnosticSquareResources();
            throw;
        }
    }

    /// Releases the presenter-owned shader and vertex buffers used by the diagnostic GX2 square path.
    void WiiUGx2Presenter::DestroyDiagnosticSquareResources() {
        if (DiagnosticSquarePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&DiagnosticSquarePositionBuffer, NoGx2rResourceFlags);
            std::memset(&DiagnosticSquarePositionBuffer, 0, sizeof(DiagnosticSquarePositionBuffer));
        }

        if (DiagnosticSquareColorBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&DiagnosticSquareColorBuffer, NoGx2rResourceFlags);
            std::memset(&DiagnosticSquareColorBuffer, 0, sizeof(DiagnosticSquareColorBuffer));
        }

        if (DiagnosticSquareShaderGroup.vertexShader != nullptr || DiagnosticSquareShaderGroup.pixelShader != nullptr || DiagnosticSquareShaderGroup.fetchShaderProgram != nullptr) {
            WHBGfxFreeShaderGroup(&DiagnosticSquareShaderGroup);
            std::memset(&DiagnosticSquareShaderGroup, 0, sizeof(DiagnosticSquareShaderGroup));
        }

        AreDiagnosticSquareResourcesInitialized = false;
    }

    /// Initializes one presenter-owned diagnostic vertex buffer from immutable float vertex data.
    void WiiUGx2Presenter::InitializeDiagnosticSquareBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t vertexCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid diagnostic vertex data.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticVertexBufferFlags;
        buffer->elemSize = DiagnosticSquareVertexElementSize;
        buffer->elemCount = vertexCount;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a diagnostic vertex buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock a diagnostic vertex buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, buffer->elemSize * buffer->elemCount);
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned shader and vertex buffers used by the diagnostic GX2 triangle path.
    void WiiUGx2Presenter::InitializeDiagnosticTriangleResources() {
        if (AreDiagnosticTriangleResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&DiagnosticTriangleShaderGroup, 0, diagnostic_triangle_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded diagnostic triangle shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticTriangleShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic triangle position shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticTriangleShaderGroup, "aColor", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic triangle color shader attribute.");
            }

            if (!WHBGfxInitFetchShader(&DiagnosticTriangleShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the diagnostic triangle fetch shader.");
            }

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticTriangleShaderGroup.vertexShader->program, DiagnosticTriangleShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticTriangleShaderGroup.pixelShader->program, DiagnosticTriangleShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticTriangleShaderGroup.fetchShader.program, DiagnosticTriangleShaderGroup.fetchShader.size);

            InitializeDiagnosticTriangleBuffer(&DiagnosticTrianglePositionBuffer, DiagnosticTrianglePositionData, DiagnosticTriangleVertexCount);
            InitializeDiagnosticTriangleBuffer(&DiagnosticTriangleColorBuffer, DiagnosticTriangleColorData, DiagnosticTriangleVertexCount);
            if (DiagnosticTriangleShaderGroup.vertexShader->uniformBlockCount != 0U) {
                InitializeDiagnosticTriangleTransformBuffer();
            }
            AreDiagnosticTriangleResourcesInitialized = true;
        } catch (...) {
            DestroyDiagnosticTriangleResources();
            throw;
        }
    }

    /// Releases the presenter-owned shader and vertex buffers used by the diagnostic GX2 triangle path.
    void WiiUGx2Presenter::DestroyDiagnosticTriangleResources() {
        if (DiagnosticTriangleTransformBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&DiagnosticTriangleTransformBuffer, NoGx2rResourceFlags);
            std::memset(&DiagnosticTriangleTransformBuffer, 0, sizeof(DiagnosticTriangleTransformBuffer));
        }

        if (DiagnosticTrianglePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&DiagnosticTrianglePositionBuffer, NoGx2rResourceFlags);
            std::memset(&DiagnosticTrianglePositionBuffer, 0, sizeof(DiagnosticTrianglePositionBuffer));
        }

        if (DiagnosticTriangleColorBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&DiagnosticTriangleColorBuffer, NoGx2rResourceFlags);
            std::memset(&DiagnosticTriangleColorBuffer, 0, sizeof(DiagnosticTriangleColorBuffer));
        }

        if (DiagnosticTriangleShaderGroup.vertexShader != nullptr || DiagnosticTriangleShaderGroup.pixelShader != nullptr || DiagnosticTriangleShaderGroup.fetchShaderProgram != nullptr) {
            WHBGfxFreeShaderGroup(&DiagnosticTriangleShaderGroup);
            std::memset(&DiagnosticTriangleShaderGroup, 0, sizeof(DiagnosticTriangleShaderGroup));
        }

        AreDiagnosticTriangleResourcesInitialized = false;
    }

    /// Initializes one presenter-owned diagnostic vertex buffer from immutable float vertex data for triangle rendering.
    void WiiUGx2Presenter::InitializeDiagnosticTriangleBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t vertexCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic triangle buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid diagnostic triangle vertex data.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticVertexBufferFlags;
        buffer->elemSize = DiagnosticTriangleVertexElementSize;
        buffer->elemCount = vertexCount;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a diagnostic triangle vertex buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock a diagnostic triangle vertex buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, buffer->elemSize * buffer->elemCount);
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned uniform buffer that stores the fixed transform used by the translated diagnostic triangle.
    void WiiUGx2Presenter::InitializeDiagnosticTriangleTransformBuffer() {
        if (DiagnosticTriangleShaderGroup.vertexShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic triangle vertex shader before allocating the transform buffer.");
        } else if (DiagnosticTriangleShaderGroup.vertexShader->uniformBlockCount == 0U || DiagnosticTriangleShaderGroup.vertexShader->uniformBlocks == nullptr) {
            return;
        }

        GX2UniformBlock* transformUniformBlock = GX2GetVertexUniformBlock(DiagnosticTriangleShaderGroup.vertexShader, "TransformBlock");
        if (transformUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the diagnostic triangle TransformBlock uniform block.");
        } else if (transformUniformBlock->size != DiagnosticTriangleTransformSizeInBytes) {
            throw std::runtime_error("Wii U GX2 presenter requires the diagnostic triangle transform buffer to match one 4x4 matrix.");
        }

        std::memset(&DiagnosticTriangleTransformBuffer, 0, sizeof(DiagnosticTriangleTransformBuffer));
        DiagnosticTriangleTransformBuffer.flags = DiagnosticUniformBufferFlags;
        DiagnosticTriangleTransformBuffer.elemSize = transformUniformBlock->size;
        DiagnosticTriangleTransformBuffer.elemCount = 1U;
        if (!GX2RCreateBuffer(&DiagnosticTriangleTransformBuffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a diagnostic triangle transform buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(&DiagnosticTriangleTransformBuffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock the diagnostic triangle transform buffer.");
        }

        std::memcpy(uploadBuffer, DiagnosticTriangleTransformData, transformUniformBlock->size);
        GX2RUnlockBufferEx(&DiagnosticTriangleTransformBuffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(&DiagnosticTriangleTransformBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned shader resources used by the generic opaque Wii U scene path.
    void WiiUGx2Presenter::InitializeSceneOpaqueResources() {
        if (AreSceneOpaqueResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&SceneOpaqueShaderGroup, 0, scene_opaque_lit_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded opaque scene shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&SceneOpaqueShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the opaque scene position shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&SceneOpaqueShaderGroup, "aNormal", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the opaque scene normal shader attribute.");
            }

            if (!WHBGfxInitFetchShader(&SceneOpaqueShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the opaque scene fetch shader.");
            }

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneOpaqueShaderGroup.vertexShader->program, SceneOpaqueShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneOpaqueShaderGroup.pixelShader->program, SceneOpaqueShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneOpaqueShaderGroup.fetchShader.program, SceneOpaqueShaderGroup.fetchShader.size);
            InitializeSceneOpaqueMaterialBuffer();
            InitializeSceneOpaqueLightBuffer();
            AreSceneOpaqueResourcesInitialized = true;
        } catch (...) {
            DestroySceneOpaqueResources();
            throw;
        }
    }

    /// Releases the presenter-owned shader resources used by the generic opaque Wii U scene path.
    void WiiUGx2Presenter::DestroySceneOpaqueResources() {
        if (SceneOpaqueLightBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueLightBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueLightBuffer, 0, sizeof(SceneOpaqueLightBuffer));
        }

        if (SceneOpaqueMaterialBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueMaterialBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueMaterialBuffer, 0, sizeof(SceneOpaqueMaterialBuffer));
        }

        if (SceneOpaqueTransformBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueTransformBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueTransformBuffer, 0, sizeof(SceneOpaqueTransformBuffer));
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr || SceneOpaqueNormalBuffer.buffer != nullptr || SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2DrawDone();
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaquePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaquePositionBuffer, 0, sizeof(SceneOpaquePositionBuffer));
        }

        if (SceneOpaqueNormalBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueNormalBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueNormalBuffer, 0, sizeof(SceneOpaqueNormalBuffer));
        }

        if (SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        }

        if (SceneOpaqueShaderGroup.vertexShader != nullptr || SceneOpaqueShaderGroup.pixelShader != nullptr || SceneOpaqueShaderGroup.fetchShaderProgram != nullptr) {
            WHBGfxFreeShaderGroup(&SceneOpaqueShaderGroup);
            std::memset(&SceneOpaqueShaderGroup, 0, sizeof(SceneOpaqueShaderGroup));
        }

        SceneOpaqueVertexCount = 0U;
        AreSceneOpaqueResourcesInitialized = false;
    }

    /// Initializes one presenter-owned opaque-scene vertex buffer from immutable float vertex data.
    void WiiUGx2Presenter::InitializeSceneOpaqueVertexBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t floatCount, std::uint32_t elementSize, std::uint32_t elementStride) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid opaque-scene vertex buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid opaque-scene vertex data.");
        } else if (floatCount == 0U || (floatCount % elementStride) != 0U) {
            throw std::runtime_error("Wii U GX2 presenter requires opaque-scene vertex data aligned to the requested stride.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticVertexBufferFlags;
        buffer->elemSize = elementSize;
        buffer->elemCount = floatCount / elementStride;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate an opaque-scene vertex buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock an opaque-scene vertex buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, static_cast<std::size_t>(buffer->elemSize) * static_cast<std::size_t>(buffer->elemCount));
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes one presenter-owned opaque-scene index buffer from immutable 16-bit index data.
    void WiiUGx2Presenter::InitializeSceneOpaqueIndexBuffer(GX2RBuffer* buffer, const std::uint16_t* sourceData, std::uint32_t indexCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid opaque-scene index buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid opaque-scene index data.");
        } else if (indexCount == 0U) {
            throw std::runtime_error("Wii U GX2 presenter requires at least one opaque-scene index.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticIndexBufferFlags;
        buffer->elemSize = SceneOpaqueIndexElementSize;
        buffer->elemCount = indexCount;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate an opaque-scene index buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock an opaque-scene index buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, static_cast<std::size_t>(SceneOpaqueIndexElementSize) * static_cast<std::size_t>(indexCount));
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned transform uniform buffer used by the generic opaque Wii U scene path.
    void WiiUGx2Presenter::InitializeSceneOpaqueTransformBuffer() {
        if (SceneOpaqueShaderGroup.vertexShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid opaque-scene vertex shader before allocating the transform buffer.");
        }

        GX2UniformBlock* transformUniformBlock = GX2GetVertexUniformBlock(SceneOpaqueShaderGroup.vertexShader, "TransformBlock");
        if (transformUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene TransformBlock uniform block.");
        } else if (transformUniformBlock->size != SceneOpaqueTransformSizeInBytes) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene transform buffer to match three 4x4 matrices.");
        }

        std::memset(&SceneOpaqueTransformBuffer, 0, sizeof(SceneOpaqueTransformBuffer));
        SceneOpaqueTransformBuffer.flags = DiagnosticUniformBufferFlags;
        SceneOpaqueTransformBuffer.elemSize = transformUniformBlock->size;
        SceneOpaqueTransformBuffer.elemCount = 1U;
        if (!GX2RCreateBuffer(&SceneOpaqueTransformBuffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate an opaque-scene transform buffer.");
        }
    }

    /// Initializes the presenter-owned material uniform buffer used by the generic opaque Wii U scene path.
    void WiiUGx2Presenter::InitializeSceneOpaqueMaterialBuffer() {
        if (SceneOpaqueShaderGroup.pixelShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid opaque-scene pixel shader before allocating the material buffer.");
        }

        GX2UniformBlock* materialUniformBlock = GX2GetPixelUniformBlock(SceneOpaqueShaderGroup.pixelShader, "MaterialBlock");
        if (materialUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene MaterialBlock uniform block.");
        } else if (materialUniformBlock->size != SceneOpaqueMaterialSizeInBytes) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene material buffer to match two float4 values.");
        }

        std::memset(&SceneOpaqueMaterialBuffer, 0, sizeof(SceneOpaqueMaterialBuffer));
        SceneOpaqueMaterialBuffer.flags = DiagnosticUniformBufferFlags;
        SceneOpaqueMaterialBuffer.elemSize = materialUniformBlock->size;
        SceneOpaqueMaterialBuffer.elemCount = 1U;
        if (!GX2RCreateBuffer(&SceneOpaqueMaterialBuffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate an opaque-scene material buffer.");
        }
    }

    /// Initializes the presenter-owned light uniform buffer used by the generic opaque Wii U scene path.
    void WiiUGx2Presenter::InitializeSceneOpaqueLightBuffer() {
        if (SceneOpaqueShaderGroup.pixelShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid opaque-scene pixel shader before allocating the light buffer.");
        }

        GX2UniformBlock* lightUniformBlock = GX2GetPixelUniformBlock(SceneOpaqueShaderGroup.pixelShader, "LightBlock");
        if (lightUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene LightBlock uniform block.");
        } else if (lightUniformBlock->size != SceneOpaqueLightSizeInBytes) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene light buffer to match three float4 values.");
        }

        std::memset(&SceneOpaqueLightBuffer, 0, sizeof(SceneOpaqueLightBuffer));
        SceneOpaqueLightBuffer.flags = DiagnosticUniformBufferFlags;
        SceneOpaqueLightBuffer.elemSize = lightUniformBlock->size;
        SceneOpaqueLightBuffer.elemCount = 1U;
        if (!GX2RCreateBuffer(&SceneOpaqueLightBuffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate an opaque-scene light buffer.");
        }
    }

    /// Initializes the presenter-owned shader resources used by the temporary scene-cube GX2 mesh path.
    void WiiUGx2Presenter::InitializeSceneCubeResources() {
        if (AreSceneCubeResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&SceneCubeShaderGroup, 0, scene_cube_flat_color_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded scene cube shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&SceneCubeShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the scene cube position shader attribute.");
            }

            if (!WHBGfxInitFetchShader(&SceneCubeShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the scene cube fetch shader.");
            }

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneCubeShaderGroup.vertexShader->program, SceneCubeShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneCubeShaderGroup.pixelShader->program, SceneCubeShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, SceneCubeShaderGroup.fetchShader.program, SceneCubeShaderGroup.fetchShader.size);
            InitializeSceneCubeTransformBuffer();
            AreSceneCubeResourcesInitialized = true;
        } catch (...) {
            DestroySceneCubeResources();
            throw;
        }
    }

    /// Releases the presenter-owned shader resources used by the temporary scene-cube GX2 mesh path.
    void WiiUGx2Presenter::DestroySceneCubeResources() {
        if (SceneCubeTransformBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubeTransformBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubeTransformBuffer, 0, sizeof(SceneCubeTransformBuffer));
        }

        if (SceneCubePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubePositionBuffer, 0, sizeof(SceneCubePositionBuffer));
        }

        if (SceneCubeIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubeIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubeIndexBuffer, 0, sizeof(SceneCubeIndexBuffer));
        }

        if (SceneCubeShaderGroup.vertexShader != nullptr || SceneCubeShaderGroup.pixelShader != nullptr || SceneCubeShaderGroup.fetchShaderProgram != nullptr) {
            WHBGfxFreeShaderGroup(&SceneCubeShaderGroup);
            std::memset(&SceneCubeShaderGroup, 0, sizeof(SceneCubeShaderGroup));
        }

        SceneCubeIndexCount = 0U;
        IsSceneCubeMeshConfigured = false;
        AreSceneCubeResourcesInitialized = false;
    }

    /// Initializes one presenter-owned scene-cube vertex buffer from immutable float vertex data.
    void WiiUGx2Presenter::InitializeSceneCubeVertexBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t floatCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid scene cube position buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid scene cube position data.");
        } else if (floatCount == 0U || (floatCount % 4U) != 0U) {
            throw std::runtime_error("Wii U GX2 presenter requires scene cube position data stored as XYZW float quads.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticVertexBufferFlags;
        buffer->elemSize = SceneCubeVertexElementSize;
        buffer->elemCount = floatCount / 4U;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a scene cube position buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock a scene cube position buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, static_cast<std::size_t>(buffer->elemSize) * static_cast<std::size_t>(buffer->elemCount));
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes one presenter-owned scene-cube index buffer from immutable 16-bit index data.
    void WiiUGx2Presenter::InitializeSceneCubeIndexBuffer(GX2RBuffer* buffer, const std::uint16_t* sourceData, std::uint32_t indexCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid scene cube index buffer.");
        } else if (sourceData == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires valid scene cube index data.");
        } else if (indexCount == 0U) {
            throw std::runtime_error("Wii U GX2 presenter requires at least one scene cube index.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticIndexBufferFlags;
        buffer->elemSize = SceneCubeIndexElementSize;
        buffer->elemCount = indexCount;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a scene cube index buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock a scene cube index buffer.");
        }

        std::memcpy(uploadBuffer, sourceData, static_cast<std::size_t>(SceneCubeIndexElementSize) * static_cast<std::size_t>(indexCount));
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned uniform buffer that stores the fixed transform used by the scene-cube path.
    void WiiUGx2Presenter::InitializeSceneCubeTransformBuffer() {
        if (SceneCubeShaderGroup.vertexShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid scene cube vertex shader before allocating the transform buffer.");
        } else if (SceneCubeShaderGroup.vertexShader->uniformBlockCount == 0U || SceneCubeShaderGroup.vertexShader->uniformBlocks == nullptr) {
            return;
        }

        GX2UniformBlock* transformUniformBlock = GX2GetVertexUniformBlock(SceneCubeShaderGroup.vertexShader, "TransformBlock");
        if (transformUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the scene cube TransformBlock uniform block.");
        } else if (transformUniformBlock->size != SceneCubeTransformSizeInBytes) {
            throw std::runtime_error("Wii U GX2 presenter requires the scene cube transform buffer to match one 4x4 matrix.");
        }

        std::memset(&SceneCubeTransformBuffer, 0, sizeof(SceneCubeTransformBuffer));
        SceneCubeTransformBuffer.flags = DiagnosticUniformBufferFlags;
        SceneCubeTransformBuffer.elemSize = transformUniformBlock->size;
        SceneCubeTransformBuffer.elemCount = 1U;
        if (!GX2RCreateBuffer(&SceneCubeTransformBuffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a scene cube transform buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(&SceneCubeTransformBuffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock the scene cube transform buffer.");
        }

        std::memcpy(uploadBuffer, SceneCubeTransformData, transformUniformBlock->size);
        GX2RUnlockBufferEx(&SceneCubeTransformBuffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(&SceneCubeTransformBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Initializes the presenter-owned shader, buffers, and fallback texture used by the pure GX2 UI path.
    void WiiUGx2Presenter::InitializeUiQuadResources() {
        if (AreUiQuadResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&UiQuadShaderGroup, 0, ui_quad_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded UI quad shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&UiQuadShaderGroup, "aPos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the UI position shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&UiQuadShaderGroup, "aTexCoord", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the UI texture-coordinate shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&UiQuadShaderGroup, "aColor", 2, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the UI color shader attribute.");
            }

            if (!WHBGfxInitFetchShader(&UiQuadShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the UI fetch shader.");
            }

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, UiQuadShaderGroup.vertexShader->program, UiQuadShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, UiQuadShaderGroup.pixelShader->program, UiQuadShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, UiQuadShaderGroup.fetchShader.program, UiQuadShaderGroup.fetchShader.size);

            InitializeUiQuadBuffer(&UiQuadPositionBuffer, UiQuadPositionElementSize, UiQuadVertexCount);
            InitializeUiQuadBuffer(&UiQuadTexCoordBuffer, UiQuadTexCoordElementSize, UiQuadVertexCount);
            InitializeUiQuadBuffer(&UiQuadColorBuffer, UiQuadColorElementSize, UiQuadVertexCount);
            InitializeUiSolidWhiteTexture();
            AreUiQuadResourcesInitialized = true;
        } catch (...) {
            DestroyUiQuadResources();
            throw;
        }
    }

    /// Releases the presenter-owned shader, buffers, and fallback texture used by the pure GX2 UI path.
    void WiiUGx2Presenter::DestroyUiQuadResources() {
        if (UiQuadPositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadPositionBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadPositionBuffer, 0, sizeof(UiQuadPositionBuffer));
        }

        if (UiQuadTexCoordBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadTexCoordBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadTexCoordBuffer, 0, sizeof(UiQuadTexCoordBuffer));
        }

        if (UiQuadColorBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadColorBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadColorBuffer, 0, sizeof(UiQuadColorBuffer));
        }

        DestroyTextureHandle(&UiSolidWhiteTextureHandle);

        if (UiQuadShaderGroup.vertexShader != nullptr || UiQuadShaderGroup.pixelShader != nullptr || UiQuadShaderGroup.fetchShaderProgram != nullptr) {
            WHBGfxFreeShaderGroup(&UiQuadShaderGroup);
            std::memset(&UiQuadShaderGroup, 0, sizeof(UiQuadShaderGroup));
        }

        AreUiQuadResourcesInitialized = false;
    }

    /// Initializes one presenter-owned UI vertex buffer for dynamic quad data.
    void WiiUGx2Presenter::InitializeUiQuadBuffer(GX2RBuffer* buffer, std::uint32_t elementSize, std::uint32_t elementCount) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid UI quad buffer.");
        } else if (elementSize == 0U || elementCount == 0U) {
            throw std::runtime_error("Wii U GX2 presenter requires nonzero UI quad buffer dimensions.");
        }

        std::memset(buffer, 0, sizeof(GX2RBuffer));
        buffer->flags = DiagnosticVertexBufferFlags;
        buffer->elemSize = elementSize;
        buffer->elemCount = elementCount;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate a UI quad vertex buffer.");
        }

        void* uploadBuffer = GX2RLockBufferEx(buffer, NoGx2rResourceFlags);
        if (uploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock a UI quad vertex buffer.");
        }

        std::memset(uploadBuffer, 0, buffer->elemSize * buffer->elemCount);
        GX2RUnlockBufferEx(buffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    }

    /// Grows the presenter-owned UI buffers so one full captured frame can be uploaded without overwriting in-flight quad data.
    void WiiUGx2Presenter::EnsureUiQuadBufferCapacity(std::uint32_t requiredVertexCount) {
        if (requiredVertexCount == 0U) {
            return;
        } else if (
            UiQuadPositionBuffer.buffer != nullptr
            && UiQuadTexCoordBuffer.buffer != nullptr
            && UiQuadColorBuffer.buffer != nullptr
            && UiQuadPositionBuffer.elemCount >= requiredVertexCount
            && UiQuadTexCoordBuffer.elemCount >= requiredVertexCount
            && UiQuadColorBuffer.elemCount >= requiredVertexCount) {
            return;
        }

        if (UiQuadPositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadPositionBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadPositionBuffer, 0, sizeof(UiQuadPositionBuffer));
        }

        if (UiQuadTexCoordBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadTexCoordBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadTexCoordBuffer, 0, sizeof(UiQuadTexCoordBuffer));
        }

        if (UiQuadColorBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&UiQuadColorBuffer, NoGx2rResourceFlags);
            std::memset(&UiQuadColorBuffer, 0, sizeof(UiQuadColorBuffer));
        }

        InitializeUiQuadBuffer(&UiQuadPositionBuffer, UiQuadPositionElementSize, requiredVertexCount);
        InitializeUiQuadBuffer(&UiQuadTexCoordBuffer, UiQuadTexCoordElementSize, requiredVertexCount);
        InitializeUiQuadBuffer(&UiQuadColorBuffer, UiQuadColorElementSize, requiredVertexCount);
    }

    /// Initializes the presenter-owned 1x1 white texture used for solid-color quad rendering.
    void WiiUGx2Presenter::InitializeUiSolidWhiteTexture() {
        std::memset(&UiSolidWhiteTextureHandle.Texture, 0, sizeof(UiSolidWhiteTextureHandle.Texture));
        std::memset(&UiSolidWhiteTextureHandle.Sampler, 0, sizeof(UiSolidWhiteTextureHandle.Sampler));

        UiSolidWhiteTextureHandle.Texture.surface.use = GX2_SURFACE_USE_TEXTURE;
        UiSolidWhiteTextureHandle.Texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        UiSolidWhiteTextureHandle.Texture.surface.width = 1U;
        UiSolidWhiteTextureHandle.Texture.surface.height = 1U;
        UiSolidWhiteTextureHandle.Texture.surface.depth = 1U;
        UiSolidWhiteTextureHandle.Texture.surface.mipLevels = 1U;
        UiSolidWhiteTextureHandle.Texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        UiSolidWhiteTextureHandle.Texture.surface.aa = GX2_AA_MODE1X;
        UiSolidWhiteTextureHandle.Texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        GX2CalcSurfaceSizeAndAlignment(&UiSolidWhiteTextureHandle.Texture.surface);
        if (!GX2RCreateSurface(&UiSolidWhiteTextureHandle.Texture.surface, TextureSurfaceFlags)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the solid-white UI texture.");
        }

        UiSolidWhiteTextureHandle.Texture.viewFirstMip = 0U;
        UiSolidWhiteTextureHandle.Texture.viewNumMips = 1U;
        UiSolidWhiteTextureHandle.Texture.viewFirstSlice = 0U;
        UiSolidWhiteTextureHandle.Texture.viewNumSlices = 1U;
        UiSolidWhiteTextureHandle.Texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
        GX2InitTextureRegs(&UiSolidWhiteTextureHandle.Texture);
        GX2InitSampler(&UiSolidWhiteTextureHandle.Sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

        std::uint32_t* destinationPixels = static_cast<std::uint32_t*>(GX2RLockSurfaceEx(&UiSolidWhiteTextureHandle.Texture.surface, 0, NoGx2rResourceFlags));
        if (destinationPixels == nullptr) {
            DestroyTextureHandle(&UiSolidWhiteTextureHandle);
            throw std::runtime_error("Wii U GX2 presenter could not lock the solid-white UI texture.");
        }

        destinationPixels[0] = (SolidWhitePixel & 0xFF00FF00U)
            | ((SolidWhitePixel & 0x00FF0000U) >> 16)
            | ((SolidWhitePixel & 0x000000FFU) << 16);
        GX2RUnlockSurfaceEx(&UiSolidWhiteTextureHandle.Texture.surface, 0, NoGx2rResourceFlags);
        GX2RInvalidateSurface(&UiSolidWhiteTextureHandle.Texture.surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, UiSolidWhiteTextureHandle.Texture.surface.image, UiSolidWhiteTextureHandle.Texture.surface.imageSize);
    }

    /// Releases one presenter-owned texture handle.
    void WiiUGx2Presenter::DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle) {
        if (textureHandle == nullptr) {
            return;
        }

        if (textureHandle->Texture.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&textureHandle->Texture.surface, NoGx2rResourceFlags);
        }

        std::memset(&textureHandle->Texture, 0, sizeof(textureHandle->Texture));
        std::memset(&textureHandle->Sampler, 0, sizeof(textureHandle->Sampler));
    }

    /// Initializes the TV color buffer used for steady-state presentation.
    void WiiUGx2Presenter::InitializeTvColorBuffer() {
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        TvColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        TvColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        TvColorBuffer.surface.width = TvSurfaceWidth;
        TvColorBuffer.surface.height = TvSurfaceHeight;
        TvColorBuffer.surface.depth = 1U;
        TvColorBuffer.surface.mipLevels = 1U;
        TvColorBuffer.surface.format = PresentationSurfaceFormat;
        TvColorBuffer.surface.aa = GX2_AA_MODE1X;
        TvColorBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        TvColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&TvColorBuffer.surface);
        if (!GX2RCreateSurface(
            &TvColorBuffer.surface,
            GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the TV color buffer.");
        }

        GX2InitColorBufferRegs(&TvColorBuffer);
    }

    /// Initializes the DRC color buffer used for steady-state presentation.
    void WiiUGx2Presenter::InitializeDrcColorBuffer() {
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
        DrcColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        DrcColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        DrcColorBuffer.surface.width = DrcSurfaceWidth;
        DrcColorBuffer.surface.height = DrcSurfaceHeight;
        DrcColorBuffer.surface.depth = 1U;
        DrcColorBuffer.surface.mipLevels = 1U;
        DrcColorBuffer.surface.format = PresentationSurfaceFormat;
        DrcColorBuffer.surface.aa = GX2_AA_MODE1X;
        DrcColorBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        DrcColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&DrcColorBuffer.surface);
        if (!GX2RCreateSurface(
            &DrcColorBuffer.surface,
            GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the DRC color buffer.");
        }

        GX2InitColorBufferRegs(&DrcColorBuffer);
    }

    /// Initializes the TV depth buffer used for scene-driven 3D depth testing.
    void WiiUGx2Presenter::InitializeTvDepthBuffer() {
        std::memset(&TvDepthBuffer, 0, sizeof(TvDepthBuffer));
        TvDepthBuffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
        TvDepthBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        TvDepthBuffer.surface.width = TvSurfaceWidth;
        TvDepthBuffer.surface.height = TvSurfaceHeight;
        TvDepthBuffer.surface.depth = 1U;
        TvDepthBuffer.surface.mipLevels = 1U;
        TvDepthBuffer.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
        TvDepthBuffer.surface.aa = GX2_AA_MODE1X;
        TvDepthBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        TvDepthBuffer.viewNumSlices = 1U;
        TvDepthBuffer.depthClear = 1.0f;
        TvDepthBuffer.stencilClear = 0U;
        GX2CalcSurfaceSizeAndAlignment(&TvDepthBuffer.surface);
        if (!GX2RCreateSurface(
            &TvDepthBuffer.surface,
            GX2R_RESOURCE_BIND_DEPTH_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the TV depth buffer.");
        }

        GX2InitDepthBufferRegs(&TvDepthBuffer);
    }

    /// Initializes the DRC depth buffer used for scene-driven 3D depth testing.
    void WiiUGx2Presenter::InitializeDrcDepthBuffer() {
        std::memset(&DrcDepthBuffer, 0, sizeof(DrcDepthBuffer));
        DrcDepthBuffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
        DrcDepthBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        DrcDepthBuffer.surface.width = DrcSurfaceWidth;
        DrcDepthBuffer.surface.height = DrcSurfaceHeight;
        DrcDepthBuffer.surface.depth = 1U;
        DrcDepthBuffer.surface.mipLevels = 1U;
        DrcDepthBuffer.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
        DrcDepthBuffer.surface.aa = GX2_AA_MODE1X;
        DrcDepthBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        DrcDepthBuffer.viewNumSlices = 1U;
        DrcDepthBuffer.depthClear = 1.0f;
        DrcDepthBuffer.stencilClear = 0U;
        GX2CalcSurfaceSizeAndAlignment(&DrcDepthBuffer.surface);
        if (!GX2RCreateSurface(
            &DrcDepthBuffer.surface,
            GX2R_RESOURCE_BIND_DEPTH_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the DRC depth buffer.");
        }

        GX2InitDepthBufferRegs(&DrcDepthBuffer);
    }

    /// Renders one captured frame into one target color buffer.
    void WiiUGx2Presenter::RenderFrameToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer, const WiiUGx2RenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid frame context state.");
        } else if (colorBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid frame color buffer.");
        }

        const std::vector<WiiUGx2QuadCommand>& quadCommands = frame.GetQuadCommands();
        const WiiUGx2Color& clearColor = frame.GetClearColor();
        GX2SetContextState(contextState);
        GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
        GX2ClearColor(
            colorBuffer,
            static_cast<float>(clearColor.Red) / 255.0f,
            static_cast<float>(clearColor.Green) / 255.0f,
            static_cast<float>(clearColor.Blue) / 255.0f,
            static_cast<float>(clearColor.Alpha) / 255.0f);

        RenderQuadCommandsToColorBuffer(frame, targetWidth, targetHeight);
    }

    /// Renders one captured 3D frame into one target color buffer using one paired depth buffer.
    void WiiUGx2Presenter::Render3DFrameToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer, GX2DepthBuffer* depthBuffer, const WiiUGx23DRenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid 3D frame context state.");
        } else if (colorBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid 3D frame color buffer.");
        } else if (depthBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid 3D frame depth buffer.");
        }

        const WiiUGx2Color& clearColor = frame.GetClearColor();
        GX2SetContextState(contextState);
        GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
        GX2SetDepthBuffer(depthBuffer);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
        GX2ClearColor(
            colorBuffer,
            static_cast<float>(clearColor.Red) / 255.0f,
            static_cast<float>(clearColor.Green) / 255.0f,
            static_cast<float>(clearColor.Blue) / 255.0f,
            static_cast<float>(clearColor.Alpha) / 255.0f);
        GX2ClearDepthStencilEx(depthBuffer, 1.0f, 0U, GX2_CLEAR_FLAGS_DEPTH);

        if (!frame.GetHasCamera()) {
            return;
        }

        GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LEQUAL);

        const WiiUGx23DCameraState& cameraState = frame.GetCamera();
        const std::vector<WiiUGx23DDrawCommand>& drawCommands = frame.GetDrawCommands();
        for (std::size_t commandIndex = 0; commandIndex < drawCommands.size(); commandIndex++) {
            Render3DDrawCommandToColorBuffer(drawCommands[commandIndex], frame, cameraState, targetWidth, targetHeight);
        }
    }

    /// Renders the captured 2D quad commands into the currently bound color buffer without clearing it first.
    void WiiUGx2Presenter::RenderQuadCommandsToColorBuffer(const WiiUGx2RenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight) {
        const std::vector<WiiUGx2QuadCommand>& quadCommands = frame.GetQuadCommands();

        GX2SetFetchShader(&UiQuadShaderGroup.fetchShader);
        GX2SetVertexShader(UiQuadShaderGroup.vertexShader);
        GX2SetPixelShader(UiQuadShaderGroup.pixelShader);
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
        GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x1, FALSE, TRUE);
        GX2SetBlendControl(
            GX2_RENDER_TARGET_0,
            GX2_BLEND_MODE_SRC_ALPHA,
            GX2_BLEND_MODE_INV_SRC_ALPHA,
            GX2_BLEND_COMBINE_MODE_ADD,
            TRUE,
            GX2_BLEND_MODE_ONE,
            GX2_BLEND_MODE_INV_SRC_ALPHA,
            GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetTargetChannelMasks(
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA);

        if (quadCommands.empty()) {
            return;
        }

        const std::uint32_t totalVertexCount = static_cast<std::uint32_t>(quadCommands.size()) * UiQuadVertexCount;
        EnsureUiQuadBufferCapacity(totalVertexCount);

        std::vector<float> framePositionData(static_cast<std::size_t>(totalVertexCount) * 2U);
        std::vector<float> frameTexCoordData(static_cast<std::size_t>(totalVertexCount) * 2U);
        std::vector<float> frameColorData(static_cast<std::size_t>(totalVertexCount) * 4U);
        for (std::size_t commandIndex = 0; commandIndex < quadCommands.size(); commandIndex++) {
            const WiiUGx2QuadCommand& command = quadCommands[commandIndex];
            const float left = (command.X / static_cast<float>(LogicalFrameWidth)) * 2.0f - 1.0f;
            const float right = ((command.X + command.Width) / static_cast<float>(LogicalFrameWidth)) * 2.0f - 1.0f;
            const float top = 1.0f - ((command.Y / static_cast<float>(LogicalFrameHeight)) * 2.0f);
            const float bottom = 1.0f - (((command.Y + command.Height) / static_cast<float>(LogicalFrameHeight)) * 2.0f);
            const float texLeft = command.SourceX;
            const float texTop = command.SourceY;
            const float texRight = command.SourceX + command.SourceWidth;
            const float texBottom = command.SourceY + command.SourceHeight;
            const float colorRed = static_cast<float>(command.Color.Red) / 255.0f;
            const float colorGreen = static_cast<float>(command.Color.Green) / 255.0f;
            const float colorBlue = static_cast<float>(command.Color.Blue) / 255.0f;
            const float colorAlpha = static_cast<float>(command.Color.Alpha) / 255.0f;

            const std::size_t vertexStartIndex = commandIndex * UiQuadVertexCount;
            const std::size_t positionStartIndex = vertexStartIndex * 2U;
            const std::size_t texCoordStartIndex = vertexStartIndex * 2U;
            const std::size_t colorStartIndex = vertexStartIndex * 4U;
            const float positionData[] = {
                left, bottom,
                right, bottom,
                right, top,
                left, bottom,
                right, top,
                left, top
            };
            const float texCoordData[] = {
                texLeft, texBottom,
                texRight, texBottom,
                texRight, texTop,
                texLeft, texBottom,
                texRight, texTop,
                texLeft, texTop
            };
            const float colorData[] = {
                colorRed, colorGreen, colorBlue, colorAlpha,
                colorRed, colorGreen, colorBlue, colorAlpha,
                colorRed, colorGreen, colorBlue, colorAlpha,
                colorRed, colorGreen, colorBlue, colorAlpha,
                colorRed, colorGreen, colorBlue, colorAlpha,
                colorRed, colorGreen, colorBlue, colorAlpha
            };
            std::memcpy(framePositionData.data() + positionStartIndex, positionData, sizeof(positionData));
            std::memcpy(frameTexCoordData.data() + texCoordStartIndex, texCoordData, sizeof(texCoordData));
            std::memcpy(frameColorData.data() + colorStartIndex, colorData, sizeof(colorData));
        }

        void* positionUploadBuffer = GX2RLockBufferEx(&UiQuadPositionBuffer, NoGx2rResourceFlags);
        void* texCoordUploadBuffer = GX2RLockBufferEx(&UiQuadTexCoordBuffer, NoGx2rResourceFlags);
        void* colorUploadBuffer = GX2RLockBufferEx(&UiQuadColorBuffer, NoGx2rResourceFlags);
        if (positionUploadBuffer == nullptr || texCoordUploadBuffer == nullptr || colorUploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock one of the frame UI quad buffers.");
        }

        std::memcpy(positionUploadBuffer, framePositionData.data(), framePositionData.size() * sizeof(float));
        std::memcpy(texCoordUploadBuffer, frameTexCoordData.data(), frameTexCoordData.size() * sizeof(float));
        std::memcpy(colorUploadBuffer, frameColorData.data(), frameColorData.size() * sizeof(float));
        GX2RUnlockBufferEx(&UiQuadPositionBuffer, NoGx2rResourceFlags);
        GX2RUnlockBufferEx(&UiQuadTexCoordBuffer, NoGx2rResourceFlags);
        GX2RUnlockBufferEx(&UiQuadColorBuffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(&UiQuadPositionBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2RInvalidateBuffer(&UiQuadTexCoordBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2RInvalidateBuffer(&UiQuadColorBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);

        for (std::size_t commandIndex = 0; commandIndex < quadCommands.size(); commandIndex++) {
            RenderQuadCommandToColorBuffer(quadCommands[commandIndex], static_cast<std::uint32_t>(commandIndex), LogicalFrameWidth, LogicalFrameHeight, targetWidth, targetHeight);
        }
    }

    /// Renders one captured 3D draw command into the currently bound color buffer.
    void WiiUGx2Presenter::Render3DDrawCommandToColorBuffer(const WiiUGx23DDrawCommand& drawCommand, const WiiUGx23DRenderFrame& frame, const WiiUGx23DCameraState& cameraState, std::uint32_t targetWidth, std::uint32_t targetHeight) {
        if (drawCommand.RuntimeModel == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires one runtime model for 3D draw submission.");
        } else if (drawCommand.RuntimeMaterial == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires one runtime material for 3D draw submission.");
        } else if (targetWidth == 0U || targetHeight == 0U) {
            return;
        }

        float4x4 projectionMatrix;
        float4x4::CreatePerspectiveFieldOfView__out4(
            static_cast<float>(SceneDrivenFieldOfViewRadians),
            static_cast<float>(static_cast<double>(targetWidth) / static_cast<double>(targetHeight)),
            cameraState.NearPlaneDistance,
            cameraState.FarPlaneDistance,
            projectionMatrix);

        float4x4 worldMatrix = drawCommand.WorldMatrix;
        float4x4 viewMatrix = cameraState.ViewMatrix;

        const std::vector<float>& sourcePositionData = drawCommand.RuntimeModel->GetPositionData();
        float4x4 worldViewMatrix;
        float4x4 worldViewProjectionMatrix;
        float4x4::Multiply__ref0_ref1_out2(worldMatrix, viewMatrix, worldViewMatrix);
        float4x4::Multiply__ref0_ref1_out2(worldViewMatrix, projectionMatrix, worldViewProjectionMatrix);
        const bool shouldLogOpaqueDraw = Scene3DDebugLogCount < 12U;
        if (sourcePositionData.size() >= 4U) {
            const float sourceX = sourcePositionData[0];
            const float sourceY = sourcePositionData[1];
            const float sourceZ = sourcePositionData[2];
            const float sourceW = sourcePositionData[3];
            const float clipX =
                (sourceX * worldViewProjectionMatrix.M11) +
                (sourceY * worldViewProjectionMatrix.M21) +
                (sourceZ * worldViewProjectionMatrix.M31) +
                (sourceW * worldViewProjectionMatrix.M41);
            const float clipY =
                (sourceX * worldViewProjectionMatrix.M12) +
                (sourceY * worldViewProjectionMatrix.M22) +
                (sourceZ * worldViewProjectionMatrix.M32) +
                (sourceW * worldViewProjectionMatrix.M42);
            const float clipZ =
                (sourceX * worldViewProjectionMatrix.M13) +
                (sourceY * worldViewProjectionMatrix.M23) +
                (sourceZ * worldViewProjectionMatrix.M33) +
                (sourceW * worldViewProjectionMatrix.M43);
            const float clipW =
                (sourceX * worldViewProjectionMatrix.M14) +
                (sourceY * worldViewProjectionMatrix.M24) +
                (sourceZ * worldViewProjectionMatrix.M34) +
                (sourceW * worldViewProjectionMatrix.M44);
            const float ndcX = clipW != 0.0f ? clipX / clipW : 0.0f;
            const float ndcY = clipW != 0.0f ? clipY / clipW : 0.0f;
            const float ndcZ = clipW != 0.0f ? clipZ / clipW : 0.0f;
            if (shouldLogOpaqueDraw) {
                AppendInitializationTrace(
                    "[WiiUFile] Opaque draw setup target=%ux%u cpuClipSpace=1 worldNormals=1 lightBlock=1 opaqueShader=1 nonIndexed=1 noCull=1 positions=%u normals=%u indices=%u firstClip=(%f,%f,%f,%f) firstNdc=(%f,%f,%f)\n",
                    targetWidth,
                    targetHeight,
                    static_cast<unsigned>(sourcePositionData.size() / 4U),
                    static_cast<unsigned>(drawCommand.RuntimeModel->GetNormalData().size() / 3U),
                    static_cast<unsigned>(drawCommand.RuntimeModel->GetIndexData().size()),
                    clipX,
                    clipY,
                    clipZ,
                    clipW,
                    ndcX,
                    ndcY,
                    ndcZ);
            }
        }

        if (shouldLogOpaqueDraw && sourcePositionData.size() >= 12U && drawCommand.RuntimeModel->GetNormalData().size() >= 9U && drawCommand.RuntimeModel->GetIndexData().size() >= 3U) {
            const std::vector<std::uint16_t>& sourceIndices = drawCommand.RuntimeModel->GetIndexData();
            const std::vector<float>& sourceNormals = drawCommand.RuntimeModel->GetNormalData();
            const std::size_t index0 = static_cast<std::size_t>(sourceIndices[0]);
            const std::size_t index1 = static_cast<std::size_t>(sourceIndices[1]);
            const std::size_t index2 = static_cast<std::size_t>(sourceIndices[2]);
            const std::size_t positionOffset0 = index0 * 4U;
            const std::size_t positionOffset1 = index1 * 4U;
            const std::size_t positionOffset2 = index2 * 4U;
            const std::size_t normalOffset0 = index0 * 3U;
            if (positionOffset2 + 3U < sourcePositionData.size() && normalOffset0 + 2U < sourceNormals.size()) {
                const double ax = static_cast<double>(sourcePositionData[positionOffset0 + 0U]);
                const double ay = static_cast<double>(sourcePositionData[positionOffset0 + 1U]);
                const double az = static_cast<double>(sourcePositionData[positionOffset0 + 2U]);
                const double bx = static_cast<double>(sourcePositionData[positionOffset1 + 0U]);
                const double by = static_cast<double>(sourcePositionData[positionOffset1 + 1U]);
                const double bz = static_cast<double>(sourcePositionData[positionOffset1 + 2U]);
                const double cx = static_cast<double>(sourcePositionData[positionOffset2 + 0U]);
                const double cy = static_cast<double>(sourcePositionData[positionOffset2 + 1U]);
                const double cz = static_cast<double>(sourcePositionData[positionOffset2 + 2U]);
                const double edgeAx = bx - ax;
                const double edgeAy = by - ay;
                const double edgeAz = bz - az;
                const double edgeBx = cx - ax;
                const double edgeBy = cy - ay;
                const double edgeBz = cz - az;
                const double faceNormalX = (edgeAy * edgeBz) - (edgeAz * edgeBy);
                const double faceNormalY = (edgeAz * edgeBx) - (edgeAx * edgeBz);
                const double faceNormalZ = (edgeAx * edgeBy) - (edgeAy * edgeBx);
                const double sourceNormalX = static_cast<double>(sourceNormals[normalOffset0 + 0U]);
                const double sourceNormalY = static_cast<double>(sourceNormals[normalOffset0 + 1U]);
                const double sourceNormalZ = static_cast<double>(sourceNormals[normalOffset0 + 2U]);
                const double alignmentDot =
                    (faceNormalX * sourceNormalX) +
                    (faceNormalY * sourceNormalY) +
                    (faceNormalZ * sourceNormalZ);
                AppendInitializationTrace(
                    "[WiiUFile] Opaque firstTriangle indices=(%u,%u,%u) faceNormal=(%f,%f,%f) sourceNormal=(%f,%f,%f) alignmentDot=%f\n",
                    static_cast<unsigned>(sourceIndices[0]),
                    static_cast<unsigned>(sourceIndices[1]),
                    static_cast<unsigned>(sourceIndices[2]),
                    faceNormalX,
                    faceNormalY,
                    faceNormalZ,
                    sourceNormalX,
                    sourceNormalY,
                    sourceNormalZ,
                    alignmentDot);
            }
        }

        const WiiURuntimeMaterial& runtimeMaterial = *drawCommand.RuntimeMaterial;

        if (SceneOpaqueMaterialBuffer.buffer != nullptr || SceneOpaqueLightBuffer.buffer != nullptr) {
            GX2DrawDone();
        }

        float materialData[] = {
            runtimeMaterial.GetBaseColor().X, runtimeMaterial.GetBaseColor().Y, runtimeMaterial.GetBaseColor().Z, runtimeMaterial.GetBaseColor().W,
            runtimeMaterial.GetEmissiveColor().X, runtimeMaterial.GetEmissiveColor().Y, runtimeMaterial.GetEmissiveColor().Z, runtimeMaterial.GetEmissiveColor().W
        };
        void* materialUploadBuffer = GX2RLockBufferEx(&SceneOpaqueMaterialBuffer, NoGx2rResourceFlags);
        if (materialUploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock the opaque-scene material buffer.");
        }

        StoreFloatArrayAsLittleEndian(materialUploadBuffer, materialData, sizeof(materialData) / sizeof(materialData[0]));
        GX2RUnlockBufferEx(&SceneOpaqueMaterialBuffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(&SceneOpaqueMaterialBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(
            static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK),
            SceneOpaqueMaterialBuffer.buffer,
            static_cast<std::size_t>(SceneOpaqueMaterialBuffer.elemSize) * static_cast<std::size_t>(SceneOpaqueMaterialBuffer.elemCount));

        const float4 ambientLightColor = frame.GetAmbientLightColor();
        const float4 directionalLightColor = frame.GetHasDirectionalLight() ? frame.GetDirectionalLight().Color : float4(0.0f, 0.0f, 0.0f, 0.0f);
        const float4 directionalLightDirection = frame.GetHasDirectionalLight() ? frame.GetDirectionalLight().Direction : float4(0.0f, 0.0f, -1.0f, 0.0f);
        float lightData[] = {
            ambientLightColor.X, ambientLightColor.Y, ambientLightColor.Z, ambientLightColor.W,
            directionalLightColor.X, directionalLightColor.Y, directionalLightColor.Z, directionalLightColor.W,
            directionalLightDirection.X, directionalLightDirection.Y, directionalLightDirection.Z, directionalLightDirection.W
        };
        void* lightUploadBuffer = GX2RLockBufferEx(&SceneOpaqueLightBuffer, NoGx2rResourceFlags);
        if (lightUploadBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock the opaque-scene light buffer.");
        }

        StoreFloatArrayAsLittleEndian(lightUploadBuffer, lightData, sizeof(lightData) / sizeof(lightData[0]));
        GX2RUnlockBufferEx(&SceneOpaqueLightBuffer, NoGx2rResourceFlags);
        GX2RInvalidateBuffer(&SceneOpaqueLightBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(
            static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK),
            SceneOpaqueLightBuffer.buffer,
            static_cast<std::size_t>(SceneOpaqueLightBuffer.elemSize) * static_cast<std::size_t>(SceneOpaqueLightBuffer.elemCount));

        UploadSceneOpaqueMeshClipSpace(*drawCommand.RuntimeModel, worldMatrix, worldViewProjectionMatrix);
        GX2SetFetchShader(&SceneOpaqueShaderGroup.fetchShader);
        GX2SetVertexShader(SceneOpaqueShaderGroup.vertexShader);
        GX2SetPixelShader(SceneOpaqueShaderGroup.pixelShader);
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
        GX2UniformBlock* materialUniformBlock = GX2GetPixelUniformBlock(SceneOpaqueShaderGroup.pixelShader, "MaterialBlock");
        if (materialUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene MaterialBlock uniform block before drawing.");
        }

        GX2UniformBlock* lightUniformBlock = GX2GetPixelUniformBlock(SceneOpaqueShaderGroup.pixelShader, "LightBlock");
        if (lightUniformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires the opaque-scene LightBlock uniform block before drawing.");
        }

        if (shouldLogOpaqueDraw) {
            AppendInitializationTrace(
                "[WiiUFile] Pixel LightBlock metadata uniformBlockCount=%u offset=%u size=%u bufferElemSize=%u bufferElemCount=%u\n",
                static_cast<unsigned>(SceneOpaqueShaderGroup.pixelShader->uniformBlockCount),
                static_cast<unsigned>(lightUniformBlock->offset),
                static_cast<unsigned>(lightUniformBlock->size),
                static_cast<unsigned>(SceneOpaqueLightBuffer.elemSize),
                static_cast<unsigned>(SceneOpaqueLightBuffer.elemCount));
        }

        GX2SetPixelUniformBlock(
            materialUniformBlock->offset,
            materialUniformBlock->size,
            SceneOpaqueMaterialBuffer.buffer);
        GX2SetPixelUniformBlock(
            lightUniformBlock->offset,
            lightUniformBlock->size,
            SceneOpaqueLightBuffer.buffer);
        GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x1, FALSE, TRUE);
        GX2SetBlendControl(
            GX2_RENDER_TARGET_0,
            GX2_BLEND_MODE_ONE,
            GX2_BLEND_MODE_ZERO,
            GX2_BLEND_COMBINE_MODE_ADD,
            FALSE,
            GX2_BLEND_MODE_ONE,
            GX2_BLEND_MODE_ZERO,
            GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetTargetChannelMasks(
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA,
            GX2_CHANNEL_MASK_RGBA);
        GX2RSetAttributeBuffer(&SceneOpaquePositionBuffer, 0, SceneOpaquePositionBuffer.elemSize, 0);
        GX2RSetAttributeBuffer(&SceneOpaqueNormalBuffer, 1, SceneOpaqueNormalBuffer.elemSize, 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneOpaqueVertexCount, 0, 1);
        if (shouldLogOpaqueDraw) {
            AppendInitializationTrace(
                "[WiiUFile] SceneOpaque light-block draw submitted target=%ux%u expandedVertices=%u ambient=(%f,%f,%f) directional=(%f,%f,%f) direction=(%f,%f,%f)\n",
                targetWidth,
                targetHeight,
                SceneOpaqueVertexCount,
                ambientLightColor.X,
                ambientLightColor.Y,
                ambientLightColor.Z,
                directionalLightColor.X,
                directionalLightColor.Y,
                directionalLightColor.Z,
                directionalLightDirection.X,
                directionalLightDirection.Y,
                directionalLightDirection.Z);
            Scene3DDebugLogCount++;
        }
    }

    /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using model-space positions and normals.
    void WiiUGx2Presenter::UploadSceneOpaqueMesh(const WiiURuntimeModel& runtimeModel) {
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<float>& sourceNormalData = runtimeModel.GetNormalData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneOpaqueResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize opaque scene resources before uploading geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene position data.");
        } else if (sourceNormalData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene normal data.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene index data.");
        }

        std::vector<float> expandedPositionData;
        std::vector<float> expandedNormalData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        expandedNormalData.reserve(static_cast<std::size_t>(indexData.size()) * 3U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t positionOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            const std::size_t normalOffset = static_cast<std::size_t>(sourceIndex) * 3U;
            if (positionOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene index outside the uploaded position range.");
            } else if (normalOffset + 2U >= sourceNormalData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene index outside the uploaded normal range.");
            }

            expandedPositionData.push_back(sourcePositionData[positionOffset + 0U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 1U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 2U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 3U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 0U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 1U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 2U]);
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaquePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaquePositionBuffer, 0, sizeof(SceneOpaquePositionBuffer));
        }

        if (SceneOpaqueNormalBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueNormalBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueNormalBuffer, 0, sizeof(SceneOpaqueNormalBuffer));
        }

        if (SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        }

        InitializeSceneOpaqueVertexBuffer(&SceneOpaquePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()), SceneOpaquePositionElementSize, 4U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueNormalBuffer, expandedNormalData.data(), static_cast<std::uint32_t>(expandedNormalData.size()), SceneOpaqueNormalElementSize, 3U);
        SceneOpaqueVertexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
    }

    /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using CPU-expanded clip-space positions plus CPU-transformed world-space normals.
    void WiiUGx2Presenter::UploadSceneOpaqueMeshClipSpace(const WiiURuntimeModel& runtimeModel, const float4x4& worldMatrix, const float4x4& worldViewProjectionMatrix) {
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<float>& sourceNormalData = runtimeModel.GetNormalData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneOpaqueResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize opaque scene resources before uploading clip-space geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene position data for clip-space upload.");
        } else if (sourceNormalData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene normal data for clip-space upload.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene index data for clip-space upload.");
        }

        std::vector<float> expandedPositionData;
        std::vector<float> expandedNormalData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        expandedNormalData.reserve(static_cast<std::size_t>(indexData.size()) * 3U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t positionOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            const std::size_t normalOffset = static_cast<std::size_t>(sourceIndex) * 3U;
            if (positionOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene clip-space index outside the uploaded position range.");
            } else if (normalOffset + 2U >= sourceNormalData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene clip-space index outside the uploaded normal range.");
            }

            const float sourceX = sourcePositionData[positionOffset + 0U];
            const float sourceY = sourcePositionData[positionOffset + 1U];
            const float sourceZ = sourcePositionData[positionOffset + 2U];
            const float sourceW = sourcePositionData[positionOffset + 3U];
            const float clipX =
                (sourceX * worldViewProjectionMatrix.M11) +
                (sourceY * worldViewProjectionMatrix.M21) +
                (sourceZ * worldViewProjectionMatrix.M31) +
                (sourceW * worldViewProjectionMatrix.M41);
            const float clipY =
                (sourceX * worldViewProjectionMatrix.M12) +
                (sourceY * worldViewProjectionMatrix.M22) +
                (sourceZ * worldViewProjectionMatrix.M32) +
                (sourceW * worldViewProjectionMatrix.M42);
            const float clipZ =
                (sourceX * worldViewProjectionMatrix.M13) +
                (sourceY * worldViewProjectionMatrix.M23) +
                (sourceZ * worldViewProjectionMatrix.M33) +
                (sourceW * worldViewProjectionMatrix.M43);
            const float clipW =
                (sourceX * worldViewProjectionMatrix.M14) +
                (sourceY * worldViewProjectionMatrix.M24) +
                (sourceZ * worldViewProjectionMatrix.M34) +
                (sourceW * worldViewProjectionMatrix.M44);

            expandedPositionData.push_back(clipX);
            expandedPositionData.push_back(clipY);
            expandedPositionData.push_back(clipZ);
            expandedPositionData.push_back(clipW);
            const double sourceNormalX = static_cast<double>(sourceNormalData[normalOffset + 0U]);
            const double sourceNormalY = static_cast<double>(sourceNormalData[normalOffset + 1U]);
            const double sourceNormalZ = static_cast<double>(sourceNormalData[normalOffset + 2U]);
            const double worldNormalX =
                (sourceNormalX * static_cast<double>(worldMatrix.M11)) +
                (sourceNormalY * static_cast<double>(worldMatrix.M21)) +
                (sourceNormalZ * static_cast<double>(worldMatrix.M31));
            const double worldNormalY =
                (sourceNormalX * static_cast<double>(worldMatrix.M12)) +
                (sourceNormalY * static_cast<double>(worldMatrix.M22)) +
                (sourceNormalZ * static_cast<double>(worldMatrix.M32));
            const double worldNormalZ =
                (sourceNormalX * static_cast<double>(worldMatrix.M13)) +
                (sourceNormalY * static_cast<double>(worldMatrix.M23)) +
                (sourceNormalZ * static_cast<double>(worldMatrix.M33));
            const double worldNormalLengthSquared =
                (worldNormalX * worldNormalX) +
                (worldNormalY * worldNormalY) +
                (worldNormalZ * worldNormalZ);
            if (worldNormalLengthSquared <= 0.0) {
                throw std::runtime_error("Wii U GX2 presenter requires non-zero opaque-scene world normals after CPU transform.");
            }

            const double worldNormalInverseLength = 1.0 / std::sqrt(worldNormalLengthSquared);
            expandedNormalData.push_back(static_cast<float>(worldNormalX * worldNormalInverseLength));
            expandedNormalData.push_back(static_cast<float>(worldNormalY * worldNormalInverseLength));
            expandedNormalData.push_back(static_cast<float>(worldNormalZ * worldNormalInverseLength));
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr || SceneOpaqueNormalBuffer.buffer != nullptr || SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2DrawDone();
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaquePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaquePositionBuffer, 0, sizeof(SceneOpaquePositionBuffer));
        }

        if (SceneOpaqueNormalBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueNormalBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueNormalBuffer, 0, sizeof(SceneOpaqueNormalBuffer));
        }

        if (SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        }

        InitializeSceneOpaqueVertexBuffer(&SceneOpaquePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()), SceneOpaquePositionElementSize, 4U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueNormalBuffer, expandedNormalData.data(), static_cast<std::uint32_t>(expandedNormalData.size()), SceneOpaqueNormalElementSize, 3U);
        SceneOpaqueVertexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
    }

    /// Uploads one runtime model into the presenter-owned flat-color mesh path using one CPU-expanded clip-space transform.
    void WiiUGx2Presenter::UploadSceneCubeMesh(const WiiURuntimeModel& runtimeModel, const float4x4& worldViewProjectionMatrix) {
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneCubeResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize scene cube resources before uploading geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube position data.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube index data.");
        }

        std::vector<float> expandedPositionData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t sourceOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            if (sourceOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one scene cube index outside the uploaded position range.");
            }

            const float sourceX = sourcePositionData[sourceOffset + 0U];
            const float sourceY = sourcePositionData[sourceOffset + 1U];
            const float sourceZ = sourcePositionData[sourceOffset + 2U];
            const float sourceW = sourcePositionData[sourceOffset + 3U];
            const float sceneCubeClipX =
                (sourceX * worldViewProjectionMatrix.M11) +
                (sourceY * worldViewProjectionMatrix.M21) +
                (sourceZ * worldViewProjectionMatrix.M31) +
                (sourceW * worldViewProjectionMatrix.M41);
            const float sceneCubeClipY =
                (sourceX * worldViewProjectionMatrix.M12) +
                (sourceY * worldViewProjectionMatrix.M22) +
                (sourceZ * worldViewProjectionMatrix.M32) +
                (sourceW * worldViewProjectionMatrix.M42);
            const float sceneCubeClipZ =
                (sourceX * worldViewProjectionMatrix.M13) +
                (sourceY * worldViewProjectionMatrix.M23) +
                (sourceZ * worldViewProjectionMatrix.M33) +
                (sourceW * worldViewProjectionMatrix.M43);
            const float sceneCubeClipW =
                (sourceX * worldViewProjectionMatrix.M14) +
                (sourceY * worldViewProjectionMatrix.M24) +
                (sourceZ * worldViewProjectionMatrix.M34) +
                (sourceW * worldViewProjectionMatrix.M44);

            expandedPositionData.push_back(sceneCubeClipX);
            expandedPositionData.push_back(sceneCubeClipY);
            expandedPositionData.push_back(sceneCubeClipZ);
            expandedPositionData.push_back(sceneCubeClipW);
        }

        if (SceneCubePositionBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubePositionBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubePositionBuffer, 0, sizeof(SceneCubePositionBuffer));
        }

        if (SceneCubeIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneCubeIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneCubeIndexBuffer, 0, sizeof(SceneCubeIndexBuffer));
        }

        InitializeSceneCubeVertexBuffer(&SceneCubePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()));
        InitializeSceneCubeIndexBuffer(&SceneCubeIndexBuffer, indexData.data(), static_cast<std::uint32_t>(indexData.size()));
        SceneCubeIndexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
        IsSceneCubeMeshConfigured = true;
    }

    /// Renders one captured quad command into one target color buffer using the already-uploaded vertex data for the supplied quad index.
    void WiiUGx2Presenter::RenderQuadCommandToColorBuffer(const WiiUGx2QuadCommand& command, std::uint32_t quadIndex, std::uint32_t logicalWidth, std::uint32_t logicalHeight, std::uint32_t targetWidth, std::uint32_t targetHeight) {
        if (logicalWidth == 0U || logicalHeight == 0U || targetWidth == 0U || targetHeight == 0U) {
            return;
        }

        const WiiUGx2TextureHandle* textureHandle = command.TextureHandle != nullptr ? command.TextureHandle : &UiSolidWhiteTextureHandle;
        const double scaleX = static_cast<double>(targetWidth) / static_cast<double>(logicalWidth);
        const double scaleY = static_cast<double>(targetHeight) / static_cast<double>(logicalHeight);
        const double clippedLeft = std::max(0.0, static_cast<double>(command.ClipRect.X));
        const double clippedTop = std::max(0.0, static_cast<double>(command.ClipRect.Y));
        const double clippedRight = std::min(static_cast<double>(logicalWidth), static_cast<double>(command.ClipRect.X + command.ClipRect.Width));
        const double clippedBottom = std::min(static_cast<double>(logicalHeight), static_cast<double>(command.ClipRect.Y + command.ClipRect.Height));
        if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) {
            return;
        }

        const std::uint32_t scissorX = static_cast<std::uint32_t>(std::max(0.0, std::floor(clippedLeft * scaleX)));
        const std::uint32_t scissorY = static_cast<std::uint32_t>(std::max(0.0, std::floor(clippedTop * scaleY)));
        const std::uint32_t scissorWidth = static_cast<std::uint32_t>(std::max(0.0, std::ceil(clippedRight * scaleX) - std::floor(clippedLeft * scaleX)));
        const std::uint32_t scissorHeight = static_cast<std::uint32_t>(std::max(0.0, std::ceil(clippedBottom * scaleY) - std::floor(clippedTop * scaleY)));
        if (scissorWidth == 0U || scissorHeight == 0U) {
            return;
        }
        static_cast<void>(command.RotationRadians);

        GX2SetScissor(scissorX, scissorY, scissorWidth, scissorHeight);
        const std::uint32_t vertexStartIndex = quadIndex * UiQuadVertexCount;
        GX2RSetAttributeBuffer(&UiQuadPositionBuffer, 0, UiQuadPositionBuffer.elemSize, vertexStartIndex * UiQuadPositionElementSize);
        GX2RSetAttributeBuffer(&UiQuadTexCoordBuffer, 1, UiQuadTexCoordBuffer.elemSize, vertexStartIndex * UiQuadTexCoordElementSize);
        GX2RSetAttributeBuffer(&UiQuadColorBuffer, 2, UiQuadColorBuffer.elemSize, vertexStartIndex * UiQuadColorElementSize);
        GX2SetPixelTexture(&textureHandle->Texture, UiQuadShaderGroup.pixelShader->samplerVars[0].location);
        GX2SetPixelSampler(&textureHandle->Sampler, UiQuadShaderGroup.pixelShader->samplerVars[0].location);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, UiQuadVertexCount, 0, 1);
    }

    /// Renders the presenter-owned diagnostic square into one target color buffer with the supplied GX2 context state.
    void WiiUGx2Presenter::RenderDiagnosticSquareToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic context state.");
        } else if (colorBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic color buffer.");
        } else if (!AreDiagnosticSquareResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize diagnostic square resources before drawing.");
        }

        GX2SetContextState(contextState);
        GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
        GX2ClearColor(colorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
        GX2SetFetchShader(&DiagnosticSquareShaderGroup.fetchShader);
        GX2SetVertexShader(DiagnosticSquareShaderGroup.vertexShader);
        GX2SetPixelShader(DiagnosticSquareShaderGroup.pixelShader);
        GX2RSetAttributeBuffer(&DiagnosticSquarePositionBuffer, 0, DiagnosticSquarePositionBuffer.elemSize, 0);
        GX2RSetAttributeBuffer(&DiagnosticSquareColorBuffer, 1, DiagnosticSquareColorBuffer.elemSize, 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticSquareVertexCount, 0, 1);
    }

    /// Renders the presenter-owned diagnostic triangle into one target color buffer with the supplied GX2 context state.
    void WiiUGx2Presenter::RenderDiagnosticTriangleToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic triangle context state.");
        } else if (colorBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid diagnostic triangle color buffer.");
        } else if (!AreDiagnosticTriangleResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize diagnostic triangle resources before drawing.");
        }

        GX2SetContextState(contextState);
        GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
        GX2ClearColor(colorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
        GX2SetFetchShader(&DiagnosticTriangleShaderGroup.fetchShader);
        GX2SetVertexShader(DiagnosticTriangleShaderGroup.vertexShader);
        GX2SetPixelShader(DiagnosticTriangleShaderGroup.pixelShader);
        if (DiagnosticTriangleShaderGroup.vertexShader->uniformBlockCount != 0U && DiagnosticTriangleTransformBuffer.buffer != nullptr) {
            GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
            GX2UniformBlock* transformUniformBlock = GX2GetVertexUniformBlock(DiagnosticTriangleShaderGroup.vertexShader, "TransformBlock");
            if (transformUniformBlock == nullptr) {
                throw std::runtime_error("Wii U GX2 presenter requires the diagnostic triangle TransformBlock uniform block before drawing.");
            }

            GX2RSetVertexUniformBlock(&DiagnosticTriangleTransformBuffer, transformUniformBlock->offset, 0);
        }
        GX2RSetAttributeBuffer(&DiagnosticTrianglePositionBuffer, 0, DiagnosticTrianglePositionBuffer.elemSize, 0);
        GX2RSetAttributeBuffer(&DiagnosticTriangleColorBuffer, 1, DiagnosticTriangleColorBuffer.elemSize, 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, DiagnosticTriangleVertexCount, 0, 1);
    }

    /// Renders the configured scene-cube mesh into one target color buffer with the supplied GX2 context state.
    void WiiUGx2Presenter::RenderSceneCubeToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid scene cube context state.");
        } else if (colorBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid scene cube color buffer.");
        } else if (!AreSceneCubeResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize scene cube resources before drawing.");
        } else if (!IsSceneCubeMeshConfigured) {
            throw std::runtime_error("Wii U GX2 presenter must upload scene cube geometry before drawing.");
        }

        GX2SetContextState(contextState);
        GX2SetColorBuffer(colorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(colorBuffer->surface.width), static_cast<float>(colorBuffer->surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, colorBuffer->surface.width, colorBuffer->surface.height);
        GX2ClearColor(colorBuffer, DiagnosticClearRed, DiagnosticClearGreen, DiagnosticClearBlue, DiagnosticClearAlpha);
        GX2SetFetchShader(&SceneCubeShaderGroup.fetchShader);
        GX2SetVertexShader(SceneCubeShaderGroup.vertexShader);
        GX2SetPixelShader(SceneCubeShaderGroup.pixelShader);
        if (SceneCubeShaderGroup.vertexShader->uniformBlockCount != 0U && SceneCubeTransformBuffer.buffer != nullptr) {
            GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
            GX2UniformBlock* transformUniformBlock = GX2GetVertexUniformBlock(SceneCubeShaderGroup.vertexShader, "TransformBlock");
            if (transformUniformBlock == nullptr) {
                throw std::runtime_error("Wii U GX2 presenter requires the scene cube TransformBlock uniform block before drawing.");
            }

            GX2RSetVertexUniformBlock(&SceneCubeTransformBuffer, transformUniformBlock->offset, 0);
        }
        GX2RSetAttributeBuffer(&SceneCubePositionBuffer, 0, SceneCubePositionBuffer.elemSize, 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneCubeIndexCount, 0, 1);
    }

    /// Presents the current TV and DRC color buffers to their scan buffers.
    void WiiUGx2Presenter::PresentScanBuffers() {
        GX2SetContextState(TvContextState);
        GX2CopyColorBufferToScanBuffer(&TvColorBuffer, GX2_SCAN_TARGET_TV);
        GX2SetContextState(DrcContextState);
        GX2CopyColorBufferToScanBuffer(&DrcColorBuffer, GX2_SCAN_TARGET_DRC);
        GX2SwapScanBuffers();
        GX2Flush();
        GX2DrawDone();
        GX2SetTVEnable(TRUE);
        GX2SetDRCEnable(TRUE);
    }
}
