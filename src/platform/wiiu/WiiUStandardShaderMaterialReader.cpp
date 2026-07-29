#include "platform/wiiu/WiiUStandardShaderMaterialReader.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "system/io/file.hpp"

namespace helengine::wiiu {
    namespace {
        /// Identifies the platform-owned Wii U StandardShader material payload independently of legacy generated-core assets.
        constexpr std::uint8_t MaterialPayloadMagic[] = { 'W', 'U', 'M', 'T' };

        /// Identifies the initial fixed-order little-endian StandardShader material payload contract.
        constexpr std::uint32_t MaterialPayloadVersion = 1U;

        /// Limits an individual material identity to a defensive one-megabyte allocation.
        constexpr std::size_t MaximumStringByteCount = 1024U * 1024U;
    }

    /// Opens a cooked asset path and decodes it when its signature identifies a StandardShader material payload.
    bool WiiUStandardShaderMaterialReader::TryRead(const std::string& path, WiiUStandardShaderMaterial& material) {
        FileStream* stream = File::OpenRead(path.c_str());
        if (stream == nullptr) {
            throw std::runtime_error("The Wii U StandardShader material path could not be opened.");
        }

        try {
            bool result = TryRead(stream, material);
            stream->Dispose();
            delete stream;
            return result;
        } catch (...) {
            stream->Dispose();
            delete stream;
            throw;
        }
    }

    /// Decodes a StandardShader material from the stream's current position without assuming the stream can seek.
    bool WiiUStandardShaderMaterialReader::TryRead(::Stream* stream, WiiUStandardShaderMaterial& material) {
        if (stream == nullptr) {
            throw std::runtime_error("A stream is required to read a Wii U StandardShader material payload.");
        }

        std::uint8_t magic[sizeof(MaterialPayloadMagic)];
        if (!TryReadExact(stream, magic, sizeof(magic))) {
            return false;
        }

        if (std::memcmp(magic, MaterialPayloadMagic, sizeof(MaterialPayloadMagic)) != 0) {
            return false;
        }

        std::uint32_t version;
        if (!TryReadUInt32(stream, version)) {
            throw std::runtime_error("The Wii U StandardShader material payload is truncated before its version.");
        } else if (version != MaterialPayloadVersion) {
            throw std::runtime_error("The Wii U StandardShader material payload version is unsupported.");
        }

        WiiUStandardShaderMaterial decodedMaterial;
        if (!TryReadString(stream, decodedMaterial.MaterialAssetId)
            || !TryReadString(stream, decodedMaterial.ShaderAssetId)
            || !TryReadString(stream, decodedMaterial.VertexProgramName)
            || !TryReadString(stream, decodedMaterial.PixelProgramName)
            || !TryReadString(stream, decodedMaterial.VariantName)
            || !TryReadString(stream, decodedMaterial.DiffuseTextureAssetId)) {
            throw std::runtime_error("The Wii U StandardShader material payload contains a truncated or invalid string field.");
        }

        std::uint8_t baseColor[4];
        if (!TryReadExact(stream, baseColor, sizeof(baseColor))) {
            throw std::runtime_error("The Wii U StandardShader material payload is truncated in its base color.");
        }

        decodedMaterial.BaseColorR = baseColor[0];
        decodedMaterial.BaseColorG = baseColor[1];
        decodedMaterial.BaseColorB = baseColor[2];
        decodedMaterial.BaseColorA = baseColor[3];
        if (!TryReadFloat(stream, decodedMaterial.Roughness)
            || !TryReadFloat(stream, decodedMaterial.Metallic)
            || !TryReadFloat(stream, decodedMaterial.Specular)) {
            throw std::runtime_error("The Wii U StandardShader material payload is truncated in its scalar fields.");
        }

        std::uint8_t emissiveColor[4];
        if (!TryReadExact(stream, emissiveColor, sizeof(emissiveColor))) {
            throw std::runtime_error("The Wii U StandardShader material payload is truncated in its emissive color.");
        }

        decodedMaterial.EmissiveColorR = emissiveColor[0];
        decodedMaterial.EmissiveColorG = emissiveColor[1];
        decodedMaterial.EmissiveColorB = emissiveColor[2];
        decodedMaterial.EmissiveColorA = emissiveColor[3];
        if (!TryReadBoolean(stream, decodedMaterial.Lit)
            || !TryReadBoolean(stream, decodedMaterial.DoubleSided)) {
            throw std::runtime_error("The Wii U StandardShader material payload contains a truncated or invalid Boolean field.");
        }

        Validate(decodedMaterial);
        material = decodedMaterial;
        return true;
    }

