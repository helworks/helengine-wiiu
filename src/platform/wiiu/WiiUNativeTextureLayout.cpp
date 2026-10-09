#include "platform/wiiu/WiiUNativeTextureLayout.hpp"

#include <limits>

namespace helengine::wiiu {
    namespace {
        /// Lists exact non-invalid GX2SurfaceFormat words in the pinned WUT SDK; numeric flag combinations are not inferred.
        constexpr std::uint32_t Formats[] = {
            0x02,0x0b,0x01,0x07,0x1a,0x05,0x0f,0x1f,0x08,0x0a,0x0c,0x11,0x1b,0x19,
            0x31,0x32,0x33,0x34,0x35,0x81,
            0x101,0x107,0x11a,0x105,0x10f,0x11f,0x10d,0x11d,0x122,0x11b,0x119,0x111,0x11c,
            0x201,0x207,0x21a,0x205,0x20f,0x21f,0x219,0x234,0x235,
            0x301,0x307,0x31a,0x305,0x30f,0x31f,0x30d,0x31d,0x322,0x319,
            0x41a,0x431,0x432,0x433,0x80e,0x81e,0x823,0x806,0x810,0x820,0x816,0x811,0x81c
        };

        /// Reads serialized WGT1 metadata without unaligned accesses or host-endian assumptions.
        std::uint32_t ReadWord(const std::uint8_t* bytes) {
            return bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16)
                | (std::uint32_t(bytes[3]) << 24);
        }

        /// Counts physical sampled channels before canonical component selectors are applied.
        std::uint32_t Components(std::uint32_t code) {
            if (code == 10 || code == 11 || code == 12 || code == 25 || code == 26 || code == 27
                || code == 31 || code == 32 || code == 34 || code == 35 || (code >= 49 && code <= 51)) return 4;
            if (code == 8 || code == 22) return 3;
            if (code == 2 || code == 7 || code == 15 || code == 16 || code == 17 || code == 28
                || code == 29 || code == 30 || code == 53) return 2;
            return 1;
        }

        /// Returns actual scalar or BC physical bit rates, matching native GX2 storage.
        std::uint32_t Bits(std::uint32_t code) {
            if (code == 1 || code == 2) return 8;
            if (code == 5 || code == 6 || code == 7 || code == 8 || code == 10 || code == 11 || code == 12) return 16;
            if (code == 13 || code == 14 || code == 15 || code == 16 || code == 17 || code == 22
                || code == 25 || code == 26 || code == 27) return 32;
            if (code >= 28 && code <= 32) return 64;
            if (code == 34 || code == 35) return 128;
            if (code == 49 || code == 52) return 4;
            return 8;
        }

        /// Validates the explicit alpha policy without accepting unrelated A3/A5 values or silently dropping independent alpha.
        bool SupportsAlpha(std::uint32_t format, std::uint32_t alpha) {
            const auto code = format & 63u;
            const auto numeric = format >> 8;
            if (numeric == 1 || numeric == 3 || format == 0x81 || format == 0x811 || format == 0x81c
                || Components(code) != 4) return alpha == 0;
            if (code == 10 || code == 12 || code == 49) return alpha <= 1;
            if (code == 11 || code == 50) return alpha <= 2;
            if (code == 25 || code == 27) return numeric == 2 ? alpha <= 1 : alpha <= 1 || alpha == 4;
            return alpha <= 3;
        }
    }

    bool TryBuildWiiUNativeTextureLayout(std::uint32_t format, std::uint32_t width, std::uint32_t height,
        std::uint32_t alphaPrecision, WiiUNativeTextureLayout& layout) {
        bool known = false;
        for (auto value : Formats) if (value == format) { known = true; break; }
        if (!known || width == 0 || height == 0 || width > 8192 || height > 8192 || !SupportsAlpha(format, alphaPrecision)) return false;
        const auto code = format & 63u;
        const bool nv12 = format == 0x81;
        const bool compressed = code >= 49 && code <= 53;
        const std::uint32_t block = nv12 ? 2u : compressed ? 4u : 1u;
        WiiUNativeTextureLayout value;
        value.Format = format;
        value.Width = width; value.Height = height;
        value.StorageWidth = (width + block - 1u) / block * block;
        value.StorageHeight = (height + block - 1u) / block * block;
        value.AlphaPrecision = alphaPrecision;
        const auto componentCount = Components(code);
        value.CompMap = code == 12 || code == 27 ? 0x03020100u : code == 17 ? 0x01040405u : componentCount == 4 ? 0x00010203u
            : componentCount == 3 ? 0x00010205u : componentCount == 2 ? 0x00010405u : 0x00040405u;
        value.PitchBytes = nv12 ? value.StorageWidth : compressed ? value.StorageWidth / 4u * Bits(code) * 2u
            : value.StorageWidth * Bits(code) / 8u;
        value.UvPlaneOffset = nv12 ? value.StorageWidth * value.StorageHeight : 0;
        const std::uint64_t bytes = nv12 ? std::uint64_t(value.UvPlaneOffset) * 3u / 2u
            : std::uint64_t(value.PitchBytes) * (value.StorageHeight / (compressed ? 4u : 1u));
        if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) return false;
        value.TexelLength = static_cast<std::uint32_t>(bytes);
        layout = value;
        return true;
    }

    bool TryParseWiiUNativeTexturePayload(const std::uint8_t* colors, std::size_t colorLength,
        const std::uint8_t* palette, std::size_t paletteLength, WiiUNativeTextureLayout& layout) {
        (void)palette;
        if (colors == nullptr || colorLength < 64 || paletteLength != 0 || ReadWord(colors) != 0x31544757u
            || ReadWord(colors + 4) != 1 || ReadWord(colors + 12) != 16 || ReadWord(colors + 44) != 0
            || ReadWord(colors + 52) != 0 || ReadWord(colors + 60) != 0) return false;
        WiiUNativeTextureLayout value;
        if (!TryBuildWiiUNativeTextureLayout(ReadWord(colors + 8), ReadWord(colors + 16), ReadWord(colors + 20),
            ReadWord(colors + 48), value) || value.StorageWidth != ReadWord(colors + 24)
            || value.StorageHeight != ReadWord(colors + 28) || value.PitchBytes != ReadWord(colors + 32)
            || value.TexelLength != ReadWord(colors + 36) || value.CompMap != ReadWord(colors + 40)
            || value.UvPlaneOffset != ReadWord(colors + 56) || colorLength != std::size_t(value.TexelLength) + 64u) return false;
        layout = value;
        return true;
    }
}
