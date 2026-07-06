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
        void SetGeometry(std::vector<float> positionData, std::vector<std::uint16_t> indexData);

        /// Returns the copied position stream as XYZW float quads.
        const std::vector<float>& GetPositionData() const;

        /// Returns the copied 16-bit index stream used for indexed GX2 drawing.
        const std::vector<std::uint16_t>& GetIndexData() const;

    private:
        /// Stores copied model positions as tightly packed XYZW float quads.
        std::vector<float> PositionData;

        /// Stores copied 16-bit triangle indices.
        std::vector<std::uint16_t> IndexData;
    };
}

#endif
