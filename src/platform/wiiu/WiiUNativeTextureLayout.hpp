#pragma once

#include <cstddef>
#include <cstdint>

namespace helengine::wiiu {
    /// Describes the canonical compact WGT1 transport independently of the GX2 SDK and CPU endianness.
    struct WiiUNativeTextureLayout {
        /// Stores the exact GX2 surface format, including signed, integer, floating and sRGB flags.
        std::uint32_t Format = 0;
        /// Stores the serialized transport tile mode; it is not the sampled destination mode.
        std::uint32_t TileMode = 16;
        /// Stores the logical asset dimensions used by callers.
        std::uint32_t Width = 0, Height = 0;
        /// Stores the block- or chroma-padded compact payload dimensions.
        std::uint32_t StorageWidth = 0, StorageHeight = 0;
        /// Stores compact row bytes and total payload bytes, excluding the header.
        std::uint32_t PitchBytes = 0, TexelLength = 0;
        /// Stores the GX2 component map preserved when creating the sampled texture.
        std::uint32_t CompMap = 0;
        /// Stores the explicit hardware texture endian mode; v1 accepts no swap.
        std::uint32_t EndianSwap = 0;
        /// Stores the intrinsic/selected alpha policy matching the asset metadata.
        std::uint32_t AlphaPrecision = 0;
        /// Stores the compact NV12 chroma offset, or zero for non-planar formats.
        std::uint32_t UvPlaneOffset = 0;
    };

    /// Computes one exact SDK format's compact single-level WGT1 layout, rejecting unsupported format or alpha values.
    bool TryBuildWiiUNativeTextureLayout(std::uint32_t format, std::uint32_t width, std::uint32_t height,
        std::uint32_t alphaPrecision, WiiUNativeTextureLayout& layout);
    /// Validates all serialized fields, exact native byte lengths and absence of a palette before exposing the layout.
    bool TryParseWiiUNativeTexturePayload(const std::uint8_t* colors, std::size_t colorLength,
        const std::uint8_t* palette, std::size_t paletteLength, WiiUNativeTextureLayout& layout);
}
