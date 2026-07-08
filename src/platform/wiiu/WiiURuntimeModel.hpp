#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

#include "RuntimeModel.hpp"

namespace helengine::wiiu {
    /// Represents one Wii U runtime model that exposes copied mesh geometry for the first visible cube bring-up slice.
    class WiiURuntimeModel final : public ::RuntimeModel {
    public:
        /// Creates one empty runtime model with no copied geometry.
        WiiURuntimeModel();

        /// Replaces the copied model geometry exposed to the Wii U presenter bridge.
        void SetGeometry(std::vector<float> positionData, std::vector<float> normalData, std::vector<float> texCoordData, std::vector<std::uint16_t> indexData);

        /// Returns the copied position stream as XYZW float quads.
        const std::vector<float>& GetPositionData() const;

        /// Returns the copied normal stream as tightly packed XYZ float triplets.
        const std::vector<float>& GetNormalData() const;

        /// Returns the copied UV0 stream as tightly packed XY float pairs.
        const std::vector<float>& GetTexCoordData() const;

        /// Returns the copied 16-bit index stream used for indexed GX2 drawing.
        const std::vector<std::uint16_t>& GetIndexData() const;

    private:
        /// Stores copied model positions as tightly packed XYZW float quads.
        std::vector<float> PositionData;

        /// Stores copied model normals as tightly packed XYZ float triplets.
        std::vector<float> NormalData;

        /// Stores copied model UV0 values as tightly packed XY float pairs.
        std::vector<float> TexCoordData;

        /// Stores copied 16-bit triangle indices.
        std::vector<std::uint16_t> IndexData;
    };
}

#endif
