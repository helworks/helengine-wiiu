#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <string>

class Stream;

namespace helengine::wiiu {
    /// Stores every field decoded from one platform-owned Wii U StandardShader material payload.
    struct WiiUStandardShaderMaterial final {
        /// Identifies the authored material asset represented by the payload.
        std::string MaterialAssetId;

        /// Identifies the StandardShader asset selected by the cooker.
        std::string ShaderAssetId;

        /// Names the cooked StandardShader vertex program.
        std::string VertexProgramName;

        /// Names the cooked StandardShader pixel program.
        std::string PixelProgramName;

        /// Names the cooked StandardShader variant.
        std::string VariantName;

        /// Identifies the optional cooked diffuse texture, or remains empty when none is bound.
        std::string DiffuseTextureAssetId;

        /// Stores the red byte channel of the material base color.
        std::uint8_t BaseColorR;

        /// Stores the green byte channel of the material base color.
        std::uint8_t BaseColorG;

        /// Stores the blue byte channel of the material base color.
        std::uint8_t BaseColorB;

        /// Stores the alpha byte channel of the material base color.
        std::uint8_t BaseColorA;

        /// Stores the normalized surface roughness.
        float Roughness;

        /// Stores the normalized metallic response.
        float Metallic;

        /// Stores the normalized specular response.
        float Specular;

        /// Stores the red byte channel of the material emissive color.
        std::uint8_t EmissiveColorR;

        /// Stores the green byte channel of the material emissive color.
        std::uint8_t EmissiveColorG;

        /// Stores the blue byte channel of the material emissive color.
        std::uint8_t EmissiveColorB;

        /// Stores the alpha byte channel of the material emissive color.
        std::uint8_t EmissiveColorA;

        /// Stores whether the material participates in StandardShader lighting.
        bool Lit;

        /// Stores whether the material renders without back-face culling.
        bool DoubleSided;
    };

    /// Recognizes and decodes the versioned platform-owned Wii U StandardShader material contract.
    class WiiUStandardShaderMaterialReader final {
    public:
        /// Opens a cooked asset path and decodes it when its signature identifies a StandardShader material payload.
        static bool TryRead(const std::string& path, WiiUStandardShaderMaterial& material);

        /// Decodes a StandardShader material from the stream's current position without assuming the stream can seek.
        static bool TryRead(::Stream* stream, WiiUStandardShaderMaterial& material);

    private:
        /// Reads exactly the requested bytes and reports whether the complete field was available.
        static bool TryReadExact(::Stream* stream, std::uint8_t* destination, std::size_t byteCount);

        /// Reads one unsigned 32-bit little-endian value.
        static bool TryReadUInt32(::Stream* stream, std::uint32_t& value);

        /// Reads one signed 32-bit little-endian value.
        static bool TryReadInt32(::Stream* stream, std::int32_t& value);

        /// Reads one IEEE 754 single-precision value encoded in little-endian byte order.
        static bool TryReadFloat(::Stream* stream, float& value);

        /// Reads one canonical Boolean byte and rejects encodings other than zero and one.
        static bool TryReadBoolean(::Stream* stream, bool& value);

        /// Reads one signed-length-prefixed UTF-8 string without allocating an unbounded buffer.
        static bool TryReadString(::Stream* stream, std::string& value);

        /// Enforces the required identities and normalized scalar ranges of the decoded material contract.
        static void Validate(const WiiUStandardShaderMaterial& material);
    };
}

#endif
