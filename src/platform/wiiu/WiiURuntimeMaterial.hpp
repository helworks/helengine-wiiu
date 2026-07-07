#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include "RuntimeMaterial.hpp"
#include "float4.hpp"

namespace helengine::wiiu {
    /// Represents one concrete Wii U runtime material consumed by the opaque scene renderer.
    class WiiURuntimeMaterial final : public ::RuntimeMaterial {
    public:
        /// Creates one Wii U runtime material with white base color, zero emissive color, and opaque single-sided lit defaults.
        WiiURuntimeMaterial()
            : RuntimeMaterial()
            , BaseColor(1.0f, 1.0f, 1.0f, 1.0f)
            , EmissiveColor(0.0f, 0.0f, 0.0f, 0.0f)
            , Lit(true)
            , DoubleSided(false) {
        }

        /// Stores the material tint consumed by the opaque Wii U shader path.
        void SetBaseColor(float4 baseColor) {
            BaseColor = baseColor;
        }

        /// Returns the material tint consumed by the opaque Wii U shader path.
        const float4& GetBaseColor() const {
            return BaseColor;
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

    private:
        /// Stores the material tint consumed by the opaque Wii U shader path.
        float4 BaseColor;

        /// Stores the emissive-ready material contribution consumed by the opaque Wii U shader path.
        float4 EmissiveColor;

        /// Tracks whether the material should receive scene lighting.
        bool Lit;

        /// Tracks whether the material should render without back-face culling.
        bool DoubleSided;
    };
}

#endif
