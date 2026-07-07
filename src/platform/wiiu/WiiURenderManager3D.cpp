#include "platform/wiiu/WiiURenderManager3D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "IContentStreamSource.hpp"
#include "ModelAsset.hpp"
#include "RuntimeMaterial.hpp"
#include "runtime/finally.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_string.hpp"
#include "system/io/file.hpp"
#include "platform/wiiu/WiiURuntimeModel.hpp"
#include <coreinit/debug.h>

namespace helengine::wiiu {
    /// Creates one Wii U 3D bridge with no cached runtime model.
    WiiURenderManager3D::WiiURenderManager3D()
        : RenderManager3D()
        , LatestRuntimeModel(nullptr) {
    }

    /// Releases cached bridge state.
    WiiURenderManager3D::~WiiURenderManager3D() {
        LatestRuntimeModel = nullptr;
    }

    /// Builds one placeholder runtime material from a cooked platform material asset record.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        return CreatePlaceholderRuntimeMaterial("wiiu:material");
    }

    /// Builds one placeholder runtime material from a cooked Wii U material asset path.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (String::IsNullOrWhiteSpace(cookedAssetPath)) {
            throw new ArgumentException("Cooked material asset path must be provided.", "cookedAssetPath");
        }

        return CreatePlaceholderRuntimeMaterial(cookedAssetPath);
    }

    /// Builds one placeholder runtime material from a cooked Wii U material asset path through the content-stream-based generated-core contract.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
        if (contentStreamSource == nullptr) {
            throw new ArgumentNullException("contentStreamSource");
        }

        return BuildMaterialFromCooked(cookedAssetPath);
    }

    /// Builds one placeholder runtime material from a raw authored material path through the legacy generated-core contract that still passes a content root path.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw new ArgumentNullException("assetContentManager");
        }

        if (String::IsNullOrWhiteSpace(contentRootPath)) {
            throw new ArgumentException("Content root path must be provided.", "contentRootPath");
        }

        return BuildMaterialFromRawAsset(assetContentManager, materialAssetPath);
    }

    /// Builds one placeholder runtime material from a raw authored material path through the current generated-core contract.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw new ArgumentNullException("assetContentManager");
        }

        if (String::IsNullOrWhiteSpace(materialAssetPath)) {
            throw new ArgumentException("Material asset path must be provided.", "materialAssetPath");
        }

        return CreatePlaceholderRuntimeMaterial(materialAssetPath);
    }

    /// Builds one runtime model from a cooked Wii U model asset path by copying the authored mesh payload into a Wii U-owned geometry container.
    ::RuntimeModel* WiiURenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (String::IsNullOrWhiteSpace(cookedAssetPath)) {
            throw new ArgumentException("Cooked model asset path must be provided.", "cookedAssetPath");
        }

        FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
        auto streamGuard = he_cpp_make_scope_exit([&]() {
            if (stream != nullptr) {
                stream->Dispose();
                delete stream;
            }
        });

        Asset* asset = AssetSerializer::Deserialize(stream);
        ModelAsset* modelAsset = he_cpp_try_cast<ModelAsset>(asset);
        if (modelAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii U cooked model payload did not deserialize into a ModelAsset.");
        }

        auto modelAssetGuard = he_cpp_make_scope_exit([&]() {
            ReleaseTransientModelAsset(modelAsset);
            delete modelAsset;
        });
        return BuildRuntimeModelFromAsset(modelAsset);
    }

    /// Builds one runtime model from a cooked Wii U model asset path through the content-stream-based generated-core contract.
    ::RuntimeModel* WiiURenderManager3D::BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
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
        ModelAsset* modelAsset = he_cpp_try_cast<ModelAsset>(asset);
        if (modelAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii U cooked model payload did not deserialize into a ModelAsset.");
        }

        auto modelAssetGuard = he_cpp_make_scope_exit([&]() {
            ReleaseTransientModelAsset(modelAsset);
            delete modelAsset;
        });
        return BuildRuntimeModelFromAsset(modelAsset);
    }

    /// Builds one runtime model from a raw authored model asset by copying its positions and indices into Wii U-owned geometry storage.
    ::RuntimeModel* WiiURenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        return BuildRuntimeModelFromAsset(data);
    }

    /// Returns the most recently built runtime model captured during scene loading.
    WiiURuntimeModel* WiiURenderManager3D::GetLatestRuntimeModel() const {
        return LatestRuntimeModel;
    }

    /// Releases one runtime model and clears the cached latest-model pointer when it matches.
    void WiiURenderManager3D::ReleaseModel(::RuntimeModel* model) {
        if (model == nullptr) {
            throw new ArgumentNullException("model");
        }

        if (model == LatestRuntimeModel) {
            LatestRuntimeModel = nullptr;
        }

        RenderManager3D::ReleaseModel(model);
    }

    /// Creates one placeholder runtime material that lets cooked scene loading proceed.
    ::RuntimeMaterial* WiiURenderManager3D::CreatePlaceholderRuntimeMaterial(std::string runtimeMaterialId) {
        RuntimeMaterial* runtimeMaterial = new RuntimeMaterial();
        runtimeMaterial->set_Id(runtimeMaterialId);
        return runtimeMaterial;
    }

    /// Builds one Wii U runtime model from a shared model asset payload.
    WiiURuntimeModel* WiiURenderManager3D::BuildRuntimeModelFromAsset(::ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Positions == nullptr || data->Positions->get_Length() <= 0) {
            throw new InvalidOperationException("Wii U runtime model creation requires at least one authored vertex position.");
        }

        std::vector<float> positionData;
        std::vector<std::uint16_t> indexData;
        const int32_t positionCount = data->Positions->get_Length();
        float minX = 0.0f;
        float minY = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float maxZ = 0.0f;
        positionData.reserve(static_cast<std::size_t>(positionCount) * 4U);
        for (int32_t positionIndex = 0; positionIndex < positionCount; positionIndex++) {
            const float3 position = (*data->Positions)[positionIndex];
            if (positionIndex == 0) {
                minX = position.X;
                minY = position.Y;
                minZ = position.Z;
                maxX = position.X;
                maxY = position.Y;
                maxZ = position.Z;
            } else {
                if (position.X < minX) {
                    minX = position.X;
                }
                if (position.Y < minY) {
                    minY = position.Y;
                }
                if (position.Z < minZ) {
                    minZ = position.Z;
                }
                if (position.X > maxX) {
                    maxX = position.X;
                }
                if (position.Y > maxY) {
                    maxY = position.Y;
                }
                if (position.Z > maxZ) {
                    maxZ = position.Z;
                }
            }
            positionData.push_back(position.X);
            positionData.push_back(position.Y);
            positionData.push_back(position.Z);
            positionData.push_back(1.0f);
        }

        if (data->Indices16 != nullptr && data->Indices16->get_Length() > 0) {
            indexData.reserve(static_cast<std::size_t>(data->Indices16->get_Length()));
            for (int32_t index = 0; index < data->Indices16->get_Length(); index++) {
                indexData.push_back((*data->Indices16)[index]);
            }
        } else if (data->Indices32 != nullptr && data->Indices32->get_Length() > 0) {
            indexData.reserve(static_cast<std::size_t>(data->Indices32->get_Length()));
            for (int32_t index = 0; index < data->Indices32->get_Length(); index++) {
                const std::uint32_t sourceIndex = (*data->Indices32)[index];
                if (sourceIndex > 0xFFFFU) {
                    throw new InvalidOperationException("Wii U first cube bring-up only supports 16-bit indexable geometry.");
                }

                indexData.push_back(static_cast<std::uint16_t>(sourceIndex));
            }
        } else {
            indexData.reserve(static_cast<std::size_t>(positionCount));
            for (int32_t index = 0; index < positionCount; index++) {
                indexData.push_back(static_cast<std::uint16_t>(index));
            }
        }

        WiiURuntimeModel* runtimeModel = new WiiURuntimeModel();
        runtimeModel->SetGeometry(std::move(positionData), std::move(indexData));
        OSReport(
            "[WiiUModel] positions=%d indices=%u boundsMin=(%.3f, %.3f, %.3f) boundsMax=(%.3f, %.3f, %.3f)\n",
            positionCount,
            static_cast<unsigned int>(runtimeModel->GetIndexData().size()),
            minX,
            minY,
            minZ,
            maxX,
            maxY,
            maxZ);
        LatestRuntimeModel = runtimeModel;
        return runtimeModel;
    }

    /// Releases one transient cooked model asset after the runtime geometry has been copied out.
    void WiiURenderManager3D::ReleaseTransientModelAsset(::ModelAsset* asset) {
        if (asset == nullptr) {
            return;
        }

        Array<float3>* positions = asset->Positions;
        Array<float3>* normals = asset->Normals;
        Array<float2>* texCoords = asset->TexCoords;
        Array<std::uint16_t>* indices16 = asset->Indices16;
        Array<std::uint32_t>* indices32 = asset->Indices32;
        asset->Positions = nullptr;
        asset->Normals = nullptr;
        asset->TexCoords = nullptr;
        asset->Indices16 = nullptr;
        asset->Indices32 = nullptr;
        if (positions != nullptr && positions != Array<float3>::Empty()) {
            delete positions;
        }

        if (normals != nullptr && normals != Array<float3>::Empty()) {
            delete normals;
        }

        if (texCoords != nullptr && texCoords != Array<float2>::Empty()) {
            delete texCoords;
        }

        if (indices16 != nullptr && indices16 != Array<std::uint16_t>::Empty()) {
            delete indices16;
        }

        if (indices32 != nullptr && indices32 != Array<std::uint32_t>::Empty()) {
            delete indices32;
        }
    }
}

#endif
