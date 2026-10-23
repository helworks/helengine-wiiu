#pragma once

#include <algorithm>
#include <stdexcept>
#include "platform/wiiu/WiiUGx2RenderFrame.hpp"

namespace helengine::wiiu {
    /// Projects captured logical corners into the presenter's existing six-vertex UV triangle order.
    class WiiUGx2QuadGeometry {
    public:
        /// Writes bottom-left, bottom-right, top-right / bottom-left, top-right, top-left positions.
        static void WritePositions(const WiiUGx2QuadCommand& command, WiiUGx2Evaluation logicalWidth, WiiUGx2Evaluation logicalHeight, float* positions) {
            if (logicalWidth <= 0 || logicalHeight <= 0 || positions == nullptr) {
                throw std::invalid_argument("GX2 quad projection requires positive logical dimensions and an output buffer.");
            }
            const WiiUGx2Evaluation rectangleX[4] = {command.X, command.X + command.Width, command.X + command.Width, command.X};
            const WiiUGx2Evaluation rectangleY[4] = {command.Y, command.Y, command.Y + command.Height, command.Y + command.Height};
            const int corners[6] = {3, 2, 1, 3, 1, 0};
            for (int index = 0; index < 6; ++index) {
                const int corner = corners[index];
                const WiiUGx2Evaluation x = command.HasResolvedCorners ? WiiUGx2Evaluation(command.CornerX[corner]) : rectangleX[corner];
                const WiiUGx2Evaluation y = command.HasResolvedCorners ? WiiUGx2Evaluation(command.CornerY[corner]) : rectangleY[corner];
                positions[index * 2] = static_cast<float>(x / logicalWidth * 2.0f - 1.0f);
                positions[index * 2 + 1] = static_cast<float>(1.0f - y / logicalHeight * 2.0f);
            }
        }

        /// Packs texture coordinates only after source-rectangle arithmetic uses the selected precision.
        static void WriteTextureCoordinates(const WiiUGx2QuadCommand& command, float* coordinates) {
            if (coordinates == nullptr) {
                throw std::invalid_argument("GX2 texture coordinate packing requires an output buffer.");
            }
            const float left = static_cast<float>(WiiUGx2Evaluation(command.SourceX));
            const float top = static_cast<float>(WiiUGx2Evaluation(command.SourceY));
            const float right = static_cast<float>(WiiUGx2Evaluation(command.SourceX) + command.SourceWidth);
            const float bottom = static_cast<float>(WiiUGx2Evaluation(command.SourceY) + command.SourceHeight);
            const float values[12] = {left, bottom, right, bottom, right, top, left, bottom, right, top, left, top};
            std::copy(values, values + 12, coordinates);
        }

        /// Clips and scales in configured arithmetic before packing the integer GX2 scissor rectangle.
        static void WriteScissor(const WiiUGx2ClipRect& clip, WiiUGx2Evaluation logicalWidth, WiiUGx2Evaluation logicalHeight,
            WiiUGx2Evaluation targetWidth, WiiUGx2Evaluation targetHeight, std::uint32_t* scissor) {
            if (logicalWidth <= 0 || logicalHeight <= 0 || targetWidth <= 0 || targetHeight <= 0 || scissor == nullptr) {
                throw std::invalid_argument("GX2 scissor packing requires positive dimensions and an output buffer.");
            }
            const WiiUGx2Evaluation zero(0);
            const WiiUGx2Evaluation scaleX = targetWidth / logicalWidth;
            const WiiUGx2Evaluation scaleY = targetHeight / logicalHeight;
            const WiiUGx2Evaluation left = std::max(zero, WiiUGx2Evaluation(clip.X));
            const WiiUGx2Evaluation top = std::max(zero, WiiUGx2Evaluation(clip.Y));
            const WiiUGx2Evaluation right = std::min(logicalWidth, WiiUGx2Evaluation(clip.X) + clip.Width);
            const WiiUGx2Evaluation bottom = std::min(logicalHeight, WiiUGx2Evaluation(clip.Y) + clip.Height);
            if (right <= left || bottom <= top) {
                std::fill(scissor, scissor + 4, 0U);
                return;
            }
            const WiiUGx2Evaluation x = WiiUGx2Math::Floor(left * scaleX);
            const WiiUGx2Evaluation y = WiiUGx2Math::Floor(top * scaleY);
            scissor[0] = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::max(zero, x)));
            scissor[1] = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::max(zero, y)));
            scissor[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::max(zero, WiiUGx2Math::Ceiling(right * scaleX) - x)));
            scissor[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(std::max(zero, WiiUGx2Math::Ceiling(bottom * scaleY) - y)));
        }
    };
}