    /// Reads exactly the requested bytes and reports whether the complete field was available.
    bool WiiUStandardShaderMaterialReader::TryReadExact(::Stream* stream, std::uint8_t* destination, std::size_t byteCount) {
        std::size_t totalByteCount = 0U;
        while (totalByteCount < byteCount) {
            std::size_t readByteCount = stream->Read(destination, totalByteCount, byteCount - totalByteCount);
            if (readByteCount == 0U) {
                return false;
            }

            totalByteCount += readByteCount;
        }

        return true;
    }

    /// Reads one unsigned 32-bit little-endian value.
    bool WiiUStandardShaderMaterialReader::TryReadUInt32(::Stream* stream, std::uint32_t& value) {
        std::uint8_t bytes[4];
        if (!TryReadExact(stream, bytes, sizeof(bytes))) {
            return false;
        }

        value = static_cast<std::uint32_t>(bytes[0])
            | (static_cast<std::uint32_t>(bytes[1]) << 8U)
            | (static_cast<std::uint32_t>(bytes[2]) << 16U)
            | (static_cast<std::uint32_t>(bytes[3]) << 24U);
        return true;
    }

    /// Reads one signed 32-bit little-endian value.
    bool WiiUStandardShaderMaterialReader::TryReadInt32(::Stream* stream, std::int32_t& value) {
        std::uint32_t unsignedValue;
        if (!TryReadUInt32(stream, unsignedValue)) {
            return false;
        }

        std::memcpy(&value, &unsignedValue, sizeof(value));
        return true;
    }

    /// Reads one IEEE 754 single-precision value encoded in little-endian byte order.
    bool WiiUStandardShaderMaterialReader::TryReadFloat(::Stream* stream, float& value) {
        std::uint32_t bits;
        if (!TryReadUInt32(stream, bits)) {
            return false;
        }

        static_assert(sizeof(value) == sizeof(bits), "Wii U StandardShader material floats must occupy 32 bits.");
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    /// Reads one canonical Boolean byte and rejects encodings other than zero and one.
    bool WiiUStandardShaderMaterialReader::TryReadBoolean(::Stream* stream, bool& value) {
        std::uint8_t byte;
        if (!TryReadExact(stream, &byte, 1U) || byte > 1U) {
            return false;
        }

        value = byte == 1U;
        return true;
    }

    /// Reads one signed-length-prefixed UTF-8 string without allocating an unbounded buffer.
    bool WiiUStandardShaderMaterialReader::TryReadString(::Stream* stream, std::string& value) {
        std::int32_t signedByteCount;
        if (!TryReadInt32(stream, signedByteCount) || signedByteCount < 0) {
            return false;
        }

        std::size_t byteCount = static_cast<std::size_t>(signedByteCount);
        if (byteCount > MaximumStringByteCount) {
            return false;
        }

        if (stream->CanSeek()) {
            std::size_t position = stream->Position();
            std::size_t length = stream->Length();
            if (position > length || byteCount > length - position) {
                return false;
            }
        }

        if (byteCount == 0U) {
            value.clear();
            return true;
        }

        std::vector<std::uint8_t> bytes(byteCount);
        if (!TryReadExact(stream, bytes.data(), byteCount)) {
            return false;
        }

        value.assign(reinterpret_cast<const char*>(bytes.data()), byteCount);
        return true;
    }

    /// Enforces the required identities and normalized scalar ranges of the decoded material contract.
    void WiiUStandardShaderMaterialReader::Validate(const WiiUStandardShaderMaterial& material) {
        if (material.MaterialAssetId.find_first_not_of(" \t\r\n") == std::string::npos) {
            throw std::runtime_error("Wii U StandardShader materials require one material asset id.");
        } else if (material.ShaderAssetId.find_first_not_of(" \t\r\n") == std::string::npos) {
            throw std::runtime_error("Wii U StandardShader materials require one shader asset id.");
        } else if (material.VertexProgramName.find_first_not_of(" \t\r\n") == std::string::npos) {
            throw std::runtime_error("Wii U StandardShader materials require one vertex-program name.");
        } else if (material.PixelProgramName.find_first_not_of(" \t\r\n") == std::string::npos) {
            throw std::runtime_error("Wii U StandardShader materials require one pixel-program name.");
        } else if (material.VariantName.find_first_not_of(" \t\r\n") == std::string::npos) {
            throw std::runtime_error("Wii U StandardShader materials require one shader variant name.");
        }

        if (!std::isfinite(material.Roughness) || material.Roughness < 0.0f || material.Roughness > 1.0f) {
            throw std::runtime_error("Wii U StandardShader material roughness must be finite and normalized.");
        } else if (!std::isfinite(material.Metallic) || material.Metallic < 0.0f || material.Metallic > 1.0f) {
            throw std::runtime_error("Wii U StandardShader material metallic response must be finite and normalized.");
        } else if (!std::isfinite(material.Specular) || material.Specular < 0.0f || material.Specular > 1.0f) {
            throw std::runtime_error("Wii U StandardShader material specular response must be finite and normalized.");
        }
    }
}

#endif
