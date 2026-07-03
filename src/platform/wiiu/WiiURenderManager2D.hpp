#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"
#include "platform/wiiu/WiiUSoftwareSurface.hpp"

class RuntimeTexture;
class FontAsset;
class IDrawable2D;
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

    /// Stores one CPU-readable runtime texture payload used by Wii U software-surface rasterization.
    struct WiiUTexturePixelData {
        /// Runtime texture instance resolved by the shared engine.
        RuntimeTexture* Texture;

        /// Logical texture width in pixels.
        std::uint32_t Width;

        /// Logical texture height in pixels.
        std::uint32_t Height;

        /// Packed ARGB8888 pixels in row-major order.
        std::vector<std::uint32_t> Pixels;
    };

    /// Provides the minimum Wii U 2D renderer bridge required to make authored menu scenes visible on software surfaces.
    class WiiURenderManager2D final : public ::RenderManager2D, public IRenderVisitor2D {
    public:
        /// Creates the Wii U 2D render bridge.
        WiiURenderManager2D();

        /// Releases reusable render state owned by the Wii U 2D render bridge.
        ~WiiURenderManager2D() override;

        /// Attaches the software surfaces that receive TV and DRC menu output.
        void AttachSurface(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface);

        /// Rebuilds one platform-owned cooked texture payload into a CPU-readable Wii U runtime texture.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;

        /// Rebuilds one shared-engine texture asset into a CPU-readable Wii U runtime texture.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;

        /// Releases one Wii U runtime texture.
        void ReleaseTexture(::RuntimeTexture* texture) override;

        /// Releases one font asset.
        void ReleaseFont(::FontAsset* font) override;

        /// Executes one full 2D draw pass into the attached software surfaces.
        void Draw() override;

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

        /// Executes one command list generated from the active camera render queue into the attached software surfaces.
        void ExecuteCommandList(RenderCommandList2D* commandList, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Executes one textured sprite command from the generated 2D command list.
        void ExecuteTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Executes one text glyph command from the generated 2D command list.
        void ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Executes one rounded-rectangle command from the generated 2D command list.
        void ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Draws one solid logical-space rectangle into both attached software surfaces.
        void DrawSolidQuad2D(float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Draws one textured logical-space rectangle into both attached software surfaces.
        void DrawTexturedQuad2D(float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, RuntimeTexture* texture, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Returns one packed texture record for the supplied runtime texture or null when it is unknown to the renderer.
        WiiUTexturePixelData* FindTexturePixelData(RuntimeTexture* texture);

        /// Builds one packed ARGB8888 pixel buffer from one shared-engine texture asset payload.
        static std::vector<std::uint32_t> DecodeTexturePixels(TextureAsset* data);

        /// Draws one solid logical-space rectangle into one target software surface.
        static void DrawSolidQuadToSurface(WiiUSoftwareSurface* surface, float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Draws one textured logical-space rectangle into one target software surface.
        static void DrawTexturedQuadToSurface(WiiUSoftwareSurface* surface, float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, const WiiUTexturePixelData& texturePixelData, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight);

        /// Packs one 8-bit RGBA color into the ARGB8888 layout used by the software surfaces.
        static std::uint32_t PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);

        /// Applies one byte4 tint to one packed ARGB8888 source pixel.
        static std::uint32_t ApplyTint(std::uint32_t argbColor, byte4 color);

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

        /// Stores the currently attached TV software surface.
        WiiUSoftwareSurface* TvSurface;

        /// Stores the currently attached DRC software surface.
        WiiUSoftwareSurface* DrcSurface;

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
    };
}

#endif
