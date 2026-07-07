#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "RenderManager3D.hpp"
#include "platform/wiiu/WiiUGx23DRenderFrame.hpp"

class CameraComponent;
class IDrawable3D;
class IContentStreamSource;
class RenderFrame;

namespace helengine::wiiu {
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

        /// Builds one placeholder runtime material from a cooked platform material asset record.
        ::RuntimeMaterial* BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) override;

        /// Builds one placeholder runtime material from a cooked Wii U material asset path using the legacy path-based generated-core contract.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath);

        /// Builds one placeholder runtime material from a cooked Wii U material asset path using the current content-stream-based generated-core contract.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource);

        /// Builds one placeholder runtime material from a raw authored material path using the legacy generated-core contract that still passes a content root path.
        ::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath);

        /// Builds one placeholder runtime material from a raw authored material path using the current generated-core contract.
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

    private:
        /// Resets the current frame before capture begins.
        void BeginFrame();

        /// Captures one extracted render frame into the Wii U frame contract.
        void CaptureFrame(RenderFrame* frame, CameraComponent* camera);

        /// Captures one extracted drawable submission into the current frame when its runtime model is Wii U-owned.
        void CaptureDrawCommand(IDrawable3D* drawable);

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

        /// Creates one placeholder runtime material that lets cooked scene loading proceed.
        ::RuntimeMaterial* CreatePlaceholderRuntimeMaterial(std::string runtimeMaterialId);

        /// Builds one Wii U runtime model from a shared model asset payload.
        WiiURuntimeModel* BuildRuntimeModelFromAsset(::ModelAsset* data);

        /// Releases one transient cooked model asset after the runtime geometry has been copied out.
        static void ReleaseTransientModelAsset(::ModelAsset* asset);

        /// Stores the most recently captured 3D frame.
        WiiUGx23DRenderFrame CurrentFrame;
    };
}

#endif
