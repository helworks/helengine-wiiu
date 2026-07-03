#include "platform/wiiu/WiiURenderManager2D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "IRenderQueue2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "ObjectManager.hpp"
#include "RenderCommand2DType.hpp"
#include "RenderCommandList2D.hpp"
#include "RenderCommandListBuilder2D.hpp"
#include "RuntimeTexture.hpp"
#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "runtime/finally.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"

namespace helengine::wiiu {
    namespace {
        constexpr std::uint16_t LogicalFrameWidth = 1280U;
        constexpr std::uint16_t LogicalFrameHeight = 720U;
        constexpr std::uint32_t TransparentBlack = 0x00000000U;
        constexpr std::uint32_t OpaqueBlack = 0xFF000000U;
    }

    /// Creates the Wii U 2D render bridge.
    WiiURenderManager2D::WiiURenderManager2D()
        : RenderManager2D()
        , TvSurface(nullptr)
        , DrcSurface(nullptr)
        , SpriteQueue()
        , TextQueue()
        , RoundedRectQueue()
        , TexturePixelDataRecords()
        , CommandListBuilder(new RenderCommandListBuilder2D()) {
    }

    /// Releases reusable render state owned by the Wii U 2D render bridge.
    WiiURenderManager2D::~WiiURenderManager2D() {
        if (CommandListBuilder != nullptr) {
            CommandListBuilder->Dispose();
            delete CommandListBuilder;
            CommandListBuilder = nullptr;
        }
    }

    /// Attaches the software surfaces that receive TV and DRC menu output.
    void WiiURenderManager2D::AttachSurface(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface) {
        if (tvSurface == nullptr) {
            throw new ArgumentNullException("tvSurface");
        } else if (drcSurface == nullptr) {
            throw new ArgumentNullException("drcSurface");
        }

        TvSurface = tvSurface;
        DrcSurface = drcSurface;
    }

