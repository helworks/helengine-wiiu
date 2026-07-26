#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include "RenderManager3D.hpp"
#include "platform/wiiu/WiiUGx23DRenderFrame.hpp"
#include "platform/wiiu/WiiUGx2TextureHandle.hpp"

class CameraComponent;
class DirectionalLightComponent;
class IDrawable3D;
class IContentStreamSource;
class LightComponent;
class RenderFrame;
class RenderFrameDrawableSubmission;
class RenderFrameShadowCasterSubmission;
class TextureAsset;

namespace helengine::wiiu {
    class WiiURuntimeMaterial;
    class WiiURuntimeModel;

    /// Provides the Wii U 3D renderer bridge that captures one generic scene-driven frame for the GX2 presenter.
    class WiiURenderManager3D final : public ::RenderManager3D {
    public:
        /// Creates one Wii U 3D bridge with an empty captured frame.
        WiiURenderManager3D();

        /// Releases cached bridge state.
        ~WiiURenderManager3D() override;

        /// Captures the current scene-driven 3D frame from the generated runtime.
        void Draw() override;

        /// Builds one concrete Wii U runtime material from a cooked platform material asset record.
        ::RuntimeMaterial* BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) override;

        /// Builds one concrete Wii U runtime material from a cooked Wii U material asset path using the legacy path-based generated-core contract.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath);

        /// Builds one concrete Wii U runtime material from a cooked Wii U material asset path using the current content-stream-based generated-core contract.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);

        /// Builds one concrete Wii U runtime material from a raw authored material path using the legacy generated-core contract that still passes a content root path.
        ::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath);

        /// Builds one concrete Wii U runtime material from a raw authored material path using the current generated-core contract.
        ::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath);

        /// Builds one placeholder runtime model from a cooked Wii U model asset path using the legacy path-based generated-core contract.
        ::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath);

        /// Builds one placeholder runtime model from a cooked Wii U model asset path using the current content-stream-based generated-core contract.
        ::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);

        /// Builds one placeholder runtime model from a raw authored model asset.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;

        /// Returns the most recently captured scene-driven 3D frame.
        const WiiUGx23DRenderFrame& GetCurrentFrame() const;

        /// Releases one runtime model built by the Wii U bridge.
        void ReleaseModel(::RuntimeModel* model) override;

        /// Releases one runtime material built by the Wii U bridge.
        void ReleaseMaterial(::RuntimeMaterial* material) override;

    private:
        /// Resets the current frame before capture begins.
        void BeginFrame();

        /// Captures one extracted render frame into the Wii U frame contract.
        void CaptureFrame(RenderFrame* frame, CameraComponent* camera);

        /// Captures the current scene ambient and directional light state into the Wii U frame contract.
        void CaptureSceneLighting();

        /// Captures one extracted drawable submission into the current frame when its runtime model and runtime material are Wii U-owned.
        void CaptureDrawCommand(RenderFrameDrawableSubmission* submission);

        /// Captures one shared shadow-caster submission into the current Wii U directional-shadow frame state.
        void CaptureShadowCasterCommand(RenderFrameShadowCasterSubmission* submission, WiiUGx23DDirectionalShadowState& directionalShadowState);

        /// Resolves the primary runtime camera for the current frame.
        bool TryResolvePrimaryCamera(CameraComponent*& camera) const;

        /// Converts one runtime camera clear color into the 8-bit GX2 color used by the presenter.
        static WiiUGx2Color ConvertClearColor(float4 clearColor);

        /// Converts one normalized float color channel into one 8-bit color channel.
        static std::uint8_t ConvertColorChannel(float value);

        /// Builds one view matrix from the active runtime camera transform.
        static float4x4 CreateViewMatrix(CameraComponent* camera);

        /// Builds one camera state record for the current frame.
        static WiiUGx23DCameraState CreateCameraState(CameraComponent* camera);

        /// Builds one normalized float color from one cooked 8-bit base-color payload.
        static float4 CreateBaseColor(::PlatformMaterialAsset* materialAsset);

        /// Converts one linear light color plus intensity into one packed float4 radiance color.
        static float4 CreateLightColor(::LightComponent* light);

        /// Builds one directional-light capture record from one scene directional light.
        static WiiUGx23DDirectionalLightState CreateDirectionalLightState(::DirectionalLightComponent* light);

        /// Builds the camera-fitted directional shadow transform used by the shared StandardShader.
        static float4x4 CreateDirectionalShadowViewProjection(CameraComponent* camera, const WiiUGx23DDirectionalLightState& directionalLightState);

        /// Creates one concrete Wii U runtime material from the supplied material fields.
        WiiURuntimeMaterial* CreateRuntimeMaterial(std::string runtimeMaterialId, float4 baseColor, bool isLit, bool isDoubleSided);

        /// Builds one GX2 texture handle from a cooked runtime texture payload path.
        static WiiUGx2TextureHandle BuildTextureHandleFromCooked(std::string cookedAssetPath);

        /// Builds one GX2 texture handle from a cooked runtime texture payload path through the content-stream contract.
        static WiiUGx2TextureHandle BuildTextureHandleFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);

        /// Builds one GX2 texture handle from one shared-engine texture payload.
        static WiiUGx2TextureHandle BuildTextureHandleFromRaw(::TextureAsset* data);

        /// Builds one Wii U runtime model from a shared model asset payload.
        WiiURuntimeModel* BuildRuntimeModelFromAsset(::ModelAsset* data);

        /// Initializes one GX2 texture handle from decoded ARGB pixels.
        static void InitializeTextureHandle(WiiUGx2TextureHandle* textureHandle, std::uint32_t width, std::uint32_t height, const std::vector<std::uint32_t>& pixels);

        /// Releases one GX2 texture handle owned by one Wii U runtime material.
        static void DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle);

        /// Decodes one shared-engine texture payload into ARGB texels ready for GX2 upload.
        static std::vector<std::uint32_t> DecodeTexturePixels(::TextureAsset* data);

        /// Packs one 8-bit RGBA color into one ARGB8888 word.
        static std::uint32_t PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);

        /// Expands one 4-bit color channel into 8-bit precision.
        static std::uint8_t Expand4To8(std::uint8_t value);

        /// Expands one 5-bit color channel into 8-bit precision.
        static std::uint8_t Expand5To8(std::uint16_t value);

        /// Expands one 3-bit alpha channel into 8-bit precision.
        static std::uint8_t Expand3To8(std::uint16_t value);

        /// Decodes one packed GX RGB5A3 texel into ARGB8888.
        static std::uint32_t DecodeRgb5A3(std::uint16_t pixel);

        /// Releases one transient cooked model asset after the runtime geometry has been copied out.
        static void ReleaseTransientModelAsset(::ModelAsset* asset);

        /// Releases one transient cooked texture asset after one runtime GX2 texture handle has been rebuilt from its payload.
        static void ReleaseTransientTextureAsset(::TextureAsset* asset);

        /// Stores the most recently captured 3D frame.
        WiiUGx23DRenderFrame CurrentFrame;
    };
}

#endif
