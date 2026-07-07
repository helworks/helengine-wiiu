#include "platform/wiiu/WiiURenderManager3D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "CameraClearSettings.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "IContentStreamSource.hpp"
#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "LightComponent.hpp"
#include "ModelAsset.hpp"
#include "ObjectManager.hpp"
#include "RenderFrame.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameExtractionResult.hpp"
#include "RenderFrameExtractionService.hpp"
#include "RenderFrameLightSubmission.hpp"
#include "RenderFrameShadowCasterSubmission.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "RuntimeMaterial.hpp"
#include "runtime/finally.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_list.hpp"
#include "runtime/native_string.hpp"
#include "system/io/file.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wiiu/WiiURuntimeModel.hpp"
#include <coreinit/debug.h>

namespace helengine::wiiu {
    /// Creates one Wii U 3D bridge with an empty captured frame.
    WiiURenderManager3D::WiiURenderManager3D()
        : RenderManager3D()
        , CurrentFrame() {
    }

    /// Releases cached bridge state.
    WiiURenderManager3D::~WiiURenderManager3D() {
    }

    /// Captures the current scene-driven 3D frame from the generated runtime.
    void WiiURenderManager3D::Draw() {
        BeginFrame();

        CameraComponent* primaryCamera = nullptr;
        if (!TryResolvePrimaryCamera(primaryCamera)) {
            return;
        }

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            throw new InvalidOperationException("Wii U 3D frame capture requires one initialized Core object manager.");
        }

        ObjectManager* objectManager = core->get_ObjectManager();
        if (objectManager->get_Drawables3D() == nullptr) {
            return;
        }

        List<CameraComponent*>* cameras = new List<CameraComponent*>(1);
        cameras->Add(primaryCamera);
        auto cameraListGuard = he_cpp_make_scope_exit([&]() {
            delete cameras;
        });

        List<LightComponent*>* lights = new List<LightComponent*>();
        auto lightListGuard = he_cpp_make_scope_exit([&]() {
            delete lights;
        });

        RendererBackendCapabilityProfile* capabilityProfile = GetCapabilityProfile();
        auto capabilityProfileGuard = he_cpp_make_scope_exit([&]() {
            delete capabilityProfile;
        });

        RenderFrameExtractionService extractionService {};
        RenderFrameExtractionResult* extractionResult = extractionService.Extract(cameras, objectManager->get_Drawables3D(), lights, capabilityProfile);
        auto extractionResultGuard = he_cpp_make_scope_exit([&]() {
            if (extractionResult == nullptr) {
                return;
            }

            List<RenderFrame*>* frames = extractionResult->get_Frames();
            if (frames != nullptr) {
                for (int32_t frameIndex = 0; frameIndex < frames->get_Count(); frameIndex++) {
                    RenderFrame* frame = (*frames).get_Item(frameIndex);
                    if (frame == nullptr) {
                        continue;
                    }

                    List<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
                    if (drawableSubmissions != nullptr) {
                        for (int32_t drawableIndex = 0; drawableIndex < drawableSubmissions->get_Count(); drawableIndex++) {
                            delete (*drawableSubmissions).get_Item(drawableIndex);
                        }

                        delete drawableSubmissions;
                    }

                    List<RenderFrameLightSubmission*>* lightSubmissions = frame->get_LightSubmissions();
                    if (lightSubmissions != nullptr) {
                        for (int32_t lightIndex = 0; lightIndex < lightSubmissions->get_Count(); lightIndex++) {
                            delete (*lightSubmissions).get_Item(lightIndex);
                        }

                        delete lightSubmissions;
                    }

                    List<RenderFrameShadowCasterSubmission*>* shadowCasterSubmissions = frame->get_ShadowCasterSubmissions();
                    if (shadowCasterSubmissions != nullptr) {
                        for (int32_t shadowCasterIndex = 0; shadowCasterIndex < shadowCasterSubmissions->get_Count(); shadowCasterIndex++) {
                            delete (*shadowCasterSubmissions).get_Item(shadowCasterIndex);
                        }

                        delete shadowCasterSubmissions;
                    }

                    delete frame;
                }

                delete frames;
            }

            delete extractionResult;
        });

        if (extractionResult->get_Frames() == nullptr || extractionResult->get_Frames()->get_Count() <= 0) {
            return;
        }

        CaptureFrame((*extractionResult->get_Frames()).get_Item(0), primaryCamera);
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

    /// Returns the most recently captured scene-driven 3D frame.
    const WiiUGx23DRenderFrame& WiiURenderManager3D::GetCurrentFrame() const {
        return CurrentFrame;
    }

    /// Releases one runtime model built by the Wii U bridge.
    void WiiURenderManager3D::ReleaseModel(::RuntimeModel* model) {
        if (model == nullptr) {
            throw new ArgumentNullException("model");
        }

        RenderManager3D::ReleaseModel(model);
    }