    /// Rebuilds one platform-owned cooked texture payload into a CPU-readable Wii U runtime texture.
    ::RuntimeTexture* WiiURenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii U cooked texture path is required.", "cookedAssetPath");
        }

        FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
        auto streamGuard = he_cpp_make_scope_exit([&]() {
            if (stream != nullptr) {
                stream->Dispose();
                delete stream;
            }
        });

        Asset* asset = AssetSerializer::Deserialize(stream);
        TextureAsset* textureAsset = he_cpp_try_cast<TextureAsset>(asset);
        if (textureAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii U cooked texture payload did not deserialize into a TextureAsset.");
        }

        auto textureAssetGuard = he_cpp_make_scope_exit([&]() {
            ReleaseTransientTextureAsset(textureAsset);
        });
        return BuildTextureFromRaw(textureAsset);
    }

    /// Rebuilds one shared-engine texture asset into a CPU-readable Wii U runtime texture.
    ::RuntimeTexture* WiiURenderManager2D::BuildTextureFromRaw(::TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0U || data->Height == 0U) {
            throw new InvalidOperationException("Wii U runtime textures require nonzero dimensions.");
        }

        RuntimeTexture* runtimeTexture = new RuntimeTexture();
        runtimeTexture->set_Width(static_cast<int32_t>(data->Width));
        runtimeTexture->set_Height(static_cast<int32_t>(data->Height));
        runtimeTexture->set_IsEngineOwned(data->IsEngineOwned);

        WiiUTexturePixelData pixelData {};
        pixelData.Texture = runtimeTexture;
        pixelData.Width = data->Width;
        pixelData.Height = data->Height;
        pixelData.Pixels = DecodeTexturePixels(data);
        TexturePixelDataRecords.push_back(pixelData);
        return runtimeTexture;
    }

    /// Releases one Wii U runtime texture.
    void WiiURenderManager2D::ReleaseTexture(::RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        TexturePixelDataRecords.erase(
            std::remove_if(
                TexturePixelDataRecords.begin(),
                TexturePixelDataRecords.end(),
                [&](const WiiUTexturePixelData& candidate) {
                    return candidate.Texture == texture;
                }),
            TexturePixelDataRecords.end());

        texture->Dispose();
        delete texture;
    }

    /// Releases one font asset.
    void WiiURenderManager2D::ReleaseFont(::FontAsset* font) {
        if (font == nullptr) {
            throw new ArgumentNullException("font");
        }

        font->Dispose();
        delete font;
    }

    /// Executes one full 2D draw pass into the attached software surfaces.
    void WiiURenderManager2D::Draw() {
        if (TvSurface == nullptr) {
            throw new InvalidOperationException("Wii U TV software surface must be attached before 2D rendering.");
        } else if (DrcSurface == nullptr) {
            throw new InvalidOperationException("Wii U DRC software surface must be attached before 2D rendering.");
        }

        BeginFrame();

        WiiUSoftwareSurface* Surface = TvSurface;
        Surface->Clear(OpaqueBlack);
        DrcSurface->Clear(OpaqueBlack);

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[cameraIndex]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            camera->get_RenderQueue2D()->VisitOrdered(this);
        }

        if (!HasCapturedDrawables()) {
            return;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[cameraIndex]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            RenderCommandList2D* commandList = CommandListBuilder->Build(camera->get_RenderQueue2D());
            if (commandList == nullptr || commandList->get_Count() <= 0) {
                continue;
            }

            ExecuteCommandList(commandList, LogicalFrameWidth, LogicalFrameHeight);
        }
    }

    /// Visits one ordered 2D drawable from the active camera queue.
    void WiiURenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        drawable->Draw();
    }

    /// Clears deferred release state after an engine frame.
    void WiiURenderManager2D::FlushReleasedTextures() {
    }

    /// Accepts a rounded-rectangle draw request without issuing software rasterization yet.
    void WiiURenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
        if (shape == nullptr || shape->get_Parent() == nullptr || !shape->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        SubmitRoundedRect(shape);
    }

    /// Accepts a sprite draw request without issuing software rasterization yet.
    void WiiURenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
        if (sprite == nullptr || sprite->get_Parent() == nullptr || !sprite->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        SubmitSprite(sprite);
    }

    /// Accepts a text draw request without issuing software rasterization yet.
    void WiiURenderManager2D::DrawText(::ITextDrawable2D* text) {
        if (text == nullptr || text->get_Parent() == nullptr || !text->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        SubmitText(text);
    }

    /// Resets the current frame capture state before the next draw pass begins.
    void WiiURenderManager2D::BeginFrame() {
        SpriteQueue.clear();
        TextQueue.clear();
        RoundedRectQueue.clear();
    }

    /// Returns whether the current frame captured any 2D draw requests.
    bool WiiURenderManager2D::HasCapturedDrawables() const {
        return !SpriteQueue.empty() || !TextQueue.empty() || !RoundedRectQueue.empty();
    }

    /// Captures one rounded-rectangle drawable for the current frame.
    void WiiURenderManager2D::SubmitRoundedRect(::IRoundedRectDrawable2D* shape) {
        RoundedRectQueue.push_back(WiiURoundedRectDrawCommand { shape });
    }

    /// Captures one sprite drawable for the current frame.
    void WiiURenderManager2D::SubmitSprite(::ISpriteDrawable2D* sprite) {
        SpriteQueue.push_back(WiiUSpriteDrawCommand { sprite });
    }

    /// Captures one text drawable for the current frame.
    void WiiURenderManager2D::SubmitText(::ITextDrawable2D* text) {
        TextQueue.push_back(WiiUTextDrawCommand { text });
    }

    /// Executes one command list generated from the active camera render queue into the attached software surfaces.
    void WiiURenderManager2D::ExecuteCommandList(RenderCommandList2D* commandList, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        std::vector<float4> clipRectStack;
        float4 currentClipRect = float4(0.0f, 0.0f, static_cast<float>(logicalFrameWidth), static_cast<float>(logicalFrameHeight));

        for (int32_t commandIndex = 0; commandIndex < commandList->get_Count(); commandIndex++) {
            switch (commandList->GetCommandType(commandIndex)) {
                case RenderCommand2DType::ClipPush: {
                    int32_t payloadIndex = commandList->GetClipPushPayloadIndex(commandIndex);
                    currentClipRect = commandList->GetClipPushRect(payloadIndex);
                    clipRectStack.push_back(currentClipRect);
                    break;
                }
                case RenderCommand2DType::ClipPop: {
                    if (!clipRectStack.empty()) {
                        clipRectStack.pop_back();
                    }

                    if (clipRectStack.empty()) {
                        currentClipRect = float4(0.0f, 0.0f, static_cast<float>(logicalFrameWidth), static_cast<float>(logicalFrameHeight));
                    } else {
                        currentClipRect = clipRectStack.back();
                    }
                    break;
                }
                case RenderCommand2DType::TexturedQuad: {
                    int32_t payloadIndex = commandList->GetTexturedQuadPayloadIndex(commandIndex);
                    ExecuteTexturedQuadCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::GlyphQuad: {
                    int32_t payloadIndex = commandList->GetGlyphQuadPayloadIndex(commandIndex);
                    ExecuteGlyphQuadCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::RoundedRect: {
                    int32_t payloadIndex = commandList->GetRoundedRectPayloadIndex(commandIndex);
                    ExecuteRoundedRectCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                default:
                    throw new InvalidOperationException("Wii U 2D rendering received an unsupported command type.");
            }
        }
    }

    /// Executes one textured sprite command from the generated 2D command list.
    void WiiURenderManager2D::ExecuteTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetTexturedQuadBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        RuntimeTexture* runtimeTexture = commandList->GetTexturedQuadTexture(payloadIndex);
        if (runtimeTexture == nullptr) {
            return;
        }

        DrawTexturedQuad2D(
            bounds.X,
            bounds.Y,
            bounds.Z,
            bounds.W,
            commandList->GetTexturedQuadRotation(payloadIndex),
            commandList->GetTexturedQuadSourceRect(payloadIndex),
            commandList->GetTexturedQuadColor(payloadIndex),
            runtimeTexture,
            clipRect,
            logicalFrameWidth,
            logicalFrameHeight);
    }

    /// Executes one text glyph command from the generated 2D command list.
    void WiiURenderManager2D::ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetGlyphQuadBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        RuntimeTexture* runtimeTexture = commandList->GetGlyphQuadTexture(payloadIndex);
        if (runtimeTexture == nullptr) {
            return;
        }

        DrawTexturedQuad2D(
            bounds.X,
            bounds.Y,
            bounds.Z,
            bounds.W,
            0.0f,
            commandList->GetGlyphQuadSourceRect(payloadIndex),
            commandList->GetGlyphQuadColor(payloadIndex),
            runtimeTexture,
            clipRect,
            logicalFrameWidth,
            logicalFrameHeight);
    }

    /// Executes one rounded-rectangle command from the generated 2D command list.
    void WiiURenderManager2D::ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetRoundedRectBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        float borderThickness = commandList->GetRoundedRectBorderThickness(payloadIndex);
        borderThickness = std::max(0.0f, std::min(borderThickness, std::min(bounds.Z, bounds.W) * 0.5f));
        DrawSolidQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, commandList->GetRoundedRectFillColor(payloadIndex), clipRect, logicalFrameWidth, logicalFrameHeight);
        if (borderThickness <= 0.0f) {
            return;
        }

        byte4 borderColor = commandList->GetRoundedRectBorderColor(payloadIndex);
        DrawSolidQuad2D(bounds.X, bounds.Y, bounds.Z, borderThickness, borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        DrawSolidQuad2D(bounds.X, bounds.Y + bounds.W - borderThickness, bounds.Z, borderThickness, borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        DrawSolidQuad2D(bounds.X, bounds.Y + borderThickness, borderThickness, std::max(0.0f, bounds.W - (borderThickness * 2.0f)), borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        DrawSolidQuad2D(bounds.X + bounds.Z - borderThickness, bounds.Y + borderThickness, borderThickness, std::max(0.0f, bounds.W - (borderThickness * 2.0f)), borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
    }

    /// Draws one solid logical-space rectangle into both attached software surfaces.
    void WiiURenderManager2D::DrawSolidQuad2D(float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        DrawSolidQuadToSurface(TvSurface, x, y, width, height, color, clipRect, logicalFrameWidth, logicalFrameHeight);
        DrawSolidQuadToSurface(DrcSurface, x, y, width, height, color, clipRect, logicalFrameWidth, logicalFrameHeight);
    }

    /// Draws one textured logical-space rectangle into both attached software surfaces.
    void WiiURenderManager2D::DrawTexturedQuad2D(float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, RuntimeTexture* texture, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        WiiUTexturePixelData* texturePixelData = FindTexturePixelData(texture);
        if (texturePixelData == nullptr || texturePixelData->Pixels.empty()) {
            return;
        }

        DrawTexturedQuadToSurface(TvSurface, x, y, width, height, rotationRadians, sourceRect, color, *texturePixelData, clipRect, logicalFrameWidth, logicalFrameHeight);
        DrawTexturedQuadToSurface(DrcSurface, x, y, width, height, rotationRadians, sourceRect, color, *texturePixelData, clipRect, logicalFrameWidth, logicalFrameHeight);
    }

    /// Returns one packed texture record for the supplied runtime texture or null when it is unknown to the renderer.
    WiiUTexturePixelData* WiiURenderManager2D::FindTexturePixelData(RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        for (std::size_t index = 0; index < TexturePixelDataRecords.size(); index++) {
            if (TexturePixelDataRecords[index].Texture == texture) {
                return &TexturePixelDataRecords[index];
            }
        }

        return nullptr;
    }

    /// Builds one packed ARGB8888 pixel buffer from one shared-engine texture asset payload.
    std::vector<std::uint32_t> WiiURenderManager2D::DecodeTexturePixels(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0U || data->Height == 0U) {
            throw new InvalidOperationException("Wii U texture decoding requires nonzero dimensions.");
        } else if (data->Colors == nullptr) {
            throw new InvalidOperationException("Wii U texture decoding requires a color payload.");
        }

        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        std::vector<std::uint32_t> pixels(pixelCount, TransparentBlack);

        if (data->ColorFormat == TextureAssetColorFormat::Rgba32) {
            const std::size_t expectedByteCount = pixelCount * 4U;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U RGBA32 textures must contain tightly packed RGBA bytes.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t sourceIndex = pixelIndex * 4U;
                pixels[pixelIndex] = PackArgb(
                    (*data->Colors)[static_cast<int32_t>(sourceIndex + 0U)],
                    (*data->Colors)[static_cast<int32_t>(sourceIndex + 1U)],
                    (*data->Colors)[static_cast<int32_t>(sourceIndex + 2U)],
                    (*data->Colors)[static_cast<int32_t>(sourceIndex + 3U)]);
            }

            return pixels;
        }

        if (data->ColorFormat == TextureAssetColorFormat::Rgba4444) {
            const std::size_t expectedByteCount = pixelCount * 2U;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U RGBA4444 textures must contain tightly packed 16-bit texels.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t sourceIndex = pixelIndex * 2U;
                const std::uint16_t packedPixel =
                    static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(sourceIndex + 0U)])
                    | (static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(sourceIndex + 1U)]) << 8U);
                pixels[pixelIndex] = PackArgb(
                    Expand4To8(static_cast<std::uint8_t>(packedPixel & 0x0FU)),
                    Expand4To8(static_cast<std::uint8_t>((packedPixel >> 4U) & 0x0FU)),
                    Expand4To8(static_cast<std::uint8_t>((packedPixel >> 8U) & 0x0FU)),
                    Expand4To8(static_cast<std::uint8_t>((packedPixel >> 12U) & 0x0FU)));
            }

            return pixels;
        }

        if (data->ColorFormat == TextureAssetColorFormat::Indexed4 || data->ColorFormat == TextureAssetColorFormat::Indexed8) {
            if (data->PaletteColors == nullptr) {
                throw new InvalidOperationException("Wii U indexed textures require a palette payload.");
            }

            const std::size_t paletteByteCount = static_cast<std::size_t>(data->PaletteColors->get_Length());
            if ((paletteByteCount % 4U) != 0U) {
                throw new InvalidOperationException("Wii U indexed texture palettes must contain RGBA entries.");
            }

            const std::size_t paletteEntryCount = paletteByteCount / 4U;
            const std::size_t expectedByteCount = data->ColorFormat == TextureAssetColorFormat::Indexed4
                ? ((pixelCount + 1U) / 2U)
                : pixelCount;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U indexed textures contained an unexpected texel payload length.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                std::size_t paletteIndex = 0U;
                if (data->ColorFormat == TextureAssetColorFormat::Indexed8) {
                    paletteIndex = static_cast<std::size_t>((*data->Colors)[static_cast<int32_t>(pixelIndex)]);
                } else {
                    const std::uint8_t packedIndex = (*data->Colors)[static_cast<int32_t>(pixelIndex / 2U)];
                    paletteIndex = (pixelIndex & 1U) == 0U
                        ? static_cast<std::size_t>(packedIndex & 0x0FU)
                        : static_cast<std::size_t>((packedIndex >> 4U) & 0x0FU);
                }

                if (paletteIndex >= paletteEntryCount) {
                    throw new InvalidOperationException("Wii U indexed texture referenced a palette entry that does not exist.");
                }

                const std::size_t paletteByteIndex = paletteIndex * 4U;
                pixels[pixelIndex] = PackArgb(
                    (*data->PaletteColors)[static_cast<int32_t>(paletteByteIndex + 0U)],
                    (*data->PaletteColors)[static_cast<int32_t>(paletteByteIndex + 1U)],
                    (*data->PaletteColors)[static_cast<int32_t>(paletteByteIndex + 2U)],
                    (*data->PaletteColors)[static_cast<int32_t>(paletteByteIndex + 3U)]);
            }

            return pixels;
        }

        if (data->ColorFormat == TextureAssetColorFormat::GxRgb5A3) {
            const std::uint32_t paddedWidth = (static_cast<std::uint32_t>(data->Width) + 3U) & ~3U;
            const std::uint32_t paddedHeight = (static_cast<std::uint32_t>(data->Height) + 3U) & ~3U;
            const std::size_t expectedByteCount = static_cast<std::size_t>(paddedWidth) * static_cast<std::size_t>(paddedHeight) * 2U;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U GX RGB5A3 textures must contain padded tiled texel bytes.");
            }

            std::size_t sourceByteIndex = 0U;
            for (std::uint32_t blockY = 0U; blockY < paddedHeight; blockY += 4U) {
                for (std::uint32_t blockX = 0U; blockX < paddedWidth; blockX += 4U) {
                    for (std::uint32_t innerY = 0U; innerY < 4U; innerY++) {
                        for (std::uint32_t innerX = 0U; innerX < 4U; innerX++) {
                            const std::uint16_t packedPixel =
                                static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(sourceByteIndex + 0U)])
                                | (static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(sourceByteIndex + 1U)]) << 8U);
                            sourceByteIndex += 2U;

                            const std::uint32_t sampleX = blockX + innerX;
                            const std::uint32_t sampleY = blockY + innerY;
                            if (sampleX >= data->Width || sampleY >= data->Height) {
                                continue;
                            }

                            const std::size_t pixelIndex = static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(data->Width) + sampleX;
                            pixels[pixelIndex] = DecodeRgb5A3(packedPixel);
                        }
                    }
                }
            }

            return pixels;
        }

        throw new InvalidOperationException("Wii U runtime textures received an unsupported color format.");
    }

    /// Draws one solid logical-space rectangle into one target software surface.
    void WiiURenderManager2D::DrawSolidQuadToSurface(WiiUSoftwareSurface* surface, float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (surface == nullptr || width <= 0.0f || height <= 0.0f || logicalFrameWidth == 0U || logicalFrameHeight == 0U) {
            return;
        }

        const double clippedLeft = std::max(static_cast<double>(x), static_cast<double>(clipRect.X));
        const double clippedTop = std::max(static_cast<double>(y), static_cast<double>(clipRect.Y));
        const double clippedRight = std::min(static_cast<double>(x) + static_cast<double>(width), static_cast<double>(clipRect.X) + static_cast<double>(clipRect.Z));
        const double clippedBottom = std::min(static_cast<double>(y) + static_cast<double>(height), static_cast<double>(clipRect.Y) + static_cast<double>(clipRect.W));
        if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) {
            return;
        }

        const double surfaceScaleX = static_cast<double>(surface->GetWidth()) / static_cast<double>(logicalFrameWidth);
        const double surfaceScaleY = static_cast<double>(surface->GetHeight()) / static_cast<double>(logicalFrameHeight);
        const int startX = std::max(0, static_cast<int>(std::floor(clippedLeft * surfaceScaleX)));
        const int startY = std::max(0, static_cast<int>(std::floor(clippedTop * surfaceScaleY)));
        const int endX = std::min(static_cast<int>(surface->GetWidth()), static_cast<int>(std::ceil(clippedRight * surfaceScaleX)));
        const int endY = std::min(static_cast<int>(surface->GetHeight()), static_cast<int>(std::ceil(clippedBottom * surfaceScaleY)));
        const std::uint32_t argbColor = PackArgb(color.X, color.Y, color.Z, color.W);

        for (int surfaceY = startY; surfaceY < endY; surfaceY++) {
            for (int surfaceX = startX; surfaceX < endX; surfaceX++) {
                surface->BlendPixel(surfaceX, surfaceY, argbColor);
            }
        }
    }

    /// Draws one textured logical-space rectangle into one target software surface.
    void WiiURenderManager2D::DrawTexturedQuadToSurface(WiiUSoftwareSurface* surface, float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, const WiiUTexturePixelData& texturePixelData, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (surface == nullptr || width <= 0.0f || height <= 0.0f || logicalFrameWidth == 0U || logicalFrameHeight == 0U || texturePixelData.Width == 0U || texturePixelData.Height == 0U || texturePixelData.Pixels.empty()) {
            return;
        }

        const double clippedLeft = std::max(static_cast<double>(x), static_cast<double>(clipRect.X));
        const double clippedTop = std::max(static_cast<double>(y), static_cast<double>(clipRect.Y));
        const double clippedRight = std::min(static_cast<double>(x) + static_cast<double>(width), static_cast<double>(clipRect.X) + static_cast<double>(clipRect.Z));
        const double clippedBottom = std::min(static_cast<double>(y) + static_cast<double>(height), static_cast<double>(clipRect.Y) + static_cast<double>(clipRect.W));
        if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) {
            return;
        }

        static_cast<void>(rotationRadians);

        const double surfaceScaleX = static_cast<double>(surface->GetWidth()) / static_cast<double>(logicalFrameWidth);
        const double surfaceScaleY = static_cast<double>(surface->GetHeight()) / static_cast<double>(logicalFrameHeight);
        const double scaledX = static_cast<double>(x) * surfaceScaleX;
        const double scaledY = static_cast<double>(y) * surfaceScaleY;
        const double scaledWidth = static_cast<double>(width) * surfaceScaleX;
        const double scaledHeight = static_cast<double>(height) * surfaceScaleY;
        if (scaledWidth <= 0.0 || scaledHeight <= 0.0) {
            return;
        }

        const int startX = std::max(0, static_cast<int>(std::floor(clippedLeft * surfaceScaleX)));
        const int startY = std::max(0, static_cast<int>(std::floor(clippedTop * surfaceScaleY)));
        const int endX = std::min(static_cast<int>(surface->GetWidth()), static_cast<int>(std::ceil(clippedRight * surfaceScaleX)));
        const int endY = std::min(static_cast<int>(surface->GetHeight()), static_cast<int>(std::ceil(clippedBottom * surfaceScaleY)));

        for (int surfaceY = startY; surfaceY < endY; surfaceY++) {
            for (int surfaceX = startX; surfaceX < endX; surfaceX++) {
                const double normalizedX = ((static_cast<double>(surfaceX) + 0.5) - scaledX) / scaledWidth;
                const double normalizedY = ((static_cast<double>(surfaceY) + 0.5) - scaledY) / scaledHeight;
                if (normalizedX < 0.0 || normalizedX > 1.0 || normalizedY < 0.0 || normalizedY > 1.0) {
                    continue;
                }

                const double sourcePixelX = (static_cast<double>(sourceRect.X) + (normalizedX * static_cast<double>(sourceRect.Z))) * static_cast<double>(texturePixelData.Width);
                const double sourcePixelY = (static_cast<double>(sourceRect.Y) + (normalizedY * static_cast<double>(sourceRect.W))) * static_cast<double>(texturePixelData.Height);
                const std::uint32_t sampleX = static_cast<std::uint32_t>(std::clamp(static_cast<int32_t>(sourcePixelX), 0, static_cast<int32_t>(texturePixelData.Width) - 1));
                const std::uint32_t sampleY = static_cast<std::uint32_t>(std::clamp(static_cast<int32_t>(sourcePixelY), 0, static_cast<int32_t>(texturePixelData.Height) - 1));
                const std::size_t sampleIndex = static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(texturePixelData.Width) + sampleX;
                surface->BlendPixel(surfaceX, surfaceY, ApplyTint(texturePixelData.Pixels[sampleIndex], color));
            }
        }
    }

    /// Packs one 8-bit RGBA color into the ARGB8888 layout used by the software surfaces.
    std::uint32_t WiiURenderManager2D::PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        return (static_cast<std::uint32_t>(alpha) << 24U)
            | (static_cast<std::uint32_t>(red) << 16U)
            | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
    }

    /// Applies one byte4 tint to one packed ARGB8888 source pixel.
    std::uint32_t WiiURenderManager2D::ApplyTint(std::uint32_t argbColor, byte4 color) {
        const std::uint32_t sourceAlpha = (argbColor >> 24U) & 0xFFU;
        const std::uint32_t sourceRed = (argbColor >> 16U) & 0xFFU;
        const std::uint32_t sourceGreen = (argbColor >> 8U) & 0xFFU;
        const std::uint32_t sourceBlue = argbColor & 0xFFU;
        const std::uint8_t tintedAlpha = static_cast<std::uint8_t>((sourceAlpha * color.W) / 0xFFU);
        const std::uint8_t tintedRed = static_cast<std::uint8_t>((sourceRed * color.X) / 0xFFU);
        const std::uint8_t tintedGreen = static_cast<std::uint8_t>((sourceGreen * color.Y) / 0xFFU);
        const std::uint8_t tintedBlue = static_cast<std::uint8_t>((sourceBlue * color.Z) / 0xFFU);
        return PackArgb(tintedRed, tintedGreen, tintedBlue, tintedAlpha);
    }

    /// Expands one 4-bit color channel into 8-bit precision.
    std::uint8_t WiiURenderManager2D::Expand4To8(std::uint8_t value) {
        return static_cast<std::uint8_t>((value << 4U) | value);
    }

    /// Expands one 5-bit color channel into 8-bit precision.
    std::uint8_t WiiURenderManager2D::Expand5To8(std::uint16_t value) {
        return static_cast<std::uint8_t>((value * 255U + 15U) / 31U);
    }

    /// Expands one 3-bit alpha channel into 8-bit precision.
    std::uint8_t WiiURenderManager2D::Expand3To8(std::uint16_t value) {
        return static_cast<std::uint8_t>((value * 255U + 3U) / 7U);
    }

    /// Decodes one packed GX RGB5A3 texel into ARGB8888.
    std::uint32_t WiiURenderManager2D::DecodeRgb5A3(std::uint16_t pixel) {
        if ((pixel & 0x8000U) != 0U) {
            return PackArgb(
                Expand5To8((pixel >> 10U) & 0x1FU),
                Expand5To8((pixel >> 5U) & 0x1FU),
                Expand5To8(pixel & 0x1FU),
                0xFFU);
        }

        return PackArgb(
            Expand4To8(static_cast<std::uint8_t>((pixel >> 8U) & 0x0FU)),
            Expand4To8(static_cast<std::uint8_t>((pixel >> 4U) & 0x0FU)),
            Expand4To8(static_cast<std::uint8_t>(pixel & 0x0FU)),
            Expand3To8((pixel >> 12U) & 0x07U));
    }

    /// Releases one transient cooked texture asset after the runtime texture has been rebuilt from its payload.
    void WiiURenderManager2D::ReleaseTransientTextureAsset(TextureAsset* asset) {
        if (asset == nullptr) {
            return;
        }

        Array<uint8_t>* colors = asset->Colors;
        Array<uint8_t>* paletteColors = asset->PaletteColors;
        asset->Colors = nullptr;
        asset->PaletteColors = nullptr;
        if (colors != nullptr && colors != Array<uint8_t>::Empty()) {
            delete colors;
        }

        if (paletteColors != nullptr && paletteColors != Array<uint8_t>::Empty()) {
            delete paletteColors;
        }
        delete asset;
    }
}

#endif
