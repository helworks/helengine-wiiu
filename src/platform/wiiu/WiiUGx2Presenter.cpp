#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include "diagnostic_triangle_shader_bin.h"
#include "diagnostic_square_shader_bin.h"
#include "ForwardStandard_shader_bin.h"
#include "ForwardStandardShadowed_shader_bin.h"
#include "ShadowDepth_shader_bin.h"
#include "scene_opaque_lit_shader_bin.h"
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
        constexpr GX2SurfaceFormat DepthSurfaceFormat = GX2_SURFACE_FORMAT_FLOAT_R32;
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
        constexpr GX2RResourceFlags ColorBufferSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
        constexpr GX2RResourceFlags DepthBufferSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_DEPTH_BUFFER | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
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
        constexpr std::uint32_t SceneOpaqueTexCoordElementSize = 2U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueIndexElementSize = sizeof(std::uint16_t);
        constexpr std::uint32_t SceneOpaqueTransformSizeInBytes = 48U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueMaterialSizeInBytes = 8U * sizeof(float);
        constexpr std::uint32_t SceneOpaqueLightSizeInBytes = 12U * sizeof(float);
        constexpr double SceneDrivenFieldOfViewRadians = 1.0;
        constexpr std::uint32_t UiQuadVertexCount = 6U;
        constexpr std::uint32_t DirectionalShadowMapSize = 1024U;
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

        void StoreFloat32LittleEndian(void* destination, float value) {
            std::uint32_t bits = 0U;
            std::memcpy(&bits, &value, sizeof(bits));
            std::uint8_t* destinationBytes = static_cast<std::uint8_t*>(destination);
            destinationBytes[0] = static_cast<std::uint8_t>(bits & 0xFFU);
            destinationBytes[1] = static_cast<std::uint8_t>((bits >> 8U) & 0xFFU);
            destinationBytes[2] = static_cast<std::uint8_t>((bits >> 16U) & 0xFFU);
            destinationBytes[3] = static_cast<std::uint8_t>((bits >> 24U) & 0xFFU);
        }

        /// Stores one float payload in the byte order consumed by GX2 uniform blocks.
        void StoreFloatArrayAsLittleEndian(void* destination, const float* source, std::size_t valueCount) {
            std::uint8_t* destinationBytes = static_cast<std::uint8_t*>(destination);
            for (std::size_t valueIndex = 0; valueIndex < valueCount; valueIndex++) {
                StoreFloat32LittleEndian(destinationBytes + (valueIndex * sizeof(float)), source[valueIndex]);
            }
        }

        /// Resolves one generated pixel sampler by its stable semantic name without depending on reflection-array order.
        const GX2SamplerVar* ResolvePixelSamplerVar(const GX2PixelShader* pixelShader, const char* samplerName) {
            if (pixelShader == nullptr) {
                throw std::runtime_error("Wii U GX2 sampler resolution requires one pixel shader.");
            } else if (samplerName == nullptr || samplerName[0] == '\0') {
                throw std::runtime_error("Wii U GX2 sampler resolution requires one semantic sampler name.");
            }

            for (std::uint32_t samplerIndex = 0U; samplerIndex < pixelShader->samplerVarCount; samplerIndex++) {
                const GX2SamplerVar& samplerVar = pixelShader->samplerVars[samplerIndex];
                if (samplerVar.name != nullptr && std::strcmp(samplerVar.name, samplerName) == 0) {
                    return &samplerVar;
                }
            }

            return nullptr;
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
        , AreStandardShaderResourcesInitialized(false)
        , ForwardStandardShaderGroup()
        , ForwardStandardShadowedShaderGroup()
        , ShadowDepthShaderGroup()
        , StandardShaderTransformBuffer()
        , ShadowDepthTransformBuffer()
        , StandardShaderForwardLightBuffer()
        , StandardShaderShadowBuffer()
        , StandardShaderBaseColorBuffer()
        , StandardShaderRoughnessBuffer()
        , StandardShaderMetallicBuffer()
        , StandardShaderSpecularBuffer()
        , StandardShaderEmissiveBuffer()
        , AreDirectionalShadowResourcesInitialized(false)
        , DirectionalShadowDepthBuffer()
        , DirectionalShadowTexture()
        , DirectionalShadowSampler()
        , SceneOpaquePositionBuffer()
        , SceneOpaqueNormalBuffer()
        , SceneOpaqueTexCoordBuffer()
        , SceneOpaqueIndexBuffer()
        , SceneOpaqueTransformBuffer()
        , SceneOpaqueMaterialBuffer()
        , SceneOpaqueLightBuffer()
        , SceneOpaqueVertexCount(0U)
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
        std::memset(&ForwardStandardShaderGroup, 0, sizeof(ForwardStandardShaderGroup));
        std::memset(&ForwardStandardShadowedShaderGroup, 0, sizeof(ForwardStandardShadowedShaderGroup));
        std::memset(&ShadowDepthShaderGroup, 0, sizeof(ShadowDepthShaderGroup));
        std::memset(&StandardShaderTransformBuffer, 0, sizeof(StandardShaderTransformBuffer));
        std::memset(&ShadowDepthTransformBuffer, 0, sizeof(ShadowDepthTransformBuffer));
        std::memset(&StandardShaderForwardLightBuffer, 0, sizeof(StandardShaderForwardLightBuffer));
        std::memset(&StandardShaderShadowBuffer, 0, sizeof(StandardShaderShadowBuffer));
        std::memset(&StandardShaderBaseColorBuffer, 0, sizeof(StandardShaderBaseColorBuffer));
        std::memset(&StandardShaderRoughnessBuffer, 0, sizeof(StandardShaderRoughnessBuffer));
        std::memset(&StandardShaderMetallicBuffer, 0, sizeof(StandardShaderMetallicBuffer));
        std::memset(&StandardShaderSpecularBuffer, 0, sizeof(StandardShaderSpecularBuffer));
        std::memset(&StandardShaderEmissiveBuffer, 0, sizeof(StandardShaderEmissiveBuffer));
        std::memset(&DirectionalShadowDepthBuffer, 0, sizeof(DirectionalShadowDepthBuffer));
        std::memset(&DirectionalShadowTexture, 0, sizeof(DirectionalShadowTexture));
        std::memset(&DirectionalShadowSampler, 0, sizeof(DirectionalShadowSampler));
        std::memset(&SceneOpaquePositionBuffer, 0, sizeof(SceneOpaquePositionBuffer));
        std::memset(&SceneOpaqueNormalBuffer, 0, sizeof(SceneOpaqueNormalBuffer));
        std::memset(&SceneOpaqueTexCoordBuffer, 0, sizeof(SceneOpaqueTexCoordBuffer));
        std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        std::memset(&SceneOpaqueTransformBuffer, 0, sizeof(SceneOpaqueTransformBuffer));
        std::memset(&SceneOpaqueMaterialBuffer, 0, sizeof(SceneOpaqueMaterialBuffer));
        std::memset(&SceneOpaqueLightBuffer, 0, sizeof(SceneOpaqueLightBuffer));
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
        AppendInitializationTrace("[WiiUFile] GX2 initialize: shared StandardShader resources begin.\n");
        InitializeStandardShaderResources();
        AppendInitializationTrace("[WiiUFile] GX2 initialize: shared StandardShader resources completed.\n");
        InitializeDirectionalShadowResources();
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
        } else if (!AreStandardShaderResourcesInitialized || !AreDirectionalShadowResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize StandardShader directional-shadow resources before 3D frame rendering.");
        } else if (!AreUiQuadResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize UI quad resources before 2D frame rendering.");
        }

        if (frame3D.GetHasDirectionalShadow()) {
            RenderDirectionalShadowDepthPass(TvContextState, frame3D);
        }

        Render3DFrameToColorBuffer(TvContextState, &TvColorBuffer, &TvDepthBuffer, frame3D, TvSurfaceWidth, TvSurfaceHeight);
        RenderQuadCommandsToColorBuffer(frame2D, TvSurfaceWidth, TvSurfaceHeight);
        GX2DrawDone();
        Render3DFrameToColorBuffer(DrcContextState, &DrcColorBuffer, &DrcDepthBuffer, frame3D, DrcSurfaceWidth, DrcSurfaceHeight);
        RenderQuadCommandsToColorBuffer(frame2D, DrcSurfaceWidth, DrcSurfaceHeight);
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

    /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
    void WiiUGx2Presenter::Shutdown() {
        DestroyUiQuadResources();
        DestroyDirectionalShadowResources();
        DestroyStandardShaderResources();
        DestroySceneOpaqueResources();
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
            if (!WHBGfxLoadGFDShaderGroup(&DiagnosticSquareShaderGroup, 0, diagnostic_square_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the embedded diagnostic shader group.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aPosition", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the diagnostic position shader attribute.");
            }

            if (!WHBGfxInitShaderAttribute(&DiagnosticSquareShaderGroup, "aColor", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32)) {
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

            if (!WHBGfxInitShaderAttribute(&SceneOpaqueShaderGroup, "aTexCoord", 2, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)) {
                throw std::runtime_error("Wii U GX2 presenter could not bind the opaque scene texcoord shader attribute.");
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

        if (SceneOpaquePositionBuffer.buffer != nullptr || SceneOpaqueNormalBuffer.buffer != nullptr || SceneOpaqueTexCoordBuffer.buffer != nullptr || SceneOpaqueIndexBuffer.buffer != nullptr) {
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

        if (SceneOpaqueTexCoordBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueTexCoordBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueTexCoordBuffer, 0, sizeof(SceneOpaqueTexCoordBuffer));
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

    /// Initializes the generated StandardShader variants and validates their stable vertex-input contract.
    void WiiUGx2Presenter::InitializeStandardShaderResources() {
        if (AreStandardShaderResourcesInitialized) {
            return;
        }

        try {
            if (!WHBGfxLoadGFDShaderGroup(&ForwardStandardShaderGroup, 0, ForwardStandard_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the generated ForwardStandard shader group.");
            }

            if (!WHBGfxLoadGFDShaderGroup(&ForwardStandardShadowedShaderGroup, 0, ForwardStandardShadowed_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the generated ForwardStandardShadowed shader group.");
            }

            if (!WHBGfxLoadGFDShaderGroup(&ShadowDepthShaderGroup, 0, ShadowDepth_shader_bin)) {
                throw std::runtime_error("Wii U GX2 presenter could not load the generated ShadowDepth shader group.");
            }

            WHBGfxShaderGroup* shaderGroups[] = {
                &ForwardStandardShaderGroup,
                &ForwardStandardShadowedShaderGroup,
                &ShadowDepthShaderGroup
            };
            for (WHBGfxShaderGroup* shaderGroup : shaderGroups) {
                if (!WHBGfxInitShaderAttribute(shaderGroup, "Position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32)) {
                    throw std::runtime_error("Wii U GX2 presenter could not bind the generated StandardShader Position attribute.");
                }

                if (!WHBGfxInitShaderAttribute(shaderGroup, "Normal", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32)) {
                    throw std::runtime_error("Wii U GX2 presenter could not bind the generated StandardShader Normal attribute.");
                }

                if (!WHBGfxInitShaderAttribute(shaderGroup, "TexCoord", 2, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32)) {
                    throw std::runtime_error("Wii U GX2 presenter could not bind the generated StandardShader TexCoord attribute.");
                }

                if (!WHBGfxInitFetchShader(shaderGroup)) {
                    throw std::runtime_error("Wii U GX2 presenter could not initialize one generated StandardShader fetch shader.");
                }

                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shaderGroup->vertexShader->program, shaderGroup->vertexShader->size);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shaderGroup->pixelShader->program, shaderGroup->pixelShader->size);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, shaderGroup->fetchShader.program, shaderGroup->fetchShader.size);
            }

            GX2UniformBlock* transformBlock = GX2GetVertexUniformBlock(ForwardStandardShaderGroup.vertexShader, "TransformBuffer");
            GX2UniformBlock* shadowDepthTransformBlock = GX2GetVertexUniformBlock(ShadowDepthShaderGroup.vertexShader, "TransformBuffer");
            GX2UniformBlock* forwardLightBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "ForwardLightBuffer");
            GX2UniformBlock* shadowBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "ShadowBuffer");
            GX2UniformBlock* baseColorBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "BaseColorBuffer");
            GX2UniformBlock* roughnessBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "RoughnessBuffer");
            GX2UniformBlock* metallicBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "MetallicBuffer");
            GX2UniformBlock* specularBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "SpecularBuffer");
            GX2UniformBlock* emissiveBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, "EmissiveBuffer");
            AppendInitializationTrace(
                "[WiiU][StandardShader] unshadowed counts blocks=%u samplers=%u; VS Transform offset=%u size=%u.\n",
                ForwardStandardShaderGroup.pixelShader->uniformBlockCount,
                ForwardStandardShaderGroup.pixelShader->samplerVarCount,
                transformBlock != nullptr ? transformBlock->offset : 0xFFFFFFFFU,
                transformBlock != nullptr ? transformBlock->size : 0U);
            const char* reflectedBlockNames[] = {
                "TransformBuffer",
                "ForwardLightBuffer",
                "ShadowBuffer",
                "BaseColorBuffer",
                "RoughnessBuffer",
                "MetallicBuffer",
                "SpecularBuffer",
                "EmissiveBuffer"
            };
            for (const char* reflectedBlockName : reflectedBlockNames) {
                GX2UniformBlock* unshadowedBlock = GX2GetPixelUniformBlock(ForwardStandardShaderGroup.pixelShader, reflectedBlockName);
                GX2UniformBlock* shadowedBlock = GX2GetPixelUniformBlock(ForwardStandardShadowedShaderGroup.pixelShader, reflectedBlockName);
                AppendInitializationTrace(
                    "[WiiU][StandardShader] block=%s unshadowed(offset=%u,size=%u) shadowed(offset=%u,size=%u).\n",
                    reflectedBlockName,
                    unshadowedBlock != nullptr ? unshadowedBlock->offset : 0xFFFFFFFFU,
                    unshadowedBlock != nullptr ? unshadowedBlock->size : 0U,
                    shadowedBlock != nullptr ? shadowedBlock->offset : 0xFFFFFFFFU,
                    shadowedBlock != nullptr ? shadowedBlock->size : 0U);
            }
            AppendInitializationTrace(
                "[WiiU][StandardShader] shadowed counts blocks=%u samplers=%u.\n",
                ForwardStandardShadowedShaderGroup.pixelShader->uniformBlockCount,
                ForwardStandardShadowedShaderGroup.pixelShader->samplerVarCount);
            for (std::uint32_t samplerIndex = 0U; samplerIndex < ForwardStandardShadowedShaderGroup.pixelShader->samplerVarCount; samplerIndex++) {
                const GX2SamplerVar& samplerVar = ForwardStandardShadowedShaderGroup.pixelShader->samplerVars[samplerIndex];
                AppendInitializationTrace(
                    "[WiiU][StandardShader] sampler[%u] name=%s location=%u type=%u.\n",
                    samplerIndex,
                    samplerVar.name != nullptr ? samplerVar.name : "<null>",
                    samplerVar.location,
                    static_cast<std::uint32_t>(samplerVar.type));
            }
            InitializeStandardShaderUniformBuffer(&StandardShaderTransformBuffer, transformBlock, "TransformBuffer");
            InitializeStandardShaderUniformBuffer(&ShadowDepthTransformBuffer, shadowDepthTransformBlock, "ShadowDepthTransformBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderForwardLightBuffer, forwardLightBlock, "ForwardLightBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderShadowBuffer, shadowBlock, "ShadowBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderBaseColorBuffer, baseColorBlock, "BaseColorBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderRoughnessBuffer, roughnessBlock, "RoughnessBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderMetallicBuffer, metallicBlock, "MetallicBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderSpecularBuffer, specularBlock, "SpecularBuffer");
            InitializeStandardShaderUniformBuffer(&StandardShaderEmissiveBuffer, emissiveBlock, "EmissiveBuffer");

            AreStandardShaderResourcesInitialized = true;
        } catch (...) {
            DestroyStandardShaderResources();
            throw;
        }
    }

    /// Releases all generated StandardShader variants after the GPU has finished using them.
    void WiiUGx2Presenter::DestroyStandardShaderResources() {
        DestroyStandardShaderUniformBuffer(&StandardShaderEmissiveBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderSpecularBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderMetallicBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderRoughnessBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderBaseColorBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderShadowBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderForwardLightBuffer);
        DestroyStandardShaderUniformBuffer(&StandardShaderTransformBuffer);
        DestroyStandardShaderUniformBuffer(&ShadowDepthTransformBuffer);
        WHBGfxShaderGroup* shaderGroups[] = {
            &ShadowDepthShaderGroup,
            &ForwardStandardShadowedShaderGroup,
            &ForwardStandardShaderGroup
        };
        for (WHBGfxShaderGroup* shaderGroup : shaderGroups) {
            if (shaderGroup->vertexShader != nullptr || shaderGroup->pixelShader != nullptr || shaderGroup->fetchShaderProgram != nullptr) {
                WHBGfxFreeShaderGroup(shaderGroup);
                std::memset(shaderGroup, 0, sizeof(WHBGfxShaderGroup));
            }
        }

        AreStandardShaderResourcesInitialized = false;
    }

    /// Initializes one uniform buffer with the exact size required by a reflected StandardShader block.
    void WiiUGx2Presenter::InitializeStandardShaderUniformBuffer(GX2RBuffer* buffer, GX2UniformBlock* uniformBlock, const char* blockName) {
        if (buffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a StandardShader uniform-buffer destination.");
        } else if (uniformBlock == nullptr || uniformBlock->size == 0U) {
            throw std::runtime_error(std::string("Wii U GX2 presenter requires the generated StandardShader ") + blockName + " uniform block.");
        }

        buffer->flags = DiagnosticUniformBufferFlags;
        buffer->elemSize = uniformBlock->size;
        buffer->elemCount = 1U;
        if (!GX2RCreateBuffer(buffer)) {
            throw std::runtime_error(std::string("Wii U GX2 presenter could not allocate the generated StandardShader ") + blockName + " uniform buffer.");
        }
    }

    /// Releases one allocated StandardShader uniform buffer.
    void WiiUGx2Presenter::DestroyStandardShaderUniformBuffer(GX2RBuffer* buffer) {
        if (buffer != nullptr && buffer->buffer != nullptr) {
            GX2RDestroyBufferEx(buffer, NoGx2rResourceFlags);
            std::memset(buffer, 0, sizeof(GX2RBuffer));
        }
    }

    /// Initializes the texture-backed depth surface used by the directional shadow pass.
    void WiiUGx2Presenter::InitializeDirectionalShadowResources() {
        if (AreDirectionalShadowResourcesInitialized) {
            return;
        }

        std::memset(&DirectionalShadowDepthBuffer, 0, sizeof(DirectionalShadowDepthBuffer));
        DirectionalShadowDepthBuffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
        DirectionalShadowDepthBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        DirectionalShadowDepthBuffer.surface.width = DirectionalShadowMapSize;
        DirectionalShadowDepthBuffer.surface.height = DirectionalShadowMapSize;
        DirectionalShadowDepthBuffer.surface.depth = 1U;
        DirectionalShadowDepthBuffer.surface.mipLevels = 1U;
        DirectionalShadowDepthBuffer.surface.format = DepthSurfaceFormat;
        DirectionalShadowDepthBuffer.surface.aa = GX2_AA_MODE1X;
        DirectionalShadowDepthBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        DirectionalShadowDepthBuffer.viewNumSlices = 1U;
        DirectionalShadowDepthBuffer.depthClear = 1.0f;
        DirectionalShadowDepthBuffer.stencilClear = 0U;
        GX2CalcSurfaceSizeAndAlignment(&DirectionalShadowDepthBuffer.surface);
        const GX2RResourceFlags shadowSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_DEPTH_BUFFER | GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
        if (!GX2RCreateSurface(&DirectionalShadowDepthBuffer.surface, shadowSurfaceFlags)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the directional shadow depth surface.");
        }

        GX2InitDepthBufferRegs(&DirectionalShadowDepthBuffer);
        std::memset(&DirectionalShadowTexture, 0, sizeof(DirectionalShadowTexture));
        DirectionalShadowTexture.surface = DirectionalShadowDepthBuffer.surface;
        DirectionalShadowTexture.viewFirstMip = 0U;
        DirectionalShadowTexture.viewNumMips = 1U;
        DirectionalShadowTexture.viewFirstSlice = 0U;
        DirectionalShadowTexture.viewNumSlices = 1U;
        DirectionalShadowTexture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_R, GX2_SQ_SEL_R, GX2_SQ_SEL_A);
        GX2InitTextureRegs(&DirectionalShadowTexture);
        GX2InitSampler(&DirectionalShadowSampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
        AreDirectionalShadowResourcesInitialized = true;
    }

    /// Releases the texture-backed directional shadow depth surface.
    void WiiUGx2Presenter::DestroyDirectionalShadowResources() {
        if (DirectionalShadowDepthBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&DirectionalShadowDepthBuffer.surface, NoGx2rResourceFlags);
        }

        std::memset(&DirectionalShadowDepthBuffer, 0, sizeof(DirectionalShadowDepthBuffer));
        std::memset(&DirectionalShadowTexture, 0, sizeof(DirectionalShadowTexture));
        std::memset(&DirectionalShadowSampler, 0, sizeof(DirectionalShadowSampler));
        AreDirectionalShadowResourcesInitialized = false;
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
        TvColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        TvColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&TvColorBuffer.surface);
        if (!GX2RCreateSurface(&TvColorBuffer.surface, ColorBufferSurfaceFlags)) {
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
        DrcColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        DrcColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&DrcColorBuffer.surface);
        if (!GX2RCreateSurface(&DrcColorBuffer.surface, ColorBufferSurfaceFlags)) {
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
        TvDepthBuffer.surface.format = DepthSurfaceFormat;
        TvDepthBuffer.surface.aa = GX2_AA_MODE1X;
        TvDepthBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        TvDepthBuffer.viewNumSlices = 1U;
        TvDepthBuffer.depthClear = 1.0f;
        TvDepthBuffer.stencilClear = 0U;
        GX2CalcSurfaceSizeAndAlignment(&TvDepthBuffer.surface);
        if (!GX2RCreateSurface(&TvDepthBuffer.surface, DepthBufferSurfaceFlags)) {
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
        DrcDepthBuffer.surface.format = DepthSurfaceFormat;
        DrcDepthBuffer.surface.aa = GX2_AA_MODE1X;
        DrcDepthBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        DrcDepthBuffer.viewNumSlices = 1U;
        DrcDepthBuffer.depthClear = 1.0f;
        DrcDepthBuffer.stencilClear = 0U;
        GX2CalcSurfaceSizeAndAlignment(&DrcDepthBuffer.surface);
        if (!GX2RCreateSurface(&DrcDepthBuffer.surface, DepthBufferSurfaceFlags)) {
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
            WHBGfxShaderGroup* standardShaderGroup = frame.GetHasDirectionalShadow()
                ? &ForwardStandardShadowedShaderGroup
                : &ForwardStandardShaderGroup;
            RenderStandard3DDrawCommandToColorBuffer(
                drawCommands[commandIndex],
                frame,
                cameraState,
                standardShaderGroup,
                frame.GetHasDirectionalShadow(),
                targetWidth,
                targetHeight);
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

    /// Renders the captured directional-shadow casters into the presenter-owned depth surface.
    void WiiUGx2Presenter::RenderDirectionalShadowDepthPass(GX2ContextState* contextState, const WiiUGx23DRenderFrame& frame) {
        if (contextState == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires one context state for a directional shadow pass.");
        } else if (!frame.GetHasDirectionalShadow()) {
            return;
        }

        const WiiUGx23DDirectionalShadowState& directionalShadowState = frame.GetDirectionalShadow();
        GX2SetContextState(contextState);
        GX2SetDepthBuffer(&DirectionalShadowDepthBuffer);
        GX2SetViewport(0.0f, 0.0f, static_cast<float>(DirectionalShadowMapSize), static_cast<float>(DirectionalShadowMapSize), 0.0f, 1.0f);
        GX2SetScissor(0, 0, DirectionalShadowMapSize, DirectionalShadowMapSize);
        GX2ClearDepthStencilEx(&DirectionalShadowDepthBuffer, 1.0f, 0U, GX2_CLEAR_FLAGS_DEPTH);
        GX2SetFetchShader(&ShadowDepthShaderGroup.fetchShader);
        GX2SetVertexShader(ShadowDepthShaderGroup.vertexShader);
        GX2SetPixelShader(ShadowDepthShaderGroup.pixelShader);
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
        GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LEQUAL);
        GX2SetTargetChannelMasks(
            static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0),
            static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0), static_cast<GX2ChannelMask>(0));

        GX2UniformBlock* transformBlock = GX2GetVertexUniformBlock(ShadowDepthShaderGroup.vertexShader, "TransformBuffer");
        if (transformBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires ShadowDepth TransformBuffer reflection.");
        }

        const std::vector<WiiUGx23DDrawCommand>& casterCommands = directionalShadowState.ShadowCasterCommands;
        for (std::size_t commandIndex = 0; commandIndex < casterCommands.size(); commandIndex++) {
            const WiiUGx23DDrawCommand& drawCommand = casterCommands[commandIndex];
            if (drawCommand.RuntimeModel == nullptr) {
                throw std::runtime_error("Wii U GX2 presenter requires one runtime model for every directional shadow caster.");
            }

            GX2DrawDone();
            float4x4 worldMatrix = drawCommand.WorldMatrix;
            float4x4 lightViewProjectionMatrix = directionalShadowState.LightViewProjection;
            float4x4 lightWorldViewProjection;
            float4x4::Multiply__ref0_ref1_out2(worldMatrix, lightViewProjectionMatrix, lightWorldViewProjection);
            const float transformData[56] = {
                worldMatrix.M11, worldMatrix.M21, worldMatrix.M31, worldMatrix.M41,
                worldMatrix.M12, worldMatrix.M22, worldMatrix.M32, worldMatrix.M42,
                worldMatrix.M13, worldMatrix.M23, worldMatrix.M33, worldMatrix.M43,
                worldMatrix.M14, worldMatrix.M24, worldMatrix.M34, worldMatrix.M44,
                lightWorldViewProjection.M11, lightWorldViewProjection.M21, lightWorldViewProjection.M31, lightWorldViewProjection.M41,
                lightWorldViewProjection.M12, lightWorldViewProjection.M22, lightWorldViewProjection.M32, lightWorldViewProjection.M42,
                lightWorldViewProjection.M13, lightWorldViewProjection.M23, lightWorldViewProjection.M33, lightWorldViewProjection.M43,
                lightWorldViewProjection.M14, lightWorldViewProjection.M24, lightWorldViewProjection.M34, lightWorldViewProjection.M44,
                worldMatrix.M11, worldMatrix.M21, worldMatrix.M31, worldMatrix.M41,
                worldMatrix.M12, worldMatrix.M22, worldMatrix.M32, worldMatrix.M42,
                worldMatrix.M13, worldMatrix.M23, worldMatrix.M33, worldMatrix.M43,
                worldMatrix.M14, worldMatrix.M24, worldMatrix.M34, worldMatrix.M44,
                0.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 0.0f, 0.0f
            };
            if (sizeof(transformData) != ShadowDepthTransformBuffer.elemSize) {
                throw std::runtime_error("Wii U GX2 presenter requires the ShadowDepth transform payload to match the full reflected TransformBuffer.");
            }
            void* transformUploadBuffer = GX2RLockBufferEx(&ShadowDepthTransformBuffer, NoGx2rResourceFlags);
            if (transformUploadBuffer == nullptr) {
                throw std::runtime_error("Wii U GX2 presenter could not lock the StandardShader transform buffer for a shadow caster.");
            }

            StoreFloatArrayAsLittleEndian(transformUploadBuffer, transformData, sizeof(transformData) / sizeof(transformData[0]));
            GX2RUnlockBufferEx(&ShadowDepthTransformBuffer, NoGx2rResourceFlags);
            GX2RInvalidateBuffer(&ShadowDepthTransformBuffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), ShadowDepthTransformBuffer.buffer, ShadowDepthTransformBuffer.elemSize);
            UploadSceneOpaqueMesh(*drawCommand.RuntimeModel);
            GX2SetVertexUniformBlock(transformBlock->offset, transformBlock->size, ShadowDepthTransformBuffer.buffer);
            GX2RSetAttributeBuffer(&SceneOpaquePositionBuffer, 0, SceneOpaquePositionBuffer.elemSize, 0);
            GX2RSetAttributeBuffer(&SceneOpaqueNormalBuffer, 1, SceneOpaqueNormalBuffer.elemSize, 0);
            GX2RSetAttributeBuffer(&SceneOpaqueTexCoordBuffer, 2, SceneOpaqueTexCoordBuffer.elemSize, 0);
            GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneOpaqueVertexCount, 0, 1);
        }

        GX2Invalidate(
            static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_DEPTH_BUFFER | GX2_INVALIDATE_MODE_TEXTURE),
            DirectionalShadowDepthBuffer.surface.image,
            DirectionalShadowDepthBuffer.surface.imageSize);
    }

    /// Renders one receiver command through the selected generated StandardShader program.
    void WiiUGx2Presenter::RenderStandard3DDrawCommandToColorBuffer(
        const WiiUGx23DDrawCommand& drawCommand,
        const WiiUGx23DRenderFrame& frame,
        const WiiUGx23DCameraState& cameraState,
        WHBGfxShaderGroup* shaderGroup,
        bool directionalShadowsEnabled,
        std::uint32_t targetWidth,
        std::uint32_t targetHeight) {
        if (drawCommand.RuntimeModel == nullptr || drawCommand.RuntimeMaterial == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires one runtime model and material for generated StandardShader rendering.");
        } else if (shaderGroup == nullptr || shaderGroup->vertexShader == nullptr || shaderGroup->pixelShader == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires one initialized generated StandardShader group.");
        } else if (targetWidth == 0U || targetHeight == 0U) {
            return;
        }

        float4x4 projectionMatrix;
        float4x4::CreatePerspectiveFieldOfView__out4(static_cast<float>(SceneDrivenFieldOfViewRadians), static_cast<float>(static_cast<double>(targetWidth) / static_cast<double>(targetHeight)), cameraState.NearPlaneDistance, cameraState.FarPlaneDistance, projectionMatrix);
        float4x4 worldMatrix = drawCommand.WorldMatrix;
        float4x4 viewMatrix = cameraState.ViewMatrix;
        float4x4 worldViewMatrix;
        float4x4 worldViewProjectionMatrix;
        float4x4::Multiply__ref0_ref1_out2(worldMatrix, viewMatrix, worldViewMatrix);
        float4x4::Multiply__ref0_ref1_out2(worldViewMatrix, projectionMatrix, worldViewProjectionMatrix);
        const WiiURuntimeMaterial& runtimeMaterial = *drawCommand.RuntimeMaterial;
        const WiiUGx2TextureHandle* baseColorTextureHandle = runtimeMaterial.GetBaseColorTextureHandle();
        if (baseColorTextureHandle == nullptr) {
            baseColorTextureHandle = &UiSolidWhiteTextureHandle;
        }

        GX2DrawDone();
        const float transformData[] = {
            drawCommand.WorldMatrix.M11, drawCommand.WorldMatrix.M21, drawCommand.WorldMatrix.M31, drawCommand.WorldMatrix.M41, drawCommand.WorldMatrix.M12, drawCommand.WorldMatrix.M22, drawCommand.WorldMatrix.M32, drawCommand.WorldMatrix.M42, drawCommand.WorldMatrix.M13, drawCommand.WorldMatrix.M23, drawCommand.WorldMatrix.M33, drawCommand.WorldMatrix.M43, drawCommand.WorldMatrix.M14, drawCommand.WorldMatrix.M24, drawCommand.WorldMatrix.M34, drawCommand.WorldMatrix.M44,
            worldViewProjectionMatrix.M11, worldViewProjectionMatrix.M21, worldViewProjectionMatrix.M31, worldViewProjectionMatrix.M41, worldViewProjectionMatrix.M12, worldViewProjectionMatrix.M22, worldViewProjectionMatrix.M32, worldViewProjectionMatrix.M42, worldViewProjectionMatrix.M13, worldViewProjectionMatrix.M23, worldViewProjectionMatrix.M33, worldViewProjectionMatrix.M43, worldViewProjectionMatrix.M14, worldViewProjectionMatrix.M24, worldViewProjectionMatrix.M34, worldViewProjectionMatrix.M44,
            drawCommand.WorldMatrix.M11, drawCommand.WorldMatrix.M21, drawCommand.WorldMatrix.M31, drawCommand.WorldMatrix.M41, drawCommand.WorldMatrix.M12, drawCommand.WorldMatrix.M22, drawCommand.WorldMatrix.M32, drawCommand.WorldMatrix.M42, drawCommand.WorldMatrix.M13, drawCommand.WorldMatrix.M23, drawCommand.WorldMatrix.M33, drawCommand.WorldMatrix.M43, drawCommand.WorldMatrix.M14, drawCommand.WorldMatrix.M24, drawCommand.WorldMatrix.M34, drawCommand.WorldMatrix.M44,
            cameraState.CameraPosition.X, cameraState.CameraPosition.Y, cameraState.CameraPosition.Z, 1.0f,
            runtimeMaterial.GetIsLit() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f
        };
        const float4 ambient = frame.GetAmbientLightColor();
        const float4 directionalColor = frame.GetDirectionalLight().Color;
        const float4 directionalDirection = frame.GetDirectionalLight().Direction;
        const float lightData[] = { ambient.X, ambient.Y, ambient.Z, ambient.W, 1.0f, 0.0f, 0.0f, 0.0f, directionalColor.X, directionalColor.Y, directionalColor.Z, 0.0f, directionalDirection.X, directionalDirection.Y, directionalDirection.Z, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        const float forwardLightData[72] = {
            ambient.X, ambient.Y, ambient.Z, ambient.W,
            1.0f, 0.0f, 0.0f, 0.0f,
            directionalColor.X, directionalColor.Y, directionalColor.Z, 0.0f,
            directionalDirection.X, directionalDirection.Y, directionalDirection.Z,
            directionalShadowsEnabled ? frame.GetDirectionalShadow().Strength : 0.0f
        };
        float shadowData[100] = {};
        shadowData[0] = directionalShadowsEnabled ? 1.0f : 0.0f;
        shadowData[1] = static_cast<float>(DirectionalShadowMapSize);
        shadowData[2] = static_cast<float>(DirectionalShadowMapSize);
        if (directionalShadowsEnabled) {
            const float4x4 shadowMatrix = frame.GetDirectionalShadow().LightViewProjection;
            shadowData[6] = 1.0f;
            shadowData[7] = 1.0f;
            shadowData[8] = 1.0f;
            shadowData[9] = frame.GetDirectionalShadow().Strength;
            shadowData[10] = 1.0f;
            shadowData[12] = shadowMatrix.M11;
            shadowData[13] = shadowMatrix.M21;
            shadowData[14] = shadowMatrix.M31;
            shadowData[15] = shadowMatrix.M41;
            shadowData[16] = shadowMatrix.M12;
            shadowData[17] = shadowMatrix.M22;
            shadowData[18] = shadowMatrix.M32;
            shadowData[19] = shadowMatrix.M42;
            shadowData[20] = shadowMatrix.M13;
            shadowData[21] = shadowMatrix.M23;
            shadowData[22] = shadowMatrix.M33;
            shadowData[23] = shadowMatrix.M43;
            shadowData[24] = shadowMatrix.M14;
            shadowData[25] = shadowMatrix.M24;
            shadowData[26] = shadowMatrix.M34;
            shadowData[27] = shadowMatrix.M44;
        }
        const float baseColorData[] = { runtimeMaterial.GetBaseColor().X, runtimeMaterial.GetBaseColor().Y, runtimeMaterial.GetBaseColor().Z, runtimeMaterial.GetBaseColor().W };
        const float roughnessData[] = { runtimeMaterial.GetRoughness(), 0.0f, 0.0f, 0.0f };
        const float metallicData[] = { runtimeMaterial.GetMetallic(), 0.0f, 0.0f, 0.0f };
        const float specularData[] = { runtimeMaterial.GetSpecular(), 0.0f, 0.0f, 0.0f };
        const float emissiveData[] = { runtimeMaterial.GetEmissiveColor().X, runtimeMaterial.GetEmissiveColor().Y, runtimeMaterial.GetEmissiveColor().Z, runtimeMaterial.GetEmissiveColor().W };
        GX2RBuffer* buffers[] = { &StandardShaderTransformBuffer, &StandardShaderForwardLightBuffer, &StandardShaderShadowBuffer, &StandardShaderBaseColorBuffer, &StandardShaderRoughnessBuffer, &StandardShaderMetallicBuffer, &StandardShaderSpecularBuffer, &StandardShaderEmissiveBuffer };
        const char* bufferNames[] = { "TransformBuffer", "ForwardLightBuffer", "ShadowBuffer", "BaseColorBuffer", "RoughnessBuffer", "MetallicBuffer", "SpecularBuffer", "EmissiveBuffer" };
        const float* payloads[] = { transformData, forwardLightData, shadowData, baseColorData, roughnessData, metallicData, specularData, emissiveData };
        const std::size_t payloadSizes[] = { sizeof(transformData), sizeof(forwardLightData), sizeof(shadowData), sizeof(baseColorData), sizeof(roughnessData), sizeof(metallicData), sizeof(specularData), sizeof(emissiveData) };
        for (std::size_t index = 0; index < sizeof(buffers) / sizeof(buffers[0]); index++) {
            void* uploadBuffer = GX2RLockBufferEx(buffers[index], NoGx2rResourceFlags);
            if (uploadBuffer == nullptr || payloadSizes[index] != buffers[index]->elemSize) {
                throw std::runtime_error(std::string("Wii U GX2 presenter could not upload ") + bufferNames[index] + ".\npayload=" + std::to_string(payloadSizes[index]) + "\nreflected=" + std::to_string(buffers[index]->elemSize));
            }
            StoreFloatArrayAsLittleEndian(uploadBuffer, payloads[index], payloadSizes[index] / sizeof(float));
            GX2RUnlockBufferEx(buffers[index], NoGx2rResourceFlags);
            GX2RInvalidateBuffer(buffers[index], GX2R_RESOURCE_USAGE_CPU_WRITE);
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), buffers[index]->buffer, buffers[index]->elemSize);
        }

        UploadSceneOpaqueMesh(*drawCommand.RuntimeModel);
        GX2SetFetchShader(&shaderGroup->fetchShader);
        GX2SetVertexShader(shaderGroup->vertexShader);
        GX2SetPixelShader(shaderGroup->pixelShader);
        GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
        GX2UniformBlock* vertexTransformBlock = GX2GetVertexUniformBlock(shaderGroup->vertexShader, "TransformBuffer");
        GX2UniformBlock* pixelTransformBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "TransformBuffer");
        GX2UniformBlock* forwardLightBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "ForwardLightBuffer");
        GX2UniformBlock* shadowBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "ShadowBuffer");
        GX2UniformBlock* baseColorBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "BaseColorBuffer");
        GX2UniformBlock* roughnessBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "RoughnessBuffer");
        GX2UniformBlock* metallicBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "MetallicBuffer");
        GX2UniformBlock* specularBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "SpecularBuffer");
        GX2UniformBlock* emissiveBlock = GX2GetPixelUniformBlock(shaderGroup->pixelShader, "EmissiveBuffer");
        if (vertexTransformBlock == nullptr || pixelTransformBlock == nullptr || forwardLightBlock == nullptr || shadowBlock == nullptr || baseColorBlock == nullptr || roughnessBlock == nullptr || metallicBlock == nullptr || specularBlock == nullptr || emissiveBlock == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires all reflected generated StandardShader bindings.");
        }
        GX2SetVertexUniformBlock(vertexTransformBlock->offset, vertexTransformBlock->size, StandardShaderTransformBuffer.buffer);
        GX2SetPixelUniformBlock(pixelTransformBlock->offset, pixelTransformBlock->size, StandardShaderTransformBuffer.buffer);
        GX2SetPixelUniformBlock(forwardLightBlock->offset, forwardLightBlock->size, StandardShaderForwardLightBuffer.buffer);
        GX2SetPixelUniformBlock(shadowBlock->offset, shadowBlock->size, StandardShaderShadowBuffer.buffer);
        GX2SetPixelUniformBlock(baseColorBlock->offset, baseColorBlock->size, StandardShaderBaseColorBuffer.buffer);
        GX2SetPixelUniformBlock(roughnessBlock->offset, roughnessBlock->size, StandardShaderRoughnessBuffer.buffer);
        GX2SetPixelUniformBlock(metallicBlock->offset, metallicBlock->size, StandardShaderMetallicBuffer.buffer);
        GX2SetPixelUniformBlock(specularBlock->offset, specularBlock->size, StandardShaderSpecularBuffer.buffer);
        GX2SetPixelUniformBlock(emissiveBlock->offset, emissiveBlock->size, StandardShaderEmissiveBuffer.buffer);
        const GX2SamplerVar* diffuseSamplerVar = ResolvePixelSamplerVar(shaderGroup->pixelShader, "DiffuseTexture");
        const GX2SamplerVar* roughnessSamplerVar = ResolvePixelSamplerVar(shaderGroup->pixelShader, "RoughnessTexture");
        const GX2SamplerVar* emissiveSamplerVar = ResolvePixelSamplerVar(shaderGroup->pixelShader, "EmissiveTexture");
        if (diffuseSamplerVar == nullptr || roughnessSamplerVar == nullptr || emissiveSamplerVar == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires named StandardShader material samplers.");
        }

        GX2SetPixelTexture(&baseColorTextureHandle->Texture, diffuseSamplerVar->location);
        GX2SetPixelSampler(&baseColorTextureHandle->Sampler, diffuseSamplerVar->location);
        GX2SetPixelTexture(&UiSolidWhiteTextureHandle.Texture, emissiveSamplerVar->location);
        GX2SetPixelSampler(&UiSolidWhiteTextureHandle.Sampler, emissiveSamplerVar->location);
        GX2SetPixelTexture(&UiSolidWhiteTextureHandle.Texture, roughnessSamplerVar->location);
        GX2SetPixelSampler(&UiSolidWhiteTextureHandle.Sampler, roughnessSamplerVar->location);
        if (directionalShadowsEnabled) {
            const GX2SamplerVar* shadowAtlasSamplerVar = ResolvePixelSamplerVar(shaderGroup->pixelShader, "shadowAtlasTexture");
            if (shadowAtlasSamplerVar == nullptr) {
                throw std::runtime_error("Wii U GX2 presenter requires the named directional-shadow atlas sampler.");
            }

            GX2SetPixelTexture(&DirectionalShadowTexture, shadowAtlasSamplerVar->location);
            GX2SetPixelSampler(&DirectionalShadowSampler, shadowAtlasSamplerVar->location);
        }
        GX2SetDepthOnlyControl(TRUE, TRUE, GX2_COMPARE_FUNC_LEQUAL);
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
        GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
        GX2RSetAttributeBuffer(&SceneOpaquePositionBuffer, 0, SceneOpaquePositionBuffer.elemSize, 0);
        GX2RSetAttributeBuffer(&SceneOpaqueNormalBuffer, 1, SceneOpaqueNormalBuffer.elemSize, 0);
        GX2RSetAttributeBuffer(&SceneOpaqueTexCoordBuffer, 2, SceneOpaqueTexCoordBuffer.elemSize, 0);
        GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneOpaqueVertexCount, 0, 1);
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

        float4x4 worldViewMatrix;
        float4x4 worldViewProjectionMatrix;
        float4x4::Multiply__ref0_ref1_out2(worldMatrix, viewMatrix, worldViewMatrix);
        float4x4::Multiply__ref0_ref1_out2(worldViewMatrix, projectionMatrix, worldViewProjectionMatrix);

        const WiiURuntimeMaterial& runtimeMaterial = *drawCommand.RuntimeMaterial;
        const WiiUGx2TextureHandle* baseColorTextureHandle = runtimeMaterial.GetBaseColorTextureHandle();
        if (baseColorTextureHandle == nullptr) {
            baseColorTextureHandle = &UiSolidWhiteTextureHandle;
        }

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

        GX2SetPixelUniformBlock(
            materialUniformBlock->offset,
            materialUniformBlock->size,
            SceneOpaqueMaterialBuffer.buffer);
        GX2SetPixelUniformBlock(
            lightUniformBlock->offset,
            lightUniformBlock->size,
            SceneOpaqueLightBuffer.buffer);
        GX2SetPixelTexture(&baseColorTextureHandle->Texture, SceneOpaqueShaderGroup.pixelShader->samplerVars[0].location);
        GX2SetPixelSampler(&baseColorTextureHandle->Sampler, SceneOpaqueShaderGroup.pixelShader->samplerVars[0].location);
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
        GX2RSetAttributeBuffer(&SceneOpaqueTexCoordBuffer, 2, SceneOpaqueTexCoordBuffer.elemSize, 0);
        GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, SceneOpaqueVertexCount, 0, 1);
    }

    /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using model-space positions and normals.
    void WiiUGx2Presenter::UploadSceneOpaqueMesh(const WiiURuntimeModel& runtimeModel) {
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<float>& sourceNormalData = runtimeModel.GetNormalData();
        const std::vector<float>& sourceTexCoordData = runtimeModel.GetTexCoordData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneOpaqueResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize opaque scene resources before uploading geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene position data.");
        } else if (sourceNormalData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene normal data.");
        } else if (sourceTexCoordData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene texcoord data.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene index data.");
        }

        std::vector<float> expandedPositionData;
        std::vector<float> expandedNormalData;
        std::vector<float> expandedTexCoordData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        expandedNormalData.reserve(static_cast<std::size_t>(indexData.size()) * 3U);
        expandedTexCoordData.reserve(static_cast<std::size_t>(indexData.size()) * 2U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t positionOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            const std::size_t normalOffset = static_cast<std::size_t>(sourceIndex) * 3U;
            const std::size_t texCoordOffset = static_cast<std::size_t>(sourceIndex) * 2U;
            if (positionOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene index outside the uploaded position range.");
            } else if (normalOffset + 2U >= sourceNormalData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene index outside the uploaded normal range.");
            } else if (texCoordOffset + 1U >= sourceTexCoordData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene index outside the uploaded texcoord range.");
            }

            expandedPositionData.push_back(sourcePositionData[positionOffset + 0U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 1U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 2U]);
            expandedPositionData.push_back(sourcePositionData[positionOffset + 3U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 0U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 1U]);
            expandedNormalData.push_back(sourceNormalData[normalOffset + 2U]);
            expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 0U]);
            expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 1U]);
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr || SceneOpaqueNormalBuffer.buffer != nullptr || SceneOpaqueTexCoordBuffer.buffer != nullptr || SceneOpaqueIndexBuffer.buffer != nullptr) {
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

        if (SceneOpaqueTexCoordBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueTexCoordBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueTexCoordBuffer, 0, sizeof(SceneOpaqueTexCoordBuffer));
        }

        if (SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        }

        InitializeSceneOpaqueVertexBuffer(&SceneOpaquePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()), SceneOpaquePositionElementSize, 4U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueNormalBuffer, expandedNormalData.data(), static_cast<std::uint32_t>(expandedNormalData.size()), SceneOpaqueNormalElementSize, 3U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueTexCoordBuffer, expandedTexCoordData.data(), static_cast<std::uint32_t>(expandedTexCoordData.size()), SceneOpaqueTexCoordElementSize, 2U);
        SceneOpaqueVertexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
    }

    /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using CPU-expanded clip-space positions plus CPU-transformed world-space normals.
    void WiiUGx2Presenter::UploadSceneOpaqueMeshClipSpace(const WiiURuntimeModel& runtimeModel, const float4x4& worldMatrix, const float4x4& worldViewProjectionMatrix) {
        const std::vector<float>& sourcePositionData = runtimeModel.GetPositionData();
        const std::vector<float>& sourceNormalData = runtimeModel.GetNormalData();
        const std::vector<float>& sourceTexCoordData = runtimeModel.GetTexCoordData();
        const std::vector<std::uint16_t>& indexData = runtimeModel.GetIndexData();
        if (!AreSceneOpaqueResourcesInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must initialize opaque scene resources before uploading clip-space geometry.");
        } else if (sourcePositionData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene position data for clip-space upload.");
        } else if (sourceNormalData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene normal data for clip-space upload.");
        } else if (sourceTexCoordData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene texcoord data for clip-space upload.");
        } else if (indexData.empty()) {
            throw std::runtime_error("Wii U GX2 presenter requires non-empty opaque-scene index data for clip-space upload.");
        }

        std::vector<float> expandedPositionData;
        std::vector<float> expandedNormalData;
        std::vector<float> expandedTexCoordData;
        expandedPositionData.reserve(static_cast<std::size_t>(indexData.size()) * 4U);
        expandedNormalData.reserve(static_cast<std::size_t>(indexData.size()) * 3U);
        expandedTexCoordData.reserve(static_cast<std::size_t>(indexData.size()) * 2U);
        for (std::uint16_t sourceIndex : indexData) {
            const std::size_t positionOffset = static_cast<std::size_t>(sourceIndex) * 4U;
            const std::size_t normalOffset = static_cast<std::size_t>(sourceIndex) * 3U;
            const std::size_t texCoordOffset = static_cast<std::size_t>(sourceIndex) * 2U;
            if (positionOffset + 3U >= sourcePositionData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene clip-space index outside the uploaded position range.");
            } else if (normalOffset + 2U >= sourceNormalData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene clip-space index outside the uploaded normal range.");
            } else if (texCoordOffset + 1U >= sourceTexCoordData.size()) {
                throw std::runtime_error("Wii U GX2 presenter received one opaque-scene clip-space index outside the uploaded texcoord range.");
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
            expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 0U]);
            expandedTexCoordData.push_back(sourceTexCoordData[texCoordOffset + 1U]);
        }

        if (SceneOpaquePositionBuffer.buffer != nullptr || SceneOpaqueNormalBuffer.buffer != nullptr || SceneOpaqueTexCoordBuffer.buffer != nullptr || SceneOpaqueIndexBuffer.buffer != nullptr) {
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

        if (SceneOpaqueTexCoordBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueTexCoordBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueTexCoordBuffer, 0, sizeof(SceneOpaqueTexCoordBuffer));
        }

        if (SceneOpaqueIndexBuffer.buffer != nullptr) {
            GX2RDestroyBufferEx(&SceneOpaqueIndexBuffer, NoGx2rResourceFlags);
            std::memset(&SceneOpaqueIndexBuffer, 0, sizeof(SceneOpaqueIndexBuffer));
        }

        InitializeSceneOpaqueVertexBuffer(&SceneOpaquePositionBuffer, expandedPositionData.data(), static_cast<std::uint32_t>(expandedPositionData.size()), SceneOpaquePositionElementSize, 4U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueNormalBuffer, expandedNormalData.data(), static_cast<std::uint32_t>(expandedNormalData.size()), SceneOpaqueNormalElementSize, 3U);
        InitializeSceneOpaqueVertexBuffer(&SceneOpaqueTexCoordBuffer, expandedTexCoordData.data(), static_cast<std::uint32_t>(expandedTexCoordData.size()), SceneOpaqueTexCoordElementSize, 2U);
        SceneOpaqueVertexCount = static_cast<std::uint32_t>(expandedPositionData.size() / 4U);
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