    /// Resets the current frame before capture begins.
    void WiiURenderManager3D::BeginFrame() {
        CurrentFrame.Clear();
    }

    /// Captures one extracted render frame into the Wii U frame contract.
    void WiiURenderManager3D::CaptureFrame(RenderFrame* frame, CameraComponent* camera) {
        if (frame == nullptr) {
            throw new ArgumentNullException("frame");
        } else if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        const CameraClearSettings clearSettings = camera->get_ClearSettings();
        if (clearSettings.get_ClearColorEnabled()) {
            CurrentFrame.SetClearColor(ConvertClearColor(clearSettings.get_ClearColor()));
        } else {
            CurrentFrame.SetClearColor(WiiUGx2Color { 0U, 0U, 0U, 255U });
        }

        CurrentFrame.SetCamera(CreateCameraState(camera));

        List<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
        if (drawableSubmissions == nullptr) {
            return;
        }

        for (int32_t index = 0; index < drawableSubmissions->get_Count(); index++) {
            RenderFrameDrawableSubmission* submission = (*drawableSubmissions).get_Item(index);
            if (submission == nullptr || submission->get_Drawable() == nullptr) {
                continue;
            }

            CaptureDrawCommand(submission->get_Drawable());
        }
    }

    /// Captures one extracted drawable submission into the current frame when its runtime model is Wii U-owned.
    void WiiURenderManager3D::CaptureDrawCommand(IDrawable3D* drawable) {
        if (drawable == nullptr) {
            throw new ArgumentNullException("drawable");
        }

        WiiURuntimeModel* runtimeModel = he_cpp_try_cast<WiiURuntimeModel>(drawable->get_Model());
        if (runtimeModel == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every runtime model to be one WiiURuntimeModel.");
        } else if (drawable->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every drawable to have one parent entity.");
        }

        WiiUGx23DDrawCommand drawCommand {};
        drawCommand.RuntimeModel = runtimeModel;
        drawCommand.WorldMatrix = drawable->get_Parent()->get_WorldTransformMatrix();
        CurrentFrame.AddDrawCommand(drawCommand);
    }

    /// Resolves the primary runtime camera for the current frame.
    bool WiiURenderManager3D::TryResolvePrimaryCamera(CameraComponent*& camera) const {
        camera = nullptr;

        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr || core->get_ObjectManager()->get_Cameras() == nullptr) {
            return false;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        for (int32_t index = 0; index < cameras->get_Count(); index++) {
            CameraComponent* runtimeCamera = he_cpp_try_cast<CameraComponent>((*cameras).get_Item(index));
            if (runtimeCamera != nullptr && runtimeCamera->get_Parent() != nullptr) {
                camera = runtimeCamera;
                return true;
            }
        }

        return false;
    }

    /// Converts one runtime camera clear color into the 8-bit GX2 color used by the presenter.
    WiiUGx2Color WiiURenderManager3D::ConvertClearColor(float4 clearColor) {
        return WiiUGx2Color {
            ConvertColorChannel(clearColor.X),
            ConvertColorChannel(clearColor.Y),
            ConvertColorChannel(clearColor.Z),
            ConvertColorChannel(clearColor.W)
        };
    }

    /// Converts one normalized float color channel into one 8-bit color channel.
    std::uint8_t WiiURenderManager3D::ConvertColorChannel(float value) {
        if (value <= 0.0f) {
            return 0U;
        } else if (value >= 1.0f) {
            return 255U;
        }

        return static_cast<std::uint8_t>(value * 255.0f);
    }

    /// Builds one view matrix from the active runtime camera transform.
    float4x4 WiiURenderManager3D::CreateViewMatrix(CameraComponent* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U view-matrix creation requires the camera to be attached to one entity.");
        }

        Entity* cameraEntity = camera->get_Parent();
        float3 cameraPosition = cameraEntity->get_Position();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraEntity->get_Orientation());
        float3 cameraUp = float4::RotateVector(float3::get_UnitY(), cameraEntity->get_Orientation());
        float3 cameraTarget = cameraPosition + cameraForward;
        float4x4 viewMatrix;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(cameraPosition, cameraTarget, cameraUp, viewMatrix);
        return viewMatrix;
    }

    /// Builds one camera state record for the current frame.
    WiiUGx23DCameraState WiiURenderManager3D::CreateCameraState(CameraComponent* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        WiiUGx23DCameraState cameraState {};
        cameraState.ViewMatrix = CreateViewMatrix(camera);
        cameraState.Viewport = camera->get_Viewport();
        cameraState.NearPlaneDistance = camera->get_NearPlaneDistance();
        cameraState.FarPlaneDistance = camera->get_FarPlaneDistance();
        return cameraState;
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
