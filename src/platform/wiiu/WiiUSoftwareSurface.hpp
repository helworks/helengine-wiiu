#pragma once

#include <cstdint>
#include <vector>

namespace helengine::wiiu {
    /// Owns one CPU-written ARGB8888 surface used by the minimal Wii U menu renderer.
    class WiiUSoftwareSurface {
    public:
        /// Creates one software surface with the supplied dimensions.
        WiiUSoftwareSurface(std::uint32_t width, std::uint32_t height);

        /// Clears the full surface to one packed ARGB8888 color.
        void Clear(std::uint32_t argbColor);

        /// Writes one packed ARGB8888 pixel when the target coordinate lies inside the surface.
        void SetPixel(int x, int y, std::uint32_t argbColor);

        /// Alpha-blends one packed ARGB8888 pixel when the target coordinate lies inside the surface.
        void BlendPixel(int x, int y, std::uint32_t argbColor);

        /// Fills one axis-aligned rectangle in surface pixel space.
        void FillRect(int x, int y, int width, int height, std::uint32_t argbColor);

        /// Returns the packed pixel buffer used for OSScreen presentation.
        const std::uint32_t* GetPixels() const;

        /// Returns the mutable packed pixel buffer used for rasterization.
        std::uint32_t* GetPixels();

        /// Returns the logical surface width in pixels.
        std::uint32_t GetWidth() const;

        /// Returns the logical surface height in pixels.
        std::uint32_t GetHeight() const;

    private:
        /// Converts one surface coordinate pair into a flat pixel-buffer index.
        std::uint32_t GetIndex(std::uint32_t x, std::uint32_t y) const;

        /// Stores the logical surface width in pixels.
        std::uint32_t Width;

        /// Stores the logical surface height in pixels.
        std::uint32_t Height;

        /// Stores packed ARGB8888 pixels in row-major order.
        std::vector<std::uint32_t> Pixels;
    };
}
