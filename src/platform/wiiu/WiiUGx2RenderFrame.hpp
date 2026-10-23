#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include "platform/wiiu/WiiUGx2Numeric.hpp"

namespace helengine::wiiu {
    class WiiUGx2TextureHandle;

    /// Stores one logical-space clip rectangle consumed by the GX2 presenter.
    struct WiiUGx2ClipRect {
        /// Left edge in logical pixels.
        WiiUGx2Real X;

        /// Top edge in logical pixels.
        WiiUGx2Real Y;

        /// Width in logical pixels.
        WiiUGx2Real Width;

        /// Height in logical pixels.
        WiiUGx2Real Height;
    };

    /// Stores one RGBA tint color consumed by the GX2 presenter.
    struct WiiUGx2Color {
        /// Red channel in 8-bit color space.
        std::uint8_t Red;

        /// Green channel in 8-bit color space.
        std::uint8_t Green;

        /// Blue channel in 8-bit color space.
        std::uint8_t Blue;

        /// Alpha channel in 8-bit color space.
        std::uint8_t Alpha;
    };

    /// Stores one textured logical-space quad ready for pure GX2 submission.
    struct WiiUGx2QuadCommand {
        /// Left edge in logical pixels.
        WiiUGx2Real X;

        /// Top edge in logical pixels.
        WiiUGx2Real Y;

        /// Width in logical pixels.
        WiiUGx2Real Width;

        /// Height in logical pixels.
        WiiUGx2Real Height;

        /// Rotation requested by the shared engine in radians.
        WiiUGx2Real RotationRadians;

        /// True when this command carries already transformed sprite geometry.
        bool HasResolvedCorners = false;

        /// Exact top-left, top-right, bottom-right, bottom-left logical X coordinates.
        std::array<WiiUGx2Real, 4> CornerX {};

        /// Exact top-left, top-right, bottom-right, bottom-left logical Y coordinates.
        std::array<WiiUGx2Real, 4> CornerY {};

        /// Normalized texture-space left edge.
        WiiUGx2Real SourceX;

        /// Normalized texture-space top edge.
        WiiUGx2Real SourceY;

        /// Normalized texture-space width.
        WiiUGx2Real SourceWidth;

        /// Normalized texture-space height.
        WiiUGx2Real SourceHeight;

        /// Per-quad tint applied during sampling.
        WiiUGx2Color Color;

        /// Active logical-space clip rectangle for the quad.
        WiiUGx2ClipRect ClipRect;

        /// Pointer to the GX2 texture sampled when rendering the quad.
        const WiiUGx2TextureHandle* TextureHandle;
    };

    /// Stores one full Wii U 2D frame captured from the generated-core command lists.
    class WiiUGx2RenderFrame {
    public:
        /// Creates one empty GX2 render frame with an opaque black clear color.
        WiiUGx2RenderFrame()
            : ClearColor { 0U, 0U, 0U, 255U }
            , QuadCommands() {
        }

        /// Resets the frame to an empty command list and restores the default clear color.
        void Clear() {
            ClearColor = WiiUGx2Color { 0U, 0U, 0U, 255U };
            QuadCommands.clear();
        }

        /// Stores the clear color applied before the presenter submits the frame.
        void SetClearColor(WiiUGx2Color color) {
            ClearColor = color;
        }

        /// Returns the clear color applied before the presenter submits the frame.
        const WiiUGx2Color& GetClearColor() const {
            return ClearColor;
        }

        /// Appends one textured quad command to the current frame.
        void AddQuad(const WiiUGx2QuadCommand& command) {
            QuadCommands.push_back(command);
        }

        /// Returns the captured quad commands in generated-core render order.
        const std::vector<WiiUGx2QuadCommand>& GetQuadCommands() const {
            return QuadCommands;
        }

    private:
        /// Stores the clear color used before drawing the captured quads.
        WiiUGx2Color ClearColor;

        /// Stores the captured textured quads for the current frame.
        std::vector<WiiUGx2QuadCommand> QuadCommands;
    };
}
