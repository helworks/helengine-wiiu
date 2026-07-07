#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include "diagnostic_triangle_shader_bin.h"
#include "diagnostic_square_shader_bin.h"
#include "scene_cube_flat_color_shader_bin.h"
#include "ui_quad_shader_bin.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

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
        constexpr std::uint32_t SceneCubeVertexElementSize = 4U * sizeof(float);
        constexpr std::uint32_t SceneCubeIndexElementSize = sizeof(std::uint16_t);
        constexpr std::uint32_t SceneCubeTransformSizeInBytes = 16U * sizeof(float);
        constexpr std::uint32_t UiQuadVertexCount = 6U;
        constexpr std::uint32_t UiQuadPositionElementSize = 2U * sizeof(float);
        constexpr std::uint32_t UiQuadTexCoordElementSize = 2U * sizeof(float);
        constexpr std::uint32_t UiQuadColorElementSize = 4U * sizeof(float);
        constexpr std::uint32_t SolidWhitePixel = 0xFFFFFFFFU;
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
    }

    /// Creates one uninitialized GX2 presenter.
    WiiUGx2Presenter::WiiUGx2Presenter()
        : IsInitialized(false)
        , TvScanBuffer(nullptr)
        , DrcScanBuffer(nullptr)
        , CommandBufferPool(nullptr)
        , TvColorBuffer()
        , DrcColorBuffer()
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
        , DrcScanBufferSize(0U) {
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
        std::memset(&DiagnosticSquareShaderGroup, 0, sizeof(DiagnosticSquareShaderGroup));
        std::memset(&DiagnosticSquarePositionBuffer, 0, sizeof(DiagnosticSquarePositionBuffer));
        std::memset(&DiagnosticSquareColorBuffer, 0, sizeof(DiagnosticSquareColorBuffer));
        std::memset(&DiagnosticTriangleShaderGroup, 0, sizeof(DiagnosticTriangleShaderGroup));
        std::memset(&DiagnosticTrianglePositionBuffer, 0, sizeof(DiagnosticTrianglePositionBuffer));
        std::memset(&DiagnosticTriangleColorBuffer, 0, sizeof(DiagnosticTriangleColorBuffer));
        std::memset(&DiagnosticTriangleTransformBuffer, 0, sizeof(DiagnosticTriangleTransformBuffer));
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
        GX2Init(initAttributes);

        GX2DrcRenderMode drcRenderMode = GX2GetSystemDRCMode();
        std::uint32_t unusedSize = 0U;
        GX2CalcTVSize(PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &TvScanBufferSize, &unusedSize);
        GX2CalcDRCSize(drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &DrcScanBufferSize, &unusedSize);

        TvScanBuffer = MEMAllocFromDefaultHeapEx(TvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        DrcScanBuffer = MEMAllocFromDefaultHeapEx(DrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        if (TvScanBuffer == nullptr || DrcScanBuffer == nullptr) {
            Shutdown();
            return false;
        }

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvScanBuffer, TvScanBufferSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, DrcScanBuffer, DrcScanBufferSize);
        GX2SetTVBuffer(TvScanBuffer, TvScanBufferSize, PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
        GX2SetDRCBuffer(DrcScanBuffer, DrcScanBufferSize, drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

        InitializeTvColorBuffer();
        InitializeDrcColorBuffer();
        TvContextState = static_cast<GX2ContextState*>(MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
        DrcContextState = static_cast<GX2ContextState*>(MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT));
        if (TvContextState == nullptr || DrcContextState == nullptr) {
            Shutdown();
            return false;
        }

        GX2SetupContextStateEx(TvContextState, TRUE);
        GX2SetContextState(TvContextState);
        GX2SetColorBuffer(&TvColorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(TvColorBuffer.surface.width), static_cast<float>(TvColorBuffer.surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, TvColorBuffer.surface.width, TvColorBuffer.surface.height);

        GX2SetupContextStateEx(DrcContextState, TRUE);
        GX2SetContextState(DrcContextState);
        GX2SetColorBuffer(&DrcColorBuffer, GX2_RENDER_TARGET_0);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(DrcColorBuffer.surface.width), static_cast<float>(DrcColorBuffer.surface.height), 0.0f, 1.0f);
        GX2SetScissor(0, 0, DrcColorBuffer.surface.width, DrcColorBuffer.surface.height);
        InitializeDiagnosticSquareResources();
        InitializeDiagnosticTriangleResources();
        InitializeSceneCubeResources();
        InitializeUiQuadResources();
        GX2SetTVScale(TvSurfaceWidth, TvSurfaceHeight);
        GX2SetDRCScale(DrcSurfaceWidth, DrcSurfaceHeight);
        GX2SetSwapInterval(1);
        IsInitialized = true;
        return true;
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
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneCubeResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize scene cube resources before uploading geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube position data.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty scene cube index data.");
        }

        constexpr double SceneCubeYawRadians = 0.65;
        constexpr double SceneCubePitchRadians = -0.55;
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
                ((SceneCubeFarPlane + SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane)) * viewZ +
                ((2.0 * SceneCubeFarPlane * SceneCubeNearPlane) / (SceneCubeNearPlane - SceneCubeFarPlane));
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
        OSReport(
            "[WiiUGx2Presenter] scene cube configured vertices=%u expandedVertices=%u indices=%u firstPosition=(%.3f, %.3f, %.3f, %.3f) firstIndex=%u\n",
            static_cast<unsigned int>(sourcePositionData.size() / 4U),
            static_cast<unsigned int>(expandedPositionData.size() / 4U),
            static_cast<unsigned int>(SceneCubeIndexCount),
            expandedPositionData.size() >= 4U ? expandedPositionData[0] : 0.0f,
            expandedPositionData.size() >= 4U ? expandedPositionData[1] : 0.0f,
            expandedPositionData.size() >= 4U ? expandedPositionData[2] : 0.0f,
            expandedPositionData.size() >= 4U ? expandedPositionData[3] : 0.0f,
            indexData.empty() ? 0U : static_cast<unsigned int>(indexData[0]));
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
    }

    /// Initializes the presenter-owned shader and vertex buffers used by the diagnostic GX2 square path.
    void WiiUGx2Presenter::InitializeDiagnosticSquareResources() {
        if (AreDiagnosticSquareResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&DiagnosticSquareShaderGroup, 0, diagnostic_square_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded diagnostic shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic position shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aColour", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic color shader attribute.");
            }

            if (!WHBGfxInitFetchShader(&DiagnosticSquareShaderGroup)) {
                throw std::runtime_error("Wii U GX2 presenter could not initialize the diagnostic fetch shader.");
            }

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.vertexShader->program, DiagnosticSquareShaderGroup.vertexShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.pixelShader->program, DiagnosticSquareShaderGroup.pixelShader->size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, DiagnosticSquareShaderGroup.fetchShader.program, DiagnosticSquareShaderGroup.fetchShader.size);

            InitializeDiagnosticSquareBuffer(&DiagnosticSquarePositionBuffer, DiagnosticSquarePositionData, DiagnosticSquareVertexCount);
            InitializeDiagnosticSquareBuffer(&DiagnosticSquareColorBuffer, DiagnosticSquareColorData, DiagnosticSquareVertexCount);
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

        GX2SetFetchShader(&UiQuadShaderGroup.fetchShader);
        GX2SetVertexShader(UiQuadShaderGroup.vertexShader);
        GX2SetPixelShader(UiQuadShaderGroup.pixelShader);
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
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
