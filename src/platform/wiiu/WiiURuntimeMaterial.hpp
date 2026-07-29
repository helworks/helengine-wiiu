#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "platform/wiiu/WiiUGx2TextureHandle.hpp"
#include "RuntimeMaterial.hpp"
#include "float4.hpp"

namespace helengine::wiiu {
    /// Represents one concrete Wii U runtime material consumed by the opaque scene renderer.
    class WiiURuntimeMaterial final : public ::RuntimeMaterial {
    public:
        /// Creates one Wii U runtime material with the StandardShader's white colors, normalized surface defaults, and opaque single-sided lit state.
        WiiURuntimeMaterial()
            : RuntimeMaterial()
            , BaseColor(1.0f, 1.0f, 1.0f, 1.0f)
            , Roughness(0.4f)
            , Metallic(0.0f)
            , Specular(0.5f)
            , EmissiveColor(1.0f, 1.0f, 1.0f, 0.0f)
            , Lit(true)
            , DoubleSided(false)
            , HasBaseColorTexture(false)
            , BaseColorTextureHandle() {
        }

        /// Stores the material tint consumed by the opaque Wii U shader path.
        void SetBaseColor(float4 baseColor) {
            BaseColor = baseColor;
        }

        /// Returns the material tint consumed by the opaque Wii U shader path.
        const float4& GetBaseColor() const {
            return BaseColor;
        }

        /// Stores the normalized roughness consumed by the StandardShader lighting model.
        void SetRoughness(float roughness) {
            Roughness = roughness;
        }

        /// Returns the normalized roughness consumed by the StandardShader lighting model.
        float GetRoughness() const {
            return Roughness;
        }

        /// Stores the normalized metallic response consumed by the StandardShader lighting model.
        void SetMetallic(float metallic) {
            Metallic = metallic;
        }

        /// Returns the normalized metallic response consumed by the StandardShader lighting model.
        float GetMetallic() const {
            return Metallic;
        }

        /// Stores the normalized specular response consumed by the StandardShader lighting model.
        void SetSpecular(float specular) {
            Specular = specular;
        }

        /// Returns the normalized specular response consumed by the StandardShader lighting model.
        float GetSpecular() const {
            return Specular;
        }

        /// Stores the emissive-ready material contribution consumed by the opaque Wii U shader path.
        void SetEmissiveColor(float4 emissiveColor) {
            EmissiveColor = emissiveColor;
        }

        /// Returns the emissive-ready material contribution consumed by the opaque Wii U shader path.
        const float4& GetEmissiveColor() const {
            return EmissiveColor;
        }

        /// Stores whether the material should receive scene lighting in the opaque Wii U shader path.
        void SetLit(bool isLit) {
            Lit = isLit;
        }

        /// Returns whether the material should receive scene lighting in the opaque Wii U shader path.
        bool GetIsLit() const {
            return Lit;
        }

        /// Stores whether the material should render without back-face culling.
        void SetDoubleSided(bool isDoubleSided) {
            DoubleSided = isDoubleSided;
        }

        /// Returns whether the material should render without back-face culling.
        bool GetIsDoubleSided() const {
            return DoubleSided;
        }

        /// Stores the GX2 base-color texture handle consumed by the opaque Wii U shader path.
        void SetBaseColorTextureHandle(const WiiUGx2TextureHandle& textureHandle) {
            BaseColorTextureHandle = textureHandle;
            HasBaseColorTexture = true;
        }

        /// Returns the optional GX2 base-color texture handle consumed by the opaque Wii U shader path.
        const WiiUGx2TextureHandle* GetBaseColorTextureHandle() const {
            return HasBaseColorTexture
                ? &BaseColorTextureHandle
                : nullptr;
        }

        /// Returns writable access to the stored GX2 base-color texture handle for final destruction.
        WiiUGx2TextureHandle* GetBaseColorTextureHandleStorage() {
            return &BaseColorTextureHandle;
        }

    private:
        /// Stores the material tint consumed by the opaque Wii U shader path.
        float4 BaseColor;

        /// Stores the normalized roughness consumed by the StandardShader lighting model.
        float Roughness;

        /// Stores the normalized metallic response consumed by the StandardShader lighting model.
        float Metallic;

        /// Stores the normalized specular response consumed by the StandardShader lighting model.
        float Specular;

        /// Stores the emissive-ready material contribution consumed by the opaque Wii U shader path.
        float4 EmissiveColor;

        /// Tracks whether the material should receive scene lighting.
        bool Lit;

        /// Tracks whether the material should render without back-face culling.
        bool DoubleSided;

        /// Tracks whether one GX2 base-color texture handle was uploaded for this material.
        bool HasBaseColorTexture;

        /// Stores the GX2 base-color texture handle consumed by the opaque Wii U shader path.
        WiiUGx2TextureHandle BaseColorTextureHandle;
    };
}

#endif
