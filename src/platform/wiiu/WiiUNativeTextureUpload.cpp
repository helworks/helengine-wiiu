#include "platform/wiiu/WiiUNativeTextureUpload.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/surface.h>
#include <gx2/utils.h>
#include <gx2r/surface.h>

namespace helengine::wiiu {
    namespace {
        /// Defines the GX2R flags for CPU-written surfaces subsequently read by texture shaders.
        constexpr GX2RResourceFlags TextureFlags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
        /// Represents calls that request the SDK's ordinary lock/unlock/destroy behavior.
        constexpr GX2RResourceFlags NoFlags = static_cast<GX2RResourceFlags>(0);

        /// Converts independent alpha only for explicitly cooked mutable RGBA8 textures.
        std::uint8_t NormalizeAlpha(std::uint8_t value, std::uint32_t precision) {
            if (precision == 0) return 255;
            if (precision == 1) return value >= 128 ? 255 : 0;
            if (precision == 2) return static_cast<std::uint8_t>(((value * 15u + 127u) / 255u) * 17u);
            if (precision == 3) return value;
            throw std::invalid_argument("Wii U RGBA8 updates require Opaque, Binary, A4 or A8 alpha.");
        }

        /// Checks both row pitch and last byte before any GX2 memory write.
        void CopyRows(std::uint8_t* destination, std::size_t capacity, std::size_t offset,
            std::size_t destinationPitch, const std::uint8_t* source, std::size_t sourcePitch, std::size_t rows) {
            if (destinationPitch < sourcePitch || offset > capacity
                || (rows != 0 && (rows - 1) > (capacity - offset) / destinationPitch)
                || (rows != 0 && sourcePitch > capacity - offset - (rows - 1) * destinationPitch)) {
                throw std::runtime_error("Wii U GX2 surface does not contain the required native rows.");
            }
            for (std::size_t row = 0; row < rows; ++row) {
                std::memcpy(destination + offset + row * destinationPitch, source + row * sourcePitch, sourcePitch);
            }
        }
    }

    void InitializeWiiUNativeTextureHandle(WiiUGx2TextureHandle* handle, const WiiUNativeTextureLayout& layout,
        const std::uint8_t* pixels, std::size_t pixelLength, bool wrap) {
        WiiUNativeTextureLayout verified;
        if (handle == nullptr || handle->Texture.surface.image != nullptr || pixels == nullptr
            || !TryBuildWiiUNativeTextureLayout(layout.Format, layout.Width, layout.Height, layout.AlphaPrecision, verified)
            || layout.PitchBytes != verified.PitchBytes || layout.TexelLength != verified.TexelLength
            || pixelLength != verified.TexelLength || layout.CompMap != verified.CompMap
            || layout.StorageWidth != verified.StorageWidth || layout.StorageHeight != verified.StorageHeight
            || layout.TileMode != verified.TileMode || layout.EndianSwap != verified.EndianSwap
            || layout.UvPlaneOffset != verified.UvPlaneOffset) {
            throw std::invalid_argument("Wii U native texture upload requires a validated compact payload and empty handle.");
        }
        GX2Surface& surface = handle->Texture.surface;
        surface.use = GX2_SURFACE_USE_TEXTURE;
        surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        surface.width = layout.Format == 0x81u ? layout.StorageWidth : layout.Width;
        surface.height = layout.Format == 0x81u ? layout.StorageHeight : layout.Height;
        surface.depth = 1;
        surface.mipLevels = 1;
        surface.format = static_cast<GX2SurfaceFormat>(layout.Format);
        surface.aa = GX2_AA_MODE1X;
        surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        GX2CalcSurfaceSizeAndAlignment(&surface);
        if (surface.tileMode != GX2_TILE_MODE_LINEAR_ALIGNED || !GX2RCreateSurface(&surface, TextureFlags)) {
            DestroyWiiUTextureHandleAfterFence(handle);
            throw std::runtime_error("Wii U GX2 failed to allocate a native linear-aligned texture surface.");
        }
        try {
            auto* destination = static_cast<std::uint8_t*>(GX2RLockSurfaceEx(&surface, 0, NoFlags));
            if (destination == nullptr) throw std::runtime_error("Wii U GX2 native texture lock failed.");
            try {
                std::memset(destination, 0, surface.imageSize);
                const std::uint32_t code = layout.Format & 0x3fu;
                const bool compressed = code >= 0x31u && code <= 0x35u;
                const std::size_t rowElementBytes = compressed
                    ? (code == 0x31u || code == 0x34u ? 8u : 16u)
                    : layout.PitchBytes / layout.StorageWidth;
                const std::size_t destinationPitch = static_cast<std::size_t>(surface.pitch) * rowElementBytes;
                CopyRows(destination, surface.imageSize, 0, destinationPitch, pixels, layout.PitchBytes,
                    compressed ? layout.StorageHeight / 4 : layout.StorageHeight);
                if (layout.Format == 0x81u) {
                    CopyRows(destination, surface.imageSize, surface.mipLevelOffset[0], surface.pitch,
                        pixels + layout.UvPlaneOffset, layout.PitchBytes, layout.StorageHeight / 2);
                }
            } catch (...) {
                GX2RUnlockSurfaceEx(&surface, 0, NoFlags);
                throw;
            }
            GX2RUnlockSurfaceEx(&surface, 0, NoFlags);
            GX2RInvalidateSurface(&surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, surface.image, surface.imageSize);
            handle->Texture.viewNumMips = 1;
            handle->Texture.viewNumSlices = 1;
            handle->Texture.compMap = layout.CompMap;
            handle->LogicalWidth = layout.Width;
            handle->LogicalHeight = layout.Height;
            GX2InitTextureRegs(&handle->Texture);
            if (layout.Format == 0x81u) {
                // A NV12 descriptor's low format bits expose only Y. The UV
                // descriptor borrows this allocation and requires an explicit
                // two-plane shader; no implicit YUV-to-RGBA conversion occurs.
                if (surface.pitch < 16u || surface.pitch % 16u != 0u
                    || surface.mipLevelOffset[0] >= surface.imageSize) {
                    throw std::runtime_error("Wii U NV12 surface cannot expose a valid aligned UV texture view.");
                }
                handle->UvPlaneTexture = handle->Texture;
                GX2Surface& uv = handle->UvPlaneTexture.surface;
                uv.width = surface.width / 2u;
                uv.height = surface.height / 2u;
                uv.format = GX2_SURFACE_FORMAT_UNORM_R8_G8;
                uv.pitch = surface.pitch / 2u;
                uv.image = static_cast<std::uint8_t*>(surface.image) + surface.mipLevelOffset[0];
                uv.imageSize = surface.imageSize - surface.mipLevelOffset[0];
                std::memset(uv.mipLevelOffset, 0, sizeof(uv.mipLevelOffset));
                handle->UvPlaneTexture.compMap = 0x00010405u;
                GX2InitTextureRegs(&handle->UvPlaneTexture);
                handle->HasUvPlaneTexture = true;
            }
            const bool integerFormat = (layout.Format & 0x100u) != 0;
            GX2InitSampler(&handle->Sampler, wrap ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                integerFormat ? GX2_TEX_XY_FILTER_MODE_POINT : GX2_TEX_XY_FILTER_MODE_LINEAR);
        } catch (...) {
            DestroyWiiUTextureHandleAfterFence(handle);
            throw;
        }
    }

