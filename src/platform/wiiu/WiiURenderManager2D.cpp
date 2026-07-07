#include "platform/wiiu/WiiURenderManager2D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/utils.h>
#include <gx2r/resource.h>
#include <gx2r/surface.h>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "ICamera.hpp"
#include "IContentStreamSource.hpp"
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
        constexpr GX2RResourceFlags NoGx2rResourceFlags = static_cast<GX2RResourceFlags>(0);
        constexpr GX2RResourceFlags TextureSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        constexpr std::uint32_t SolidWhitePixel = 0xFFFFFFFFU;
    }

    /// Creates the Wii U 2D render bridge.
    WiiURenderManager2D::WiiURenderManager2D()
        : RenderManager2D()
        , SpriteQueue()
        , TextQueue()
        , RoundedRectQueue()
        , TexturePixelDataRecords()
        , CommandListBuilder(new RenderCommandListBuilder2D())
        , CurrentFrame()
        , SolidWhiteTextureHandle() {
        InitializeTextureHandle(&SolidWhiteTextureHandle, 1U, 1U, std::vector<std::uint32_t> { SolidWhitePixel });
    }

    /// Releases reusable render state owned by the Wii U 2D render bridge.
    WiiURenderManager2D::~WiiURenderManager2D() {
        for (std::size_t index = 0; index < TexturePixelDataRecords.size(); index++) {
            DestroyTextureHandle(&TexturePixelDataRecords[index].Gx2TextureHandle);
        }

        DestroyTextureHandle(&SolidWhiteTextureHandle);

        if (CommandListBuilder != nullptr) {
            CommandListBuilder->Dispose();
            delete CommandListBuilder;
            CommandListBuilder = nullptr;
        }
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

    /// Rebuilds one platform-owned cooked texture payload into a CPU-readable Wii U runtime texture through the content-stream-based generated-core contract.
    ::RuntimeTexture* WiiURenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
        if (contentStreamSource == nullptr) {
            throw new ArgumentNullException("contentStreamSource");
        }

        ::Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);
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

    /// Rebuilds one shared-engine texture asset into a GX2-backed Wii U runtime texture.
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
        std::vector<std::uint32_t> pixels = DecodeTexturePixels(data);
        InitializeTextureHandle(&pixelData.Gx2TextureHandle, data->Width, data->Height, pixels);
        TexturePixelDataRecords.push_back(pixelData);
        return runtimeTexture;
    }

    /// Releases one Wii U runtime texture.
    void WiiURenderManager2D::ReleaseTexture(::RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        for (std::size_t index = 0; index < TexturePixelDataRecords.size(); index++) {
            if (TexturePixelDataRecords[index].Texture != texture) {
                continue;
            }

            DestroyTextureHandle(&TexturePixelDataRecords[index].Gx2TextureHandle);
            TexturePixelDataRecords.erase(TexturePixelDataRecords.begin() + static_cast<std::ptrdiff_t>(index));
            break;
        }

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

    /// Executes one full 2D draw pass into the current GX2-ready frame.
    void WiiURenderManager2D::Draw() {
        BeginFrame();
        CurrentFrame.Clear();
        CurrentFrame.SetClearColor(WiiUGx2Color { 30U, 17U, 41U, 255U });

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

            CaptureCommandList(commandList, LogicalFrameWidth, LogicalFrameHeight);
        }
    }

    /// Returns the most recently captured GX2-ready frame.
    const WiiUGx2RenderFrame& WiiURenderManager2D::GetCurrentFrame() const {
        return CurrentFrame;
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

    /// Accepts a rounded-rectangle draw request without issuing rasterization yet.
    void WiiURenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
        if (shape == nullptr || shape->get_Parent() == nullptr || !shape->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        SubmitRoundedRect(shape);
    }

    /// Accepts a sprite draw request without issuing rasterization yet.
    void WiiURenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
        if (sprite == nullptr || sprite->get_Parent() == nullptr || !sprite->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        SubmitSprite(sprite);
    }

    /// Accepts a text draw request without issuing rasterization yet.
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

    /// Captures one command list generated from the active camera render queue into the current GX2 frame.
    void WiiURenderManager2D::CaptureCommandList(RenderCommandList2D* commandList, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
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
                    CaptureTexturedQuadCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::GlyphQuad: {
                    int32_t payloadIndex = commandList->GetGlyphQuadPayloadIndex(commandIndex);
                    CaptureGlyphQuadCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::RoundedRect: {
                    int32_t payloadIndex = commandList->GetRoundedRectPayloadIndex(commandIndex);
                    CaptureRoundedRectCommand(commandList, payloadIndex, currentClipRect, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                default:
                    throw new InvalidOperationException("Wii U 2D rendering received an unsupported command type.");
            }
        }
    }

    /// Captures one textured sprite command from the generated 2D command list.
    void WiiURenderManager2D::CaptureTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
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

        CaptureTexturedQuad2D(
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

    /// Captures one text glyph command from the generated 2D command list.
    void WiiURenderManager2D::CaptureGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
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

        CaptureTexturedQuad2D(
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

    /// Captures one rounded-rectangle command from the generated 2D command list.
    void WiiURenderManager2D::CaptureRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, float4 clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetRoundedRectBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        float borderThickness = commandList->GetRoundedRectBorderThickness(payloadIndex);
        borderThickness = std::max(0.0f, std::min(borderThickness, std::min(bounds.Z, bounds.W) * 0.5f));
        CaptureSolidQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, commandList->GetRoundedRectFillColor(payloadIndex), clipRect, logicalFrameWidth, logicalFrameHeight);
        if (borderThickness <= 0.0f) {
            return;
        }

        byte4 borderColor = commandList->GetRoundedRectBorderColor(payloadIndex);
        CaptureSolidQuad2D(bounds.X, bounds.Y, bounds.Z, borderThickness, borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        CaptureSolidQuad2D(bounds.X, bounds.Y + bounds.W - borderThickness, bounds.Z, borderThickness, borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        CaptureSolidQuad2D(bounds.X, bounds.Y + borderThickness, borderThickness, std::max(0.0f, bounds.W - (borderThickness * 2.0f)), borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
        CaptureSolidQuad2D(bounds.X + bounds.Z - borderThickness, bounds.Y + borderThickness, borderThickness, std::max(0.0f, bounds.W - (borderThickness * 2.0f)), borderColor, clipRect, logicalFrameWidth, logicalFrameHeight);
    }

    /// Captures one solid logical-space rectangle into the current GX2 frame.
    void WiiURenderManager2D::CaptureSolidQuad2D(float x, float y, float width, float height, byte4 color, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (width <= 0.0f || height <= 0.0f || logicalFrameWidth == 0U || logicalFrameHeight == 0U) {
            return;
        }

        WiiUGx2QuadCommand command {};
        command.X = x;
        command.Y = y;
        command.Width = width;
        command.Height = height;
        command.RotationRadians = 0.0f;
        command.SourceX = 0.0f;
        command.SourceY = 0.0f;
        command.SourceWidth = 1.0f;
        command.SourceHeight = 1.0f;
        command.Color = WiiUGx2Color { color.X, color.Y, color.Z, color.W };
        command.ClipRect = WiiUGx2ClipRect { clipRect.X, clipRect.Y, clipRect.Z, clipRect.W };
        command.TextureHandle = &SolidWhiteTextureHandle;
        CurrentFrame.AddQuad(command);
    }

    /// Captures one textured logical-space rectangle into the current GX2 frame.
    void WiiURenderManager2D::CaptureTexturedQuad2D(float x, float y, float width, float height, float rotationRadians, const float4& sourceRect, byte4 color, RuntimeTexture* texture, const float4& clipRect, std::uint16_t logicalFrameWidth, std::uint16_t logicalFrameHeight) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        } else if (width <= 0.0f || height <= 0.0f || logicalFrameWidth == 0U || logicalFrameHeight == 0U) {
            return;
        }

        WiiUTexturePixelData* texturePixelData = FindTexturePixelData(texture);
        if (texturePixelData == nullptr) {
            return;
        }

        WiiUGx2QuadCommand command {};
        command.X = x;
        command.Y = y;
        command.Width = width;
        command.Height = height;
        command.RotationRadians = rotationRadians;
        command.SourceX = sourceRect.X;
        command.SourceY = sourceRect.Y;
        command.SourceWidth = sourceRect.Z;
        command.SourceHeight = sourceRect.W;
        command.Color = WiiUGx2Color { color.X, color.Y, color.Z, color.W };
        command.ClipRect = WiiUGx2ClipRect { clipRect.X, clipRect.Y, clipRect.Z, clipRect.W };
        command.TextureHandle = &texturePixelData->Gx2TextureHandle;
        CurrentFrame.AddQuad(command);
    }

    /// Returns one GX2 texture record for the supplied runtime texture or null when it is unknown to the renderer.
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

    /// Initializes one GX2 texture handle from decoded ARGB8888 pixels.
    void WiiURenderManager2D::InitializeTextureHandle(WiiUGx2TextureHandle* textureHandle, std::uint32_t width, std::uint32_t height, const std::vector<std::uint32_t>& pixels) {
        if (textureHandle == nullptr) {
            throw new ArgumentNullException("textureHandle");
        } else if (width == 0U || height == 0U) {
            throw new InvalidOperationException("Wii U GX2 textures require nonzero dimensions.");
        } else if (pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            throw new InvalidOperationException("Wii U GX2 texture upload requires one ARGB pixel per texture texel.");
        }

        std::memset(&textureHandle->Texture, 0, sizeof(textureHandle->Texture));
        std::memset(&textureHandle->Sampler, 0, sizeof(textureHandle->Sampler));

        textureHandle->Texture.surface.use = GX2_SURFACE_USE_TEXTURE;
        textureHandle->Texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        textureHandle->Texture.surface.width = width;
        textureHandle->Texture.surface.height = height;
        textureHandle->Texture.surface.depth = 1U;
        textureHandle->Texture.surface.mipLevels = 1U;
        textureHandle->Texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        textureHandle->Texture.surface.aa = GX2_AA_MODE1X;
        textureHandle->Texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        GX2CalcSurfaceSizeAndAlignment(&textureHandle->Texture.surface);
        if (!GX2RCreateSurface(&textureHandle->Texture.surface, TextureSurfaceFlags)) {
            throw new InvalidOperationException("Wii U GX2 texture allocation failed.");
        }

        textureHandle->Texture.viewFirstMip = 0U;
        textureHandle->Texture.viewNumMips = 1U;
        textureHandle->Texture.viewFirstSlice = 0U;
        textureHandle->Texture.viewNumSlices = 1U;
        textureHandle->Texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
        GX2InitTextureRegs(&textureHandle->Texture);
        GX2InitSampler(&textureHandle->Sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

        std::uint32_t* destinationPixels = static_cast<std::uint32_t*>(GX2RLockSurfaceEx(&textureHandle->Texture.surface, 0, NoGx2rResourceFlags));
        if (destinationPixels == nullptr) {
            DestroyTextureHandle(textureHandle);
            throw new InvalidOperationException("Wii U GX2 texture surface lock failed.");
        }

        const std::uint32_t destinationPitch = textureHandle->Texture.surface.pitch;
        for (std::uint32_t row = 0U; row < height; row++) {
            for (std::uint32_t column = 0U; column < width; column++) {
                const std::uint32_t sourcePixel = pixels[(row * width) + column];
                // PackArgb builds 0xAARRGGBB words, but the GX2 R8_G8_B8_A8 upload path needs one 0xRRGGBBAA word so sampled alpha stays aligned with authored transparency.
                destinationPixels[(row * destinationPitch) + column] = (sourcePixel << 8U)
                    | ((sourcePixel >> 24U) & 0x000000FFU);
            }
        }

        GX2RUnlockSurfaceEx(&textureHandle->Texture.surface, 0, NoGx2rResourceFlags);
        GX2RInvalidateSurface(&textureHandle->Texture.surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, textureHandle->Texture.surface.image, textureHandle->Texture.surface.imageSize);
    }

    /// Releases one GX2 texture handle owned by the Wii U 2D bridge.
    void WiiURenderManager2D::DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle) {
        if (textureHandle == nullptr) {
            return;
        }

        if (textureHandle->Texture.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&textureHandle->Texture.surface, NoGx2rResourceFlags);
        }

        std::memset(&textureHandle->Texture, 0, sizeof(textureHandle->Texture));
        std::memset(&textureHandle->Sampler, 0, sizeof(textureHandle->Sampler));
    }

    /// Packs one 8-bit RGBA color into the ARGB8888 layout used by texture decode.
    std::uint32_t WiiURenderManager2D::PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        return (static_cast<std::uint32_t>(alpha) << 24U)
            | (static_cast<std::uint32_t>(red) << 16U)
            | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
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
    }
}

#endif
