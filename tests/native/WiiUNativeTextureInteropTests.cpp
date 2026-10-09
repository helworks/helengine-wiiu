#include "platform/wiiu/WiiUNativeTextureLayout.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    /// Reads a fixture emitted by the actual managed cooker without relying on its descriptor parser.
    std::vector<std::uint8_t> ReadBytes(const std::string& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Missing native fixture: " + path);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    /// Reads one independently emitted little-endian expectation word on either host byte order.
    std::uint32_t ReadWord(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        if (offset + 4 > bytes.size()) throw std::runtime_error("Truncated fixture metadata.");
        return bytes[offset] | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u)
            | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u)
            | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    }

    /// Replaces one descriptor field to verify strict rejection rather than silent native fallback.
    void WriteWord(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
        for (std::size_t index = 0; index < 4; ++index) bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8u));
    }

    /// Reports a compatibility failure with its fixture index and field name.
    void Require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    /// Parses the unmodified production payload using the pure native runtime entry point.
    bool Parse(const std::vector<std::uint8_t>& colors, const std::vector<std::uint8_t>& palette,
        helengine::wiiu::WiiUNativeTextureLayout& layout) {
        return helengine::wiiu::TryParseWiiUNativeTexturePayload(colors.data(), colors.size(),
            palette.empty() ? nullptr : palette.data(), palette.size(), layout);
    }

    /// Checks one cooker payload and rejects invalid versions, storage descriptions, flags and palettes.
    void CheckFixture(const std::string& prefix) {
        const auto colors = ReadBytes(prefix + ".colors");
        const auto palette = ReadBytes(prefix + ".palette");
        const auto metadata = ReadBytes(prefix + ".meta");
        Require(metadata.size() == 20, prefix + ": metadata length");
        helengine::wiiu::WiiUNativeTextureLayout layout{};
        Require(Parse(colors, palette, layout), prefix + ": native parser rejected cooker payload");
        Require(layout.Width == ReadWord(metadata, 0) && layout.Height == ReadWord(metadata, 4), prefix + ": dimensions");
        Require(layout.Format == ReadWord(metadata, 8), prefix + ": GX2 format");
        Require(layout.AlphaPrecision == ReadWord(metadata, 12), prefix + ": alpha precision");
        Require(ReadWord(metadata, 16) == 21, prefix + ": native asset tag");
        Require(layout.TileMode == 16 && layout.EndianSwap == 0, prefix + ": canonical transport");
        Require(colors.size() == 64u + layout.TexelLength && palette.empty(), prefix + ": owned payload extent");
        for (std::size_t offset : {std::size_t{0}, std::size_t{4}, std::size_t{52}, std::size_t{60}}) {
            auto corrupted = colors;
            WriteWord(corrupted, offset, 0xFFFFFFFFu);
            Require(!Parse(corrupted, palette, layout), prefix + ": invalid descriptor word was accepted");
        }
        for (std::size_t offset : {std::size_t{8}, std::size_t{12}, std::size_t{32}, std::size_t{40}, std::size_t{44}}) {
            auto corrupted = colors;
            WriteWord(corrupted, offset, 0xFFFFFFFFu);
            Require(!Parse(corrupted, palette, layout), prefix + ": invalid format, pitch or interpretation was accepted");
        }
        auto truncated = colors;
        truncated.pop_back();
        Require(!Parse(truncated, palette, layout), prefix + ": truncated texels were accepted");
        const std::vector<std::uint8_t> unexpectedPalette{0, 0, 0, 0};
        Require(!Parse(colors, unexpectedPalette, layout), prefix + ": unexpected native palette was accepted");
    }
}

/// Verifies actual managed cooker and raw import payloads with the unmodified production native parser.
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Expected fixture directory and fixture count.");
        const int count = std::stoi(argv[2]);
        for (int index = 0; index < count; ++index) {
            std::ostringstream prefix;
            prefix << argv[1] << '/' << std::setfill('0') << std::setw(4) << index;
            CheckFixture(prefix.str());
        }
        std::cout << count << " managed/native Wii U fixtures passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
