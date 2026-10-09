#pragma once

#include <cstring>
#include <cstdint>

#include <gx2/sampler.h>
#include <gx2/texture.h>

namespace helengine::wiiu {
    /// Stores one GX2 texture and sampler pair prepared for Wii U 2D rendering.
    class WiiUGx2TextureHandle {
    public:
        /// Creates one empty GX2 texture handle with zeroed GX2 state.
        WiiUGx2TextureHandle()
            : Texture()
            , Sampler() {
            std::memset(&Texture, 0, sizeof(Texture));
            std::memset(&Sampler, 0, sizeof(Sampler));
            std::memset(&UvPlaneTexture, 0, sizeof(UvPlaneTexture));
        }

        /// Stores the GX2 texture object sampled by the pure GX2 UI presenter.
        GX2Texture Texture;

        /// Stores the GX2 sampler state paired with the texture object.
        GX2Sampler Sampler;
        /// Exposes the native NV12 chroma view for shaders with an explicit two-plane YUV contract; it borrows the main allocation.
        GX2Texture UvPlaneTexture {};
        /// Gets whether this handle exposes both Y and interleaved UV planes rather than a normalized RGBA image.
        bool HasUvPlaneTexture = false;
        /// Preserves real sampling extents when an NV12 allocation requires even padded storage dimensions.
        std::uint32_t LogicalWidth = 0, LogicalHeight = 0;
    };
}
