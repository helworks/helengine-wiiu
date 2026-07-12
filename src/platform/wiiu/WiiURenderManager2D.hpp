#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include <gx2/sampler.h>
#include <gx2/texture.h>

#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"
#include "platform/wiiu/WiiUGx2RenderFrame.hpp"
#include "platform/wiiu/WiiUGx2TextureHandle.hpp"

class RuntimeTexture;
class FontAsset;
class IDrawable2D;
class IContentStreamSource;
class TextureAsset;
class byte4;
class float4;
class RenderCommandList2D;
class RenderCommandListBuilder2D;

namespace helengine::wiiu {
    /// Stores one captured sprite draw request for the current Wii U 2D frame.
    struct WiiUSpriteDrawCommand {
        /// Pointer to the shared-engine sprite drawable submitted during the current frame.
        ISpriteDrawable2D* Drawable;
    };

    /// Stores one captured text draw request for the current Wii U 2D frame.
    struct WiiUTextDrawCommand {
        /// Pointer to the shared-engine text drawable submitted during the current frame.
        ITextDrawable2D* Drawable;
    };

    /// Stores one captured rounded-rectangle draw request for the current Wii U 2D frame.
    struct WiiURoundedRectDrawCommand {
        /// Pointer to the shared-engine rounded-rectangle drawable submitted during the current frame.
        IRoundedRectDrawable2D* Drawable;
    };

    /// Stores one GX2-backed runtime texture payload used by pure Wii U 2D rendering.
    struct WiiUTexturePixelData {
        /// Runtime texture instance resolved by the shared engine.
        RuntimeTexture* Texture;

        /// Logical texture width in pixels.
        std::uint32_t Width;

        /// Logical texture height in pixels.
        std::uint32_t Height;

        /// GX2 texture and sampler state consumed by the presenter.
        WiiUGx2TextureHandle Gx2TextureHandle;
    };

    /// Provides the minimum Wii U 2D renderer bridge required to capture authored menu scenes into GX2-ready quad commands.
    class WiiURenderManager2D final : public ::RenderManager2D, public IRenderVisitor2D {
    public:
        /// Creates the Wii U 2D render bridge.
        WiiURenderManager2D();

        /// Releases reusable render state owned by the Wii U 2D render bridge.
        ~WiiURenderManager2D() override;

        /// Rebuilds one platform-owned cooked texture payload into a CPU-readable Wii U runtime texture using the legacy path-based generated-core contract.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath);

        /// Rebuilds one platform-owned cooked texture payload into a CPU-readable Wii U runtime texture using the current content-stream-based generated-core contract.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);

        /// Rebuilds one shared-engine texture asset into a GX2-backed Wii U runtime texture.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;

        /// Releases one Wii U runtime texture.
        void ReleaseTexture(::RuntimeTexture* texture) override;

        /// Releases one font asset.
        void ReleaseFont(::FontAsset* font) override;

        /// Executes one full 2D draw pass into the attached software surfaces.
        void Draw() override;

        /// Returns the most recently captured GX2-ready frame.
        const WiiUGx2RenderFrame& GetCurrentFrame() const;

        /// Visits one ordered 2D drawable from the active camera queue.
        void Visit(IDrawable2D* drawable) override;

        /// Clears deferred release state after an engine frame.
        void FlushReleasedTextures() override;

        /// Accepts a rounded-rectangle draw request without issuing software rasterization yet.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;

        /// Accepts a sprite draw request without issuing software rasterization yet.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;

        /// Accepts a text draw request without issuing software rasterization yet.
        void DrawText(::ITextDrawable2D* text) override;

    private:
        /// Resets the current frame capture state before the next draw pass begins.
        void BeginFrame();

        /// Returns whether the current frame captured any 2D draw requests.
        bool HasCapturedDrawables() const;

        /// Captures one rounded-rectangle drawable for the current frame.
        void SubmitRoundedRect(::IRoundedRectDrawable2D* shape);

        /// Captures one sprite drawable for the current frame.
        void SubmitSprite(::ISpriteDrawable2D* sprite);

        /// Captures one text drawable for the current frame.
        void SubmitText(::ITextDrawable2D* text);

        /// Captures one command list generated from the active camera render queue into the current GX2 frame.
        void CaptureCommandList(RenderCommandList2D* commandList, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Captures one textured sprite command from the generated 2D command list.
        void CaptureTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Captures one text glyph command from the generated 2D command list.
        void CaptureGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Captures one rounded-rectangle command from the generated 2D command list.
        void CaptureRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Captures one solid logical-space rectangle into the current GX2 frame.
        void CaptureSolidQuad2D(float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Captures one textured logical-space rectangle into the current GX2 frame.
        void CaptureTexturedQuad2D(float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, RuntimeTexture* texture, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Returns one GX2 texture record for the supplied runtime texture or null when it is unknown to the renderer.
        WiiUTexturePixelData* FindTexturePixelData(RuntimeTexture* texture);

        /// Builds one packed ARGB8888 pixel buffer from one shared-engine texture asset payload.
        static std::vector<std::uint32_t> DecodeTexturePixels(TextureAsset* data);

        /// Initializes one GX2 texture handle from decoded ARGB8888 pixels.
        static void InitializeTextureHandle(WiiUGx2TextureHandle* textureHandle, std::uint32_t width, std::uint32_t height, const std::vector<std::uint32_t>& pixels);

        /// Releases one GX2 texture handle owned by the Wii U 2D bridge.
        static void DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle);

        /// Packs one 8-bit RGBA color into the ARGB8888 layout used by the software surfaces.
        static std::uint32_t PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);

        /// Expands one 4-bit color channel into 8-bit precision.
        static std::uint8_t Expand4To8(std::uint8_t value);

        /// Expands one 5-bit color channel into 8-bit precision.
        static std::uint8_t Expand5To8(std::uint16_t value);

        /// Expands one 3-bit alpha channel into 8-bit precision.
        static std::uint8_t Expand3To8(std::uint16_t value);

        /// Decodes one packed GX RGB5A3 texel into ARGB8888.
        static std::uint32_t DecodeRgb5A3(std::uint16_t pixel);

        /// Releases one transient cooked texture asset after the runtime texture has been rebuilt from its payload.
        static void ReleaseTransientTextureAsset(TextureAsset* asset);

        /// Captured sprite draw requests in shared-engine render order.
        std::vector<WiiUSpriteDrawCommand> SpriteQueue;

        /// Captured text draw requests in shared-engine render order.
        std::vector<WiiUTextDrawCommand> TextQueue;

        /// Captured rounded-rectangle draw requests in shared-engine render order.
        std::vector<WiiURoundedRectDrawCommand> RoundedRectQueue;

        /// CPU-readable pixel payloads keyed by the runtime texture instances returned to the shared engine.
        std::vector<WiiUTexturePixelData> TexturePixelDataRecords;

        /// Reusable generated 2D command builder kept alive across frames.
        RenderCommandListBuilder2D* CommandListBuilder;

        /// Stores the GX2-ready frame captured during the most recent draw call.
        WiiUGx2RenderFrame CurrentFrame;

        /// Stores the 1x1 white texture used to render solid-color quads through the textured quad shader.
        WiiUGx2TextureHandle SolidWhiteTextureHandle;
    };
}

#endif
