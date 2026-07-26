#include "platform/wiiu/WiiURenderManager3D.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/utils.h>
#include <gx2r/resource.h>
#include <gx2r/surface.h>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "AmbientLightComponent.hpp"
#include "CameraClearSettings.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "DirectionalLightComponent.hpp"
#include "Entity.hpp"
#include "IContentStreamSource.hpp"
#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "LightComponent.hpp"
#include "ModelAsset.hpp"
#include "ObjectManager.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RenderFrame.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameExtractionResult.hpp"
#include "RenderFrameExtractionService.hpp"
#include "RenderFrameLightSubmission.hpp"
#include "RenderFrameShadowCasterSubmission.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "RuntimeMaterial.hpp"
#include "TextureAsset.hpp"
#include "TextureAssetAlphaPrecision.hpp"
#include "TextureAssetColorFormat.hpp"
#include "runtime/finally.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_list.hpp"
#include "runtime/native_string.hpp"
#include "system/io/file.hpp"
#include "float2.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wiiu/WiiURuntimeMaterial.hpp"
#include "platform/wiiu/WiiURuntimeModel.hpp"
#include <coreinit/debug.h>

namespace helengine::wiiu {
    namespace {
        constexpr GX2RResourceFlags NoGx2rResourceFlags = static_cast<GX2RResourceFlags>(0);
        constexpr GX2RResourceFlags TextureSurfaceFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
    }

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
        if (objectManager->get_AmbientLights() != nullptr) {
            for (int32_t ambientLightIndex = 0; ambientLightIndex < objectManager->get_AmbientLights()->get_Count(); ambientLightIndex++) {
                AmbientLightComponent* ambientLight = (*objectManager->get_AmbientLights()).get_Item(ambientLightIndex);
                if (ambientLight != nullptr) {
                    lights->Add(ambientLight);
                }
            }
        }
        if (objectManager->get_DirectionalLights() != nullptr) {
            for (int32_t directionalLightIndex = 0; directionalLightIndex < objectManager->get_DirectionalLights()->get_Count(); directionalLightIndex++) {
                DirectionalLightComponent* directionalLight = (*objectManager->get_DirectionalLights()).get_Item(directionalLightIndex);
                if (directionalLight != nullptr) {
                    lights->Add(directionalLight);
                }
            }
        }

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

    /// Builds one concrete Wii U runtime material from a cooked platform material asset record.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        WiiURuntimeMaterial* runtimeMaterial = CreateRuntimeMaterial("wiiu:material", CreateBaseColor(materialAsset), materialAsset->Lit, materialAsset->DoubleSided);
        if (!String::IsNullOrWhiteSpace(materialAsset->TextureRelativePath)) {
            WiiUGx2TextureHandle textureHandle = BuildTextureHandleFromCooked(materialAsset->TextureRelativePath);
            runtimeMaterial->SetBaseColorTextureHandle(textureHandle);
        }

