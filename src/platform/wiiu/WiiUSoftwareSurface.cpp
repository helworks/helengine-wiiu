#include "platform/wiiu/WiiUSoftwareSurface.hpp"

#include <algorithm>
#include <stdexcept>

namespace helengine::wiiu {
    /// Creates one software surface with the supplied dimensions.
    WiiUSoftwareSurface::WiiUSoftwareSurface(std::uint32_t width, std::uint32_t height)
        : Width(width)
        , Height(height)
        , Pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U) {
        if (width == 0U || height == 0U) {
            throw std::invalid_argument("Wii U software surface dimensions must be non-zero.");
        }
    }

    /// Clears the full surface to one packed ARGB8888 color.
    void WiiUSoftwareSurface::Clear(std::uint32_t argbColor) {
        std::fill(Pixels.begin(), Pixels.end(), argbColor);
    }

    /// Writes one packed ARGB8888 pixel when the target coordinate lies inside the surface.
    void WiiUSoftwareSurface::SetPixel(int x, int y, std::uint32_t argbColor) {
        if (x < 0 || y < 0 || x >= static_cast<int>(Width) || y >= static_cast<int>(Height)) {
            return;
        }

        Pixels[GetIndex(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))] = argbColor;
    }

    /// Alpha-blends one packed ARGB8888 pixel when the target coordinate lies inside the surface.
    void WiiUSoftwareSurface::BlendPixel(int x, int y, std::uint32_t argbColor) {
        if (x < 0 || y < 0 || x >= static_cast<int>(Width) || y >= static_cast<int>(Height)) {
            return;
        }

        std::uint32_t& destination = Pixels[GetIndex(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))];
        std::uint32_t sourceAlpha = (argbColor >> 24) & 0xFFU;
        if (sourceAlpha == 0U) {
            return;
        }

        if (sourceAlpha == 0xFFU) {
            destination = argbColor;
            return;
        }

        std::uint32_t inverseAlpha = 0xFFU - sourceAlpha;
        std::uint32_t sourceRed = (argbColor >> 16) & 0xFFU;
        std::uint32_t sourceGreen = (argbColor >> 8) & 0xFFU;
        std::uint32_t sourceBlue = argbColor & 0xFFU;
        std::uint32_t destinationRed = (destination >> 16) & 0xFFU;
        std::uint32_t destinationGreen = (destination >> 8) & 0xFFU;
        std::uint32_t destinationBlue = destination & 0xFFU;
        std::uint32_t blendedRed = ((sourceRed * sourceAlpha) + (destinationRed * inverseAlpha)) / 0xFFU;
        std::uint32_t blendedGreen = ((sourceGreen * sourceAlpha) + (destinationGreen * inverseAlpha)) / 0xFFU;
        std::uint32_t blendedBlue = ((sourceBlue * sourceAlpha) + (destinationBlue * inverseAlpha)) / 0xFFU;
        destination = 0xFF000000U | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
    }

    /// Fills one axis-aligned rectangle in surface pixel space.
    void WiiUSoftwareSurface::FillRect(int x, int y, int width, int height, std::uint32_t argbColor) {
        for (int row = 0; row < height; row++) {
            for (int column = 0; column < width; column++) {
                BlendPixel(x + column, y + row, argbColor);
            }
        }
    }

    /// Returns the packed pixel buffer used for OSScreen presentation.
    const std::uint32_t* WiiUSoftwareSurface::GetPixels() const {
        return Pixels.data();
    }

    /// Returns the mutable packed pixel buffer used for rasterization.
    std::uint32_t* WiiUSoftwareSurface::GetPixels() {
        return Pixels.data();
    }

    /// Returns the logical surface width in pixels.
    std::uint32_t WiiUSoftwareSurface::GetWidth() const {
        return Width;
    }

    /// Returns the logical surface height in pixels.
    std::uint32_t WiiUSoftwareSurface::GetHeight() const {
        return Height;
    }

    /// Converts one surface coordinate pair into a flat pixel-buffer index.
    std::uint32_t WiiUSoftwareSurface::GetIndex(std::uint32_t x, std::uint32_t y) const {
        return (y * Width) + x;
    }
}
