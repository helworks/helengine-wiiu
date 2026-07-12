#include "platform/wiiu/WiiURenderManager3D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "ContentManager.hpp"
#include "IContentStreamSource.hpp"
#include "ModelAsset.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeModel.hpp"
#include "RuntimeSubmesh.hpp"
#include "float3.hpp"
#include "runtime/array.hpp"

namespace helengine::wiiu {
    namespace {
        class WiiUPlaceholderRuntimeModel final : public ::RuntimeModel {
        public:
            WiiUPlaceholderRuntimeModel()
                : ::RuntimeModel() {
            }
        };

        ::RuntimeMaterial* CreatePlaceholderMaterial() {
            return new ::RuntimeMaterial();
        }

        ::RuntimeModel* CreatePlaceholderModel() {
            WiiUPlaceholderRuntimeModel* model = new WiiUPlaceholderRuntimeModel();
            model->SetBounds(float3::get_Zero(), float3::get_Zero());

            Array<::RuntimeSubmesh*>* submeshes = new Array<::RuntimeSubmesh*>(1);
            (*submeshes)[0] = new ::RuntimeSubmesh();
            model->SetSubmeshes(submeshes);
            delete submeshes;
            return model;
        }
    }

    WiiURenderManager3D::WiiURenderManager3D()
        : ::RenderManager3D() {
    }

    WiiURenderManager3D::~WiiURenderManager3D() = default;

    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) {
        (void)materialAsset;
        return CreatePlaceholderMaterial();
    }

    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
        (void)cookedAssetPath;
        (void)contentStreamSource;
        return CreatePlaceholderMaterial();
    }

    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath) {
        (void)assetContentManager;
        (void)materialAssetPath;
        return CreatePlaceholderMaterial();
    }

    ::RuntimeModel* WiiURenderManager3D::BuildModelFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
        (void)cookedAssetPath;
        (void)contentStreamSource;
        return CreatePlaceholderModel();
    }

    ::RuntimeModel* WiiURenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        (void)data;
        return CreatePlaceholderModel();
    }

    void WiiURenderManager3D::Draw() {
    }
}

#endif
