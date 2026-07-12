#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "RenderManager3D.hpp"

class ContentManager;
class IContentStreamSource;
class ModelAsset;
class PlatformMaterialAsset;
class RuntimeMaterial;
class RuntimeModel;

namespace helengine::wiiu {
    /// Keeps the Wii U clean host's 3D bridge intentionally skeletal so menu/audio bring-up does not depend on the full GX2 scene renderer.
    class WiiURenderManager3D final : public ::RenderManager3D {
    public:
        /// Creates the minimal Wii U 3D bridge.
        WiiURenderManager3D();

        /// Releases the minimal Wii U 3D bridge.
        ~WiiURenderManager3D() override;

        /// Rebuilds one platform-owned cooked material into a placeholder runtime material.
        ::RuntimeMaterial* BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) override;

        /// Rebuilds one cooked material path into a placeholder runtime material.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) override;

        /// Rebuilds one raw material path into a placeholder runtime material.
        ::RuntimeMaterial* BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath) override;

        /// Rebuilds one cooked model path into a placeholder runtime model.
        ::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) override;

        /// Rebuilds one raw model asset into a placeholder runtime model.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;

        /// Executes one no-op draw pass.
        void Draw() override;
    };
}

#endif
