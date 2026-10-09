#pragma once

#include <cstddef>
#include <cstdint>

#include "platform/wiiu/WiiUGx2TextureHandle.hpp"
#include "platform/wiiu/WiiUNativeTextureLayout.hpp"

namespace helengine::wiiu {
    /// Copies canonical native rows into a SDK-sized sampled LINEAR_ALIGNED surface without expanding texels.
    void InitializeWiiUNativeTextureHandle(WiiUGx2TextureHandle* handle, const WiiUNativeTextureLayout& layout,
        const std::uint8_t* pixels, std::size_t pixelLength, bool wrap);
    /// Updates mutable RGBA8 surfaces in place after validating bounds/stride and completing earlier GPU fetches.
    void UpdateWiiURgbaTextureRegion(WiiUGx2TextureHandle* handle, std::int32_t x, std::int32_t y,
        std::int32_t width, std::int32_t height, const std::uint8_t* rgba, std::size_t byteLength,
        std::int32_t sourceRowPitch, std::uint32_t alphaPrecision, bool preserveCoverage);
    /// Destroys a texture surface; its caller must have completed the GPU fence and retired captured references.
    void DestroyWiiUTextureHandleAfterFence(WiiUGx2TextureHandle* handle);
}
