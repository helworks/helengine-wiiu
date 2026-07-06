#include "platform/wiiu/WiiURuntimeModel.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

namespace helengine::wiiu {
    /// Creates one empty runtime model with no copied geometry.
    WiiURuntimeModel::WiiURuntimeModel()
        : RuntimeModel() {
    }

    /// Replaces the copied model geometry exposed to the Wii U presenter bridge.
    void WiiURuntimeModel::SetGeometry(std::vector<float> positionData, std::vector<std::uint16_t> indexData) {
        PositionData = std::move(positionData);
        IndexData = std::move(indexData);
    }

    /// Returns the copied position stream as XYZW float quads.
    const std::vector<float>& WiiURuntimeModel::GetPositionData() const {
        return PositionData;
    }

    /// Returns the copied 16-bit index stream used for indexed GX2 drawing.
    const std::vector<std::uint16_t>& WiiURuntimeModel::GetIndexData() const {
        return IndexData;
    }
}

#endif