        return runtimeMaterial;
    }

    /// Builds one concrete Wii U runtime material from a cooked Wii U material asset path.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (String::IsNullOrWhiteSpace(cookedAssetPath)) {
            throw new ArgumentException("Cooked material asset path must be provided.", "cookedAssetPath");
        }

        FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
        auto streamGuard = he_cpp_make_scope_exit([&]() {
            if (stream != nullptr) {
                stream->Dispose();
                delete stream;
            }
        });

        Asset* asset = AssetSerializer::Deserialize(stream);
        PlatformMaterialAsset* materialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);
        if (materialAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii U cooked material payload did not deserialize into a PlatformMaterialAsset.");
        }

        auto materialAssetGuard = he_cpp_make_scope_exit([&]() {
            delete materialAsset;
        });
        WiiURuntimeMaterial* runtimeMaterial = CreateRuntimeMaterial(cookedAssetPath, CreateBaseColor(materialAsset), materialAsset->Lit, materialAsset->DoubleSided);
        if (!String::IsNullOrWhiteSpace(materialAsset->TextureRelativePath)) {
            WiiUGx2TextureHandle textureHandle = BuildTextureHandleFromCooked(materialAsset->TextureRelativePath);
            runtimeMaterial->SetBaseColorTextureHandle(textureHandle);
        }

        return runtimeMaterial;
    }

    /// Builds one concrete Wii U runtime material from a cooked Wii U material asset path through the content-stream-based generated-core contract.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
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
        PlatformMaterialAsset* materialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);
        if (materialAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii U cooked material payload did not deserialize into a PlatformMaterialAsset.");
        }

        auto materialAssetGuard = he_cpp_make_scope_exit([&]() {
            delete materialAsset;
        });
        WiiURuntimeMaterial* runtimeMaterial = CreateRuntimeMaterial(cookedAssetPath, CreateBaseColor(materialAsset), materialAsset->Lit, materialAsset->DoubleSided);
        if (!String::IsNullOrWhiteSpace(materialAsset->TextureRelativePath)) {
            WiiUGx2TextureHandle textureHandle = BuildTextureHandleFromCooked(materialAsset->TextureRelativePath, contentStreamSource);
            runtimeMaterial->SetBaseColorTextureHandle(textureHandle);
        }

        return runtimeMaterial;
    }

    /// Builds one concrete Wii U runtime material from a raw authored material path through the legacy generated-core contract that still passes a content root path.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw new ArgumentNullException("assetContentManager");
        }

        if (String::IsNullOrWhiteSpace(contentRootPath)) {
            throw new ArgumentException("Content root path must be provided.", "contentRootPath");
        }

        return BuildMaterialFromRawAsset(assetContentManager, materialAssetPath);
    }

    /// Builds one concrete Wii U runtime material from a raw authored material path through the current generated-core contract.
    ::RuntimeMaterial* WiiURenderManager3D::BuildMaterialFromRawAsset(::ContentManager* assetContentManager, std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw new ArgumentNullException("assetContentManager");
        }

        if (String::IsNullOrWhiteSpace(materialAssetPath)) {
            throw new ArgumentException("Material asset path must be provided.", "materialAssetPath");
        }

        return CreateRuntimeMaterial(materialAssetPath, float4(1.0f, 1.0f, 1.0f, 1.0f), true, false);
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

    /// Releases one runtime material built by the Wii U bridge.
    void WiiURenderManager3D::ReleaseMaterial(::RuntimeMaterial* material) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        }

        WiiURuntimeMaterial* runtimeMaterial = he_cpp_try_cast<WiiURuntimeMaterial>(material);
        if (runtimeMaterial != nullptr) {
            DestroyTextureHandle(runtimeMaterial->GetBaseColorTextureHandleStorage());
        }

        RenderManager3D::ReleaseMaterial(material);
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

        CameraClearSettings clearSettings = camera->get_ClearSettings();
        if (clearSettings.get_ClearColorEnabled()) {
            CurrentFrame.SetClearColor(ConvertClearColor(clearSettings.get_ClearColor()));
        } else {
            CurrentFrame.SetClearColor(WiiUGx2Color { 0U, 0U, 0U, 255U });
        }

        CurrentFrame.SetCamera(CreateCameraState(camera));
        CaptureSceneLighting();

        List<RenderFrameDrawableSubmission*>* drawableSubmissions = frame->get_DrawableSubmissions();
        if (drawableSubmissions == nullptr) {
            return;
        }

        for (int32_t index = 0; index < drawableSubmissions->get_Count(); index++) {
            RenderFrameDrawableSubmission* submission = (*drawableSubmissions).get_Item(index);
            if (submission == nullptr || submission->get_Drawable() == nullptr) {
                continue;
            } else if (submission->get_IsTransparent()) {
                continue;
            }

            CaptureDrawCommand(submission);
        }

        if (!CurrentFrame.GetHasDirectionalLight()) {
            return;
        }

        WiiUGx23DDirectionalShadowState directionalShadowState {};
        directionalShadowState.LightViewProjection = CreateDirectionalShadowViewProjection(camera, CurrentFrame.GetDirectionalLight());
        directionalShadowState.Strength = CurrentFrame.GetDirectionalLight().ShadowStrength;
        List<RenderFrameShadowCasterSubmission*>* shadowCasterSubmissions = frame->get_ShadowCasterSubmissions();
        if (shadowCasterSubmissions != nullptr) {
            for (int32_t index = 0; index < shadowCasterSubmissions->get_Count(); index++) {
                RenderFrameShadowCasterSubmission* submission = (*shadowCasterSubmissions).get_Item(index);
                if (submission != nullptr && submission->get_Drawable() != nullptr) {
                    CaptureShadowCasterCommand(submission, directionalShadowState);
                }
            }
        }

        CurrentFrame.SetDirectionalShadow(directionalShadowState);
    }

    /// Captures the current scene ambient and directional light state into the Wii U frame contract.
    void WiiURenderManager3D::CaptureSceneLighting() {
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            throw new InvalidOperationException("Wii U scene lighting capture requires one initialized Core object manager.");
        }

        ObjectManager* objectManager = core->get_ObjectManager();
        float4 ambientLightColor(0.0f, 0.0f, 0.0f, 0.0f);
        if (objectManager->get_AmbientLights() != nullptr) {
            for (int32_t ambientLightIndex = 0; ambientLightIndex < objectManager->get_AmbientLights()->get_Count(); ambientLightIndex++) {
                AmbientLightComponent* ambientLight = (*objectManager->get_AmbientLights()).get_Item(ambientLightIndex);
                if (ambientLight == nullptr) {
                    continue;
                }

                const float4 lightColor = CreateLightColor(ambientLight);
                ambientLightColor = float4(
                    ambientLightColor.X + lightColor.X,
                    ambientLightColor.Y + lightColor.Y,
                    ambientLightColor.Z + lightColor.Z,
                    0.0f);
            }
        }
        CurrentFrame.SetAmbientLightColor(ambientLightColor);

        if (objectManager->get_DirectionalLights() == nullptr) {
            return;
        }

        for (int32_t directionalLightIndex = 0; directionalLightIndex < objectManager->get_DirectionalLights()->get_Count(); directionalLightIndex++) {
            DirectionalLightComponent* directionalLight = (*objectManager->get_DirectionalLights()).get_Item(directionalLightIndex);
            if (directionalLight == nullptr) {
                continue;
            }

            CurrentFrame.SetDirectionalLight(CreateDirectionalLightState(directionalLight));
            return;
        }
    }

    /// Captures one extracted drawable submission into the current frame when its runtime model and runtime material are Wii U-owned.
    void WiiURenderManager3D::CaptureDrawCommand(RenderFrameDrawableSubmission* submission) {
        if (submission == nullptr) {
            throw new ArgumentNullException("submission");
        } else if (submission->get_Drawable() == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every drawable submission to own one drawable.");
        }

        IDrawable3D* drawable = submission->get_Drawable();
        WiiURuntimeModel* runtimeModel = he_cpp_try_cast<WiiURuntimeModel>(drawable->get_Model());
        WiiURuntimeMaterial* runtimeMaterial = he_cpp_try_cast<WiiURuntimeMaterial>(submission->get_Material());
        if (runtimeModel == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every runtime model to be one WiiURuntimeModel.");
        } else if (runtimeMaterial == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every runtime material to be one WiiURuntimeMaterial.");
        } else if (drawable->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U 3D capture requires every drawable to have one parent entity.");
        }

        WiiUGx23DDrawCommand drawCommand {};
        drawCommand.RuntimeModel = runtimeModel;
        drawCommand.RuntimeMaterial = runtimeMaterial;
        drawCommand.WorldMatrix = drawable->get_Parent()->get_WorldTransformMatrix();
        CurrentFrame.AddDrawCommand(drawCommand);
    }

    /// Captures one shared shadow-caster submission into the current directional-shadow draw list.
    void WiiURenderManager3D::CaptureShadowCasterCommand(RenderFrameShadowCasterSubmission* submission, WiiUGx23DDirectionalShadowState& directionalShadowState) {
        if (submission == nullptr) {
            throw new ArgumentNullException("submission");
        } else if (submission->get_Drawable() == nullptr) {
            throw new InvalidOperationException("Wii U directional-shadow capture requires every caster submission to own one drawable.");
        }

        IDrawable3D* drawable = submission->get_Drawable();
        WiiURuntimeModel* runtimeModel = he_cpp_try_cast<WiiURuntimeModel>(drawable->get_Model());
        WiiURuntimeMaterial* runtimeMaterial = he_cpp_try_cast<WiiURuntimeMaterial>(submission->get_Material());
        if (runtimeModel == nullptr) {
            throw new InvalidOperationException("Wii U directional-shadow capture requires every caster model to be one WiiURuntimeModel.");
        } else if (runtimeMaterial == nullptr) {
            throw new InvalidOperationException("Wii U directional-shadow capture requires every caster material to be one WiiURuntimeMaterial.");
        } else if (drawable->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U directional-shadow capture requires every caster drawable to have one parent entity.");
        }

        WiiUGx23DDrawCommand drawCommand {};
        drawCommand.RuntimeModel = runtimeModel;
        drawCommand.RuntimeMaterial = runtimeMaterial;
        drawCommand.WorldMatrix = drawable->get_Parent()->get_WorldTransformMatrix();
        directionalShadowState.ShadowCasterCommands.push_back(drawCommand);
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
        if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U camera-state capture requires the camera to be attached to one entity.");
        }

        cameraState.CameraPosition = camera->get_Parent()->get_Position();
        cameraState.ViewMatrix = CreateViewMatrix(camera);
        cameraState.Viewport = camera->get_Viewport();
        cameraState.NearPlaneDistance = camera->get_NearPlaneDistance();
        cameraState.FarPlaneDistance = camera->get_FarPlaneDistance();
        return cameraState;
    }

    /// Builds one normalized float color from one cooked 8-bit base-color payload.
    float4 WiiURenderManager3D::CreateBaseColor(::PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        return float4(
            static_cast<float>(materialAsset->BaseColorR) / 255.0f,
            static_cast<float>(materialAsset->BaseColorG) / 255.0f,
            static_cast<float>(materialAsset->BaseColorB) / 255.0f,
            static_cast<float>(materialAsset->BaseColorA) / 255.0f);
    }

    /// Converts one linear light color plus intensity into one packed float4 radiance color.
    float4 WiiURenderManager3D::CreateLightColor(::LightComponent* light) {
        if (light == nullptr) {
            throw new ArgumentNullException("light");
        }

        float4 color = light->get_Color();
        float intensity = light->get_Intensity();
        return float4(color.X * intensity, color.Y * intensity, color.Z * intensity, 0.0f);
    }

    /// Builds one directional-light capture record from one scene directional light.
    WiiUGx23DDirectionalLightState WiiURenderManager3D::CreateDirectionalLightState(::DirectionalLightComponent* light) {
        if (light == nullptr) {
            throw new ArgumentNullException("light");
        } else if (light->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U directional-light capture requires the light to be attached to one entity.");
        }

        const float3 direction = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), light->get_Parent()->get_Orientation());
        WiiUGx23DDirectionalLightState directionalLightState {};
        directionalLightState.Color = CreateLightColor(light);
        directionalLightState.Direction = float4(direction.X, direction.Y, direction.Z, 0.0f);
        directionalLightState.ShadowDistance = light->get_ShadowDistance();
        directionalLightState.ShadowStrength = light->get_ShadowStrength();
        return directionalLightState;
    }

    /// Builds the camera-fitted directional light view-projection matrix used for the shadow depth pass.
    float4x4 WiiURenderManager3D::CreateDirectionalShadowViewProjection(CameraComponent* camera, const WiiUGx23DDirectionalLightState& directionalLightState) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii U directional-shadow projection requires the camera to be attached to one entity.");
        }

        const float shadowDistance = directionalLightState.ShadowDistance < 1.0f ? 1.0f : directionalLightState.ShadowDistance;
        float3 cameraPosition = camera->get_Parent()->get_Position();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), camera->get_Parent()->get_Orientation());
        float3 lightDirection(-directionalLightState.Direction.X, -directionalLightState.Direction.Y, -directionalLightState.Direction.Z);
        float3 target = cameraPosition + (cameraForward * (shadowDistance * 0.5f));
        float3 lightPosition = target + (lightDirection * shadowDistance);
        const float dotUp = (lightDirection.X * 0.0f) + (lightDirection.Y * 1.0f) + (lightDirection.Z * 0.0f);
        float3 up = std::fabs(dotUp) > 0.99f ? float3(0.0f, 0.0f, 1.0f) : float3::get_UnitY();
        float4x4 viewMatrix;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(lightPosition, target, up, viewMatrix);
        const float halfDistance = shadowDistance * 0.5f;
        float4x4 projectionMatrix;
        float4x4::CreateOrthographicOffCenter__out6(-halfDistance, halfDistance, -halfDistance, halfDistance, 0.1f, shadowDistance * 2.0f, projectionMatrix);
        float4x4 lightViewProjection;
        float4x4::Multiply__ref0_ref1_out2(viewMatrix, projectionMatrix, lightViewProjection);
        return lightViewProjection;
    }

    /// Creates one concrete Wii U runtime material from the supplied material fields.
    WiiURuntimeMaterial* WiiURenderManager3D::CreateRuntimeMaterial(std::string runtimeMaterialId, float4 baseColor, bool isLit, bool isDoubleSided) {
        WiiURuntimeMaterial* runtimeMaterial = new WiiURuntimeMaterial();
        runtimeMaterial->set_Id(runtimeMaterialId);
        runtimeMaterial->SetBaseColor(baseColor);
        runtimeMaterial->SetEmissiveColor(float4(0.0f, 0.0f, 0.0f, 0.0f));
        runtimeMaterial->SetLit(isLit);
        runtimeMaterial->SetDoubleSided(isDoubleSided);
        return runtimeMaterial;
    }

    /// Builds one GX2 texture handle from a cooked runtime texture payload path.
    WiiUGx2TextureHandle WiiURenderManager3D::BuildTextureHandleFromCooked(std::string cookedAssetPath) {
        if (String::IsNullOrWhiteSpace(cookedAssetPath)) {
            throw new ArgumentException("Cooked texture asset path must be provided.", "cookedAssetPath");
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
        return BuildTextureHandleFromRaw(textureAsset);
    }

    /// Builds one GX2 texture handle from a cooked runtime texture payload path through the content-stream contract.
    WiiUGx2TextureHandle WiiURenderManager3D::BuildTextureHandleFromCooked(std::string cookedAssetPath, ::IContentStreamSource* contentStreamSource) {
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
        return BuildTextureHandleFromRaw(textureAsset);
    }

    /// Builds one GX2 texture handle from one shared-engine texture payload.
    WiiUGx2TextureHandle WiiURenderManager3D::BuildTextureHandleFromRaw(::TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0U || data->Height == 0U) {
            throw new InvalidOperationException("Wii U runtime textures require nonzero dimensions.");
        }

        std::vector<std::uint32_t> pixels = DecodeTexturePixels(data);
        WiiUGx2TextureHandle textureHandle {};
        InitializeTextureHandle(&textureHandle, data->Width, data->Height, pixels);
        return textureHandle;
    }

    /// Builds one Wii U runtime model from a shared model asset payload.
    WiiURuntimeModel* WiiURenderManager3D::BuildRuntimeModelFromAsset(::ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Positions == nullptr || data->Positions->get_Length() <= 0) {
            throw new InvalidOperationException("Wii U runtime model creation requires at least one authored vertex position.");
        } else if (data->Normals == nullptr || data->Normals->get_Length() != data->Positions->get_Length()) {
            throw new InvalidOperationException("Wii U runtime model creation requires one authored normal for every authored vertex position.");
        }

        std::vector<float> positionData;
        std::vector<float> normalData;
        std::vector<float> texCoordData;
        std::vector<std::uint16_t> indexData;
        const int32_t positionCount = data->Positions->get_Length();
        Array<float2>* texCoords = data->TexCoords;
        float minX = 0.0f;
        float minY = 0.0f;
        float minZ = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        float maxZ = 0.0f;
        positionData.reserve(static_cast<std::size_t>(positionCount) * 4U);
        normalData.reserve(static_cast<std::size_t>(positionCount) * 3U);
        texCoordData.reserve(static_cast<std::size_t>(positionCount) * 2U);
        if (texCoords != nullptr && texCoords->get_Length() != positionCount) {
            throw new InvalidOperationException("Wii U runtime model creation requires one authored UV for every authored vertex position when UVs are present.");
        }

        for (int32_t positionIndex = 0; positionIndex < positionCount; positionIndex++) {
            const float3 position = (*data->Positions)[positionIndex];
            const float3 normal = (*data->Normals)[positionIndex];
            const float2 texCoord = texCoords != nullptr
                ? (*texCoords)[positionIndex]
                : float2(0.0f, 0.0f);
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
            normalData.push_back(normal.X);
            normalData.push_back(normal.Y);
            normalData.push_back(normal.Z);
            texCoordData.push_back(texCoord.X);
            texCoordData.push_back(texCoord.Y);
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
        runtimeModel->SetGeometry(std::move(positionData), std::move(normalData), std::move(texCoordData), std::move(indexData));
        return runtimeModel;
    }

    /// Initializes one GX2 texture handle from decoded ARGB pixels.
    void WiiURenderManager3D::InitializeTextureHandle(WiiUGx2TextureHandle* textureHandle, std::uint32_t width, std::uint32_t height, const std::vector<std::uint32_t>& pixels) {
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
                destinationPixels[(row * destinationPitch) + column] = (sourcePixel << 8U)
                    | ((sourcePixel >> 24U) & 0x000000FFU);
            }
        }

        GX2RUnlockSurfaceEx(&textureHandle->Texture.surface, 0, NoGx2rResourceFlags);
        GX2RInvalidateSurface(&textureHandle->Texture.surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, textureHandle->Texture.surface.image, textureHandle->Texture.surface.imageSize);
    }

    /// Releases one GX2 texture handle owned by one Wii U runtime material.
    void WiiURenderManager3D::DestroyTextureHandle(WiiUGx2TextureHandle* textureHandle) {
        if (textureHandle == nullptr) {
            return;
        }

        if (textureHandle->Texture.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&textureHandle->Texture.surface, NoGx2rResourceFlags);
        }

        std::memset(&textureHandle->Texture, 0, sizeof(textureHandle->Texture));
        std::memset(&textureHandle->Sampler, 0, sizeof(textureHandle->Sampler));
    }

    /// Decodes one shared-engine texture payload into ARGB texels ready for GX2 upload.
    std::vector<std::uint32_t> WiiURenderManager3D::DecodeTexturePixels(::TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0U || data->Height == 0U) {
            throw new InvalidOperationException("Wii U texture decoding requires nonzero dimensions.");
        } else if (data->Colors == nullptr) {
            throw new InvalidOperationException("Wii U texture decoding requires a color payload.");
        }

        const std::size_t pixelCount = static_cast<std::size_t>(data->Width) * static_cast<std::size_t>(data->Height);
        std::vector<std::uint32_t> pixels(pixelCount, 0U);

        if (data->ColorFormat == TextureAssetColorFormat::Rgba32) {
            const std::size_t expectedByteCount = pixelCount * 4U;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U RGBA32 textures must contain tightly packed RGBA bytes.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t colorOffset = pixelIndex * 4U;
                pixels[pixelIndex] = PackArgb(
                    (*data->Colors)[static_cast<int32_t>(colorOffset + 0U)],
                    (*data->Colors)[static_cast<int32_t>(colorOffset + 1U)],
                    (*data->Colors)[static_cast<int32_t>(colorOffset + 2U)],
                    (*data->Colors)[static_cast<int32_t>(colorOffset + 3U)]);
            }

            return pixels;
        }

        if (data->ColorFormat == TextureAssetColorFormat::Rgba4444) {
            const std::size_t expectedByteCount = pixelCount * 2U;
            if (data->Colors->get_Length() != static_cast<int32_t>(expectedByteCount)) {
                throw new InvalidOperationException("Wii U RGBA4444 textures must contain tightly packed 16-bit texels.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                const std::size_t colorOffset = pixelIndex * 2U;
                const std::uint16_t packedPixel =
                    static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(colorOffset + 0U)])
                    | (static_cast<std::uint16_t>((*data->Colors)[static_cast<int32_t>(colorOffset + 1U)]) << 8U);
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
                throw new InvalidOperationException("Wii U indexed textures must contain tightly packed palette indices.");
            }

            for (std::size_t pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
                std::size_t paletteIndex = 0U;
                if (data->ColorFormat == TextureAssetColorFormat::Indexed8) {
                    paletteIndex = (*data->Colors)[static_cast<int32_t>(pixelIndex)];
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

    /// Packs one 8-bit RGBA color into one ARGB8888 word.
    std::uint32_t WiiURenderManager3D::PackArgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        return (static_cast<std::uint32_t>(alpha) << 24U)
            | (static_cast<std::uint32_t>(red) << 16U)
            | (static_cast<std::uint32_t>(green) << 8U)
            | static_cast<std::uint32_t>(blue);
    }

    /// Expands one 4-bit color channel into 8-bit precision.
    std::uint8_t WiiURenderManager3D::Expand4To8(std::uint8_t value) {
        return static_cast<std::uint8_t>((value << 4U) | value);
    }

    /// Expands one 5-bit color channel into 8-bit precision.
    std::uint8_t WiiURenderManager3D::Expand5To8(std::uint16_t value) {
        return static_cast<std::uint8_t>((value * 255U + 15U) / 31U);
    }

    /// Expands one 3-bit alpha channel into 8-bit precision.
    std::uint8_t WiiURenderManager3D::Expand3To8(std::uint16_t value) {
        return static_cast<std::uint8_t>((value * 255U + 3U) / 7U);
    }

    /// Decodes one packed GX RGB5A3 texel into ARGB8888.
    std::uint32_t WiiURenderManager3D::DecodeRgb5A3(std::uint16_t pixel) {
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

    /// Releases one transient cooked texture asset after one runtime GX2 texture handle has been rebuilt from its payload.
    void WiiURenderManager3D::ReleaseTransientTextureAsset(::TextureAsset* asset) {
        if (asset == nullptr) {
            return;
        }

        Array<std::uint8_t>* colors = asset->Colors;
        Array<std::uint8_t>* paletteColors = asset->PaletteColors;
        asset->Colors = nullptr;
        asset->PaletteColors = nullptr;
        if (colors != nullptr && colors != Array<std::uint8_t>::Empty()) {
            delete colors;
        }

        if (paletteColors != nullptr && paletteColors != Array<std::uint8_t>::Empty()) {
            delete paletteColors;
        }
    }
}

#endif
