#pragma once

#include <cstdint>

#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <gx2r/buffer.h>
#include <whb/gfx.h>

#include "platform/wiiu/WiiUGx2RenderFrame.hpp"
#include "platform/wiiu/WiiUGx23DRenderFrame.hpp"
#include "platform/wiiu/WiiUGx2TextureHandle.hpp"

namespace helengine::wiiu {
    class WiiURuntimeModel;

    /// Owns the minimal GX2 presentation seam that renders captured Wii U 2D frames into GX2-owned display buffers.
    class WiiUGx2Presenter {
    public:
        /// Creates one uninitialized GX2 presenter.
        WiiUGx2Presenter();

        /// Releases all GX2-owned presentation resources.
        ~WiiUGx2Presenter();

        /// Initializes the GX2 presentation path for TV and DRC output.
        bool Initialize();

        /// Renders and presents one captured Wii U 2D frame.
        void RenderFrame(const WiiUGx2RenderFrame& frame);

        /// Renders and presents one captured Wii U 3D frame plus the captured 2D overlay.
        void RenderFrame(const WiiUGx23DRenderFrame& frame3D, const WiiUGx2RenderFrame& frame2D);

        /// Renders one presenter-owned pure GX2 clear-only frame for early bring-up verification.
        void RenderDiagnosticClearFrame();

        /// Renders one presenter-owned pure GX2 clear-plus-square frame for bring-up verification.
        void RenderDiagnosticSquareFrame();

        /// Renders one presenter-owned pure GX2 clear-plus-triangle frame for first 3D shader verification.
        void RenderDiagnosticTriangleFrame();

        /// Uploads one runtime model into the temporary scene-cube GX2 mesh path.
        void ConfigureSceneCubeMesh(const WiiURuntimeModel& runtimeModel);

        /// Renders the configured scene-cube mesh to both displays.
        void RenderSceneCubeFrame();

    private:
        /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
        void Shutdown();

        /// Initializes the TV color buffer used for software-surface upload and presentation.
        void InitializeTvColorBuffer();

        /// Initializes the DRC color buffer used for software-surface upload and presentation.
        void InitializeDrcColorBuffer();

        /// Initializes the TV depth buffer used for scene-driven 3D depth testing.
        void InitializeTvDepthBuffer();

        /// Initializes the DRC depth buffer used for scene-driven 3D depth testing.
        void InitializeDrcDepthBuffer();

        /// Initializes the presenter-owned shader and vertex buffers used by the diagnostic GX2 square path.
        void InitializeDiagnosticSquareResources();

        /// Releases the presenter-owned shader and vertex buffers used by the diagnostic GX2 square path.
        void DestroyDiagnosticSquareResources();

        /// Initializes one presenter-owned diagnostic vertex buffer from immutable float vertex data.
        void InitializeDiagnosticSquareBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t vertexCount);

        /// Initializes the presenter-owned shader and vertex buffers used by the diagnostic GX2 triangle path.
        void InitializeDiagnosticTriangleResources();

        /// Releases the presenter-owned shader and vertex buffers used by the diagnostic GX2 triangle path.
        void DestroyDiagnosticTriangleResources();

