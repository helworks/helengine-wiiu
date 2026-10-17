#pragma once

#include <stdexcept>
#include "platform/wiiu/WiiUGx2RenderFrame.hpp"

namespace helengine::wiiu {
    /// Projects captured logical corners into the presenter's existing six-vertex UV triangle order.
    class WiiUGx2QuadGeometry {
    public:
        /// Writes bottom-left, bottom-right, top-right / bottom-left, top-right, top-left positions.
        static void WritePositions(const WiiUGx2QuadCommand& command, float logicalWidth, float logicalHeight, float* positions) {
            if (logicalWidth <= 0 || logicalHeight <= 0 || positions == nullptr) {
                throw std::invalid_argument("GX2 quad projection requires positive logical dimensions and an output buffer.");
            }
            const float rectangleX[4] = {command.X, command.X + command.Width, command.X + command.Width, command.X};
            const float rectangleY[4] = {command.Y, command.Y, command.Y + command.Height, command.Y + command.Height};
            const int corners[6] = {3, 2, 1, 3, 1, 0};
            for (int index = 0; index < 6; ++index) {
                const int corner = corners[index];
                const float x = command.HasResolvedCorners ? command.CornerX[corner] : rectangleX[corner];
                const float y = command.HasResolvedCorners ? command.CornerY[corner] : rectangleY[corner];
                positions[index * 2] = x / logicalWidth * 2.0f - 1.0f;
                positions[index * 2 + 1] = 1.0f - y / logicalHeight * 2.0f;
            }
        }
    };
}
