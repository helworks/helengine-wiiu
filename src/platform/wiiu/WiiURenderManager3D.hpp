#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "RenderManager3D.hpp"

class IContentStreamSource;

namespace helengine::wiiu {
    class WiiURuntimeModel;

    /// Provides the minimum Wii U 3D renderer bridge required for cooked asset resolution and first-mesh geometry capture.
    class WiiURenderManager3D final : public ::RenderManager3D {
    public:
        /// Creates one Wii U 3D bridge with no cached runtime model.
        WiiURenderManager3D();

        /// Releases cached bridge state.
        ~WiiURenderManager3D() override;

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

        /// Returns the most recently built runtime model captured during scene loading.
        WiiURuntimeModel* GetLatestRuntimeModel() const;

        /// Releases one runtime model and clears the cached latest-model pointer when it matches.
        void ReleaseModel(::RuntimeModel* model) override;

    private:
        /// Creates one placeholder runtime material that lets cooked scene loading proceed.
        ::RuntimeMaterial* CreatePlaceholderRuntimeMaterial(std::string runtimeMaterialId);

        /// Builds one Wii U runtime model from a shared model asset payload.
        WiiURuntimeModel* BuildRuntimeModelFromAsset(::ModelAsset* data);

        /// Releases one transient cooked model asset after the runtime geometry has been copied out.
        static void ReleaseTransientModelAsset(::ModelAsset* asset);

        /// Stores the most recently built runtime model captured during scene loading.
        WiiURuntimeModel* LatestRuntimeModel;
    };
}

#endif