        /// Initializes one presenter-owned diagnostic vertex buffer from immutable float vertex data for triangle rendering.
        void InitializeDiagnosticTriangleBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t vertexCount);

        /// Initializes the presenter-owned uniform buffer that stores the fixed transform used by the translated diagnostic triangle.
        void InitializeDiagnosticTriangleTransformBuffer();

        /// Initializes the presenter-owned shader resources used by the generic opaque Wii U scene path.
        void InitializeSceneOpaqueResources();

        /// Releases the presenter-owned shader resources used by the generic opaque Wii U scene path.
        void DestroySceneOpaqueResources();

        /// Initializes one presenter-owned opaque-scene vertex buffer from immutable float vertex data.
        void InitializeSceneOpaqueVertexBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t floatCount, std::uint32_t elementSize, std::uint32_t elementStride);

        /// Initializes one presenter-owned opaque-scene index buffer from immutable 16-bit index data.
        void InitializeSceneOpaqueIndexBuffer(GX2RBuffer* buffer, const std::uint16_t* sourceData, std::uint32_t indexCount);

        /// Initializes the presenter-owned transform uniform buffer used by the generic opaque Wii U scene path.
        void InitializeSceneOpaqueTransformBuffer();

        /// Initializes the presenter-owned material uniform buffer used by the generic opaque Wii U scene path.
        void InitializeSceneOpaqueMaterialBuffer();

        /// Initializes the presenter-owned light uniform buffer used by the generic opaque Wii U scene path.
        void InitializeSceneOpaqueLightBuffer();

        /// Initializes the presenter-owned shader resources used by the temporary scene-cube GX2 mesh path.
        void InitializeSceneCubeResources();

        /// Releases the presenter-owned shader resources used by the temporary scene-cube GX2 mesh path.
        void DestroySceneCubeResources();

        /// Initializes one presenter-owned scene-cube vertex buffer from immutable float vertex data.
        void InitializeSceneCubeVertexBuffer(GX2RBuffer* buffer, const float* sourceData, std::uint32_t floatCount);

        /// Initializes one presenter-owned scene-cube index buffer from immutable 16-bit index data.
        void InitializeSceneCubeIndexBuffer(GX2RBuffer* buffer, const std::uint16_t* sourceData, std::uint32_t indexCount);

        /// Initializes the presenter-owned uniform buffer that stores the fixed transform used by the scene-cube path.
        void InitializeSceneCubeTransformBuffer();

        /// Initializes the presenter-owned shader, buffers, and fallback texture used by the pure GX2 UI path.
        void InitializeUiQuadResources();

        /// Releases the presenter-owned shader, buffers, and fallback texture used by the pure GX2 UI path.
        void DestroyUiQuadResources();

        /// Initializes one presenter-owned UI vertex buffer for dynamic quad data.
        void InitializeUiQuadBuffer(GX2RBuffer* buffer, std::uint32_t elementSize, std::uint32_t elementCount);

        /// Grows the presenter-owned UI buffers so one full captured frame can be uploaded without overwriting in-flight quad data.
        void EnsureUiQuadBufferCapacity(std::uint32_t requiredVertexCount);

        /// Initializes the presenter-owned 1x1 white texture used for solid-color quad rendering.
        void InitializeUiSolidWhiteTexture();

        /// Releases one presenter-owned texture handle.
        void DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle);

        /// Renders one captured frame into one target color buffer.
        void RenderFrameToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer, const WiiUGx2RenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight);

        /// Renders one captured 3D frame into one target color buffer using one paired depth buffer.
        void Render3DFrameToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer, GX2DepthBuffer* depthBuffer, const WiiUGx23DRenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight);

        /// Renders one captured 3D draw command into the currently bound color buffer.
        void Render3DDrawCommandToColorBuffer(const WiiUGx23DDrawCommand& drawCommand, const WiiUGx23DRenderFrame& frame, const WiiUGx23DCameraState& cameraState, std::uint32_t targetWidth, std::uint32_t targetHeight);

        /// Renders the captured 2D quad commands into the currently bound color buffer without clearing it first.
        void RenderQuadCommandsToColorBuffer(const WiiUGx2RenderFrame& frame, std::uint32_t targetWidth, std::uint32_t targetHeight);

        /// Renders one captured quad command into one target color buffer using the already-uploaded vertex data for the supplied quad index.
        void RenderQuadCommandToColorBuffer(const WiiUGx2QuadCommand& command, std::uint32_t quadIndex, std::uint32_t logicalWidth, std::uint32_t logicalHeight, std::uint32_t targetWidth, std::uint32_t targetHeight);

        /// Uploads one runtime model into the presenter-owned flat-color mesh path using one CPU-expanded clip-space transform.
        void UploadSceneCubeMesh(const WiiURuntimeModel& runtimeModel, const float4x4& worldViewProjectionMatrix);

        /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using model-space positions, normals, and UVs.
        void UploadSceneOpaqueMesh(const WiiURuntimeModel& runtimeModel);

        /// Uploads one runtime model into the presenter-owned generic opaque scene buffers using CPU-expanded clip-space positions plus CPU-transformed world-space normals and copied UVs.
        void UploadSceneOpaqueMeshClipSpace(const WiiURuntimeModel& runtimeModel, const float4x4& worldMatrix, const float4x4& worldViewProjectionMatrix);

        /// Renders the presenter-owned diagnostic square into one target color buffer with the supplied GX2 context state.
        void RenderDiagnosticSquareToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);

        /// Renders the presenter-owned diagnostic triangle into one target color buffer with the supplied GX2 context state.
        void RenderDiagnosticTriangleToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);

        /// Renders the configured scene-cube mesh into one target color buffer with the supplied GX2 context state.
        void RenderSceneCubeToColorBuffer(GX2ContextState* contextState, GX2ColorBuffer* colorBuffer);

        /// Presents the current TV and DRC color buffers to their scan buffers.
        void PresentScanBuffers();

        /// Appends one presenter trace message to the shared Wii U runtime trace file.
        void AppendInitializationTrace(const char* format, ...);

        /// Tracks whether GX2 resources were initialized successfully.
        bool IsInitialized;

        /// Stores the TV scan buffer pointer returned by the GX2 allocation path.
        void* TvScanBuffer;

        /// Stores the DRC scan buffer pointer returned by the GX2 allocation path.
        void* DrcScanBuffer;

        /// Stores the command buffer pool required by GX2 command submission.
        void* CommandBufferPool;

        /// Stores the TV color buffer used for steady-state presentation.
        GX2ColorBuffer TvColorBuffer;

        /// Stores the DRC color buffer used for steady-state presentation.
        GX2ColorBuffer DrcColorBuffer;

        /// Stores the TV depth buffer used for scene-driven 3D depth testing.
        GX2DepthBuffer TvDepthBuffer;

        /// Stores the DRC depth buffer used for scene-driven 3D depth testing.
        GX2DepthBuffer DrcDepthBuffer;

        /// Stores the TV GX2 context state used before copying the TV color buffer to the scan buffer.
        GX2ContextState* TvContextState;

        /// Stores the DRC GX2 context state used before copying the DRC color buffer to the scan buffer.
        GX2ContextState* DrcContextState;

        /// Tracks whether the diagnostic shader group and vertex buffers were initialized successfully.
        bool AreDiagnosticSquareResourcesInitialized;

        /// Stores the presenter-owned diagnostic shader group loaded from the embedded Wii U sample shader blob.
        WHBGfxShaderGroup DiagnosticSquareShaderGroup;

        /// Stores the presenter-owned diagnostic position buffer used for square vertices.
        GX2RBuffer DiagnosticSquarePositionBuffer;

        /// Stores the presenter-owned diagnostic color buffer used for square vertex colors.
        GX2RBuffer DiagnosticSquareColorBuffer;

        /// Tracks whether the diagnostic triangle shader group and vertex buffers were initialized successfully.
        bool AreDiagnosticTriangleResourcesInitialized;

        /// Stores the presenter-owned diagnostic shader group loaded from the embedded triangle shader blob.
        WHBGfxShaderGroup DiagnosticTriangleShaderGroup;

        /// Stores the presenter-owned diagnostic position buffer used for triangle vertices.
        GX2RBuffer DiagnosticTrianglePositionBuffer;

        /// Stores the presenter-owned diagnostic color buffer used for triangle vertex colors.
        GX2RBuffer DiagnosticTriangleColorBuffer;

        /// Stores the presenter-owned uniform buffer used to translate the diagnostic triangle through the vertex shader.
        GX2RBuffer DiagnosticTriangleTransformBuffer;

        /// Tracks whether the generic opaque-scene shader group and uniform resources were initialized successfully.
        bool AreSceneOpaqueResourcesInitialized;

        /// Stores the presenter-owned shader group loaded from the embedded generic opaque-scene shader blob.
        WHBGfxShaderGroup SceneOpaqueShaderGroup;

        /// Stores the presenter-owned position buffer used for generic opaque-scene vertices.
        GX2RBuffer SceneOpaquePositionBuffer;

        /// Stores the presenter-owned normal buffer used for generic opaque-scene vertices.
        GX2RBuffer SceneOpaqueNormalBuffer;

        /// Stores the presenter-owned UV buffer used for generic opaque-scene vertices.
        GX2RBuffer SceneOpaqueTexCoordBuffer;

        /// Stores the presenter-owned index buffer used for generic opaque-scene triangles.
        GX2RBuffer SceneOpaqueIndexBuffer;

        /// Stores the presenter-owned transform uniform buffer used by the generic opaque-scene shader path.
        GX2RBuffer SceneOpaqueTransformBuffer;

        /// Stores the presenter-owned material uniform buffer used by the generic opaque-scene shader path.
        GX2RBuffer SceneOpaqueMaterialBuffer;

        /// Stores the presenter-owned light uniform buffer used by the generic opaque-scene shader path.
        GX2RBuffer SceneOpaqueLightBuffer;

        /// Stores the number of expanded vertices currently uploaded for the generic opaque-scene path.
        std::uint32_t SceneOpaqueVertexCount;

        /// Tracks whether the temporary scene-cube shader group and uniform resources were initialized successfully.
        bool AreSceneCubeResourcesInitialized;

        /// Tracks whether scene-cube mesh geometry has been uploaded successfully.
        bool IsSceneCubeMeshConfigured;

        /// Stores the presenter-owned shader group loaded from the embedded scene-cube shader blob.
        WHBGfxShaderGroup SceneCubeShaderGroup;

        /// Stores the presenter-owned position buffer used for scene-cube vertices.
        GX2RBuffer SceneCubePositionBuffer;

        /// Stores the presenter-owned index buffer used for scene-cube triangles.
        GX2RBuffer SceneCubeIndexBuffer;

        /// Stores the presenter-owned uniform buffer used to transform the scene cube before clip-space output.
        GX2RBuffer SceneCubeTransformBuffer;

        /// Stores the number of indices currently uploaded for the scene cube.
        std::uint32_t SceneCubeIndexCount;

        /// Tracks whether the pure GX2 UI shader, buffers, and fallback texture were initialized successfully.
        bool AreUiQuadResourcesInitialized;

        /// Stores the presenter-owned GX2 shader group used for textured UI quads.
        WHBGfxShaderGroup UiQuadShaderGroup;

        /// Stores the presenter-owned dynamic position buffer used for textured UI quads.
        GX2RBuffer UiQuadPositionBuffer;

        /// Stores the presenter-owned dynamic texture-coordinate buffer used for textured UI quads.
        GX2RBuffer UiQuadTexCoordBuffer;

        /// Stores the presenter-owned dynamic vertex-color buffer used for textured UI quads.
        GX2RBuffer UiQuadColorBuffer;

        /// Stores the presenter-owned 1x1 white texture used for solid-color quad rendering.
        WiiUGx2TextureHandle UiSolidWhiteTextureHandle;

        /// Stores the raw scan-buffer size required for TV presentation.
        std::uint32_t TvScanBufferSize;

        /// Stores the raw scan-buffer size required for DRC presentation.
        std::uint32_t DrcScanBufferSize;
    };
}