    void UpdateWiiURgbaTextureRegion(WiiUGx2TextureHandle* handle, std::int32_t x, std::int32_t y,
        std::int32_t width, std::int32_t height, const std::uint8_t* rgba, std::size_t byteLength,
        std::int32_t sourceRowPitch, std::uint32_t alphaPrecision, bool preserveCoverage) {
        if (handle == nullptr || handle->Texture.surface.image == nullptr || rgba == nullptr
            || handle->Texture.surface.format != GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8
            || handle->Texture.surface.tileMode != GX2_TILE_MODE_LINEAR_ALIGNED
            || x < 0 || y < 0 || width <= 0 || height <= 0 || sourceRowPitch < 0
            || static_cast<std::uint64_t>(x) + width > handle->Texture.surface.width
            || static_cast<std::uint64_t>(y) + height > handle->Texture.surface.height
            || static_cast<std::uint64_t>(sourceRowPitch) < static_cast<std::uint64_t>(width) * 4u
            || static_cast<std::uint64_t>(height - 1) * sourceRowPitch + static_cast<std::uint64_t>(width) * 4u > byteLength
            || alphaPrecision > 3 || handle->Texture.compMap != 0x00010203u) {
            throw std::invalid_argument("Wii U texture region update requires mutable RGBA8 data, valid bounds and complete source rows.");
        }
        GX2Surface& surface = handle->Texture.surface;
        const std::uint64_t lastByte = (static_cast<std::uint64_t>(y + height - 1) * surface.pitch + x + width) * 4u;
        if (surface.pitch < surface.width || lastByte > surface.imageSize) {
            throw std::runtime_error("Wii U RGBA8 texture surface does not contain the updated region.");
        }
        GX2DrawDone();
        auto* destination = static_cast<std::uint8_t*>(GX2RLockSurfaceEx(&surface, 0, NoFlags));
        if (destination == nullptr) throw std::runtime_error("Wii U texture region update could not lock the surface.");
        for (std::int32_t row = 0; row < height; ++row) {
            const auto* source = rgba + static_cast<std::size_t>(row) * sourceRowPitch;
            auto* target = destination + (static_cast<std::size_t>(y + row) * surface.pitch + x) * 4u;
            for (std::int32_t column = 0; column < width; ++column) {
                std::memcpy(target + column * 4, source + column * 4, 3);
                target[column * 4 + 3] = preserveCoverage ? source[column * 4 + 3]
                    : NormalizeAlpha(source[column * 4 + 3], alphaPrecision);
            }
        }
        GX2RUnlockSurfaceEx(&surface, 0, NoFlags);
        GX2RInvalidateSurface(&surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, surface.image, surface.imageSize);
    }

    void DestroyWiiUTextureHandleAfterFence(WiiUGx2TextureHandle* handle) {
        if (handle == nullptr) return;
        if (handle->Texture.surface.image != nullptr) GX2RDestroySurfaceEx(&handle->Texture.surface, NoFlags);
        std::memset(&handle->Texture, 0, sizeof(handle->Texture));
        std::memset(&handle->Sampler, 0, sizeof(handle->Sampler));
        std::memset(&handle->UvPlaneTexture, 0, sizeof(handle->UvPlaneTexture));
        handle->HasUvPlaneTexture = false;
        handle->LogicalWidth = 0;
        handle->LogicalHeight = 0;
    }
}
