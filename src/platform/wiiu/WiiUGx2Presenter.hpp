#pragma once

#include <cstdint>

#include <gx2/display.h>
#include <gx2/surface.h>

#include "platform/wiiu/WiiUSoftwareSurface.hpp"

namespace helengine::wiiu {
    /// Owns the minimal GX2 presentation seam that uploads CPU-rendered TV and DRC software surfaces into GX2-owned display buffers.
    class WiiUGx2Presenter {
    public:
        /// Creates one uninitialized GX2 presenter.
        WiiUGx2Presenter();

        /// Releases all GX2-owned presentation resources.
        ~WiiUGx2Presenter();

        /// Initializes the GX2 presentation path for TV and DRC output.
        bool Initialize();

        /// Uploads and presents the supplied TV and DRC software surfaces.
        void Present(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface);

    private:
        /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
        void Shutdown();

        /// Initializes the TV color buffer used for software-surface upload and presentation.
        void InitializeTvColorBuffer();

        /// Initializes the DRC color buffer used for software-surface upload and presentation.
        void InitializeDrcColorBuffer();

        /// Uploads one packed ARGB8888 software surface into one GX2 color buffer image.
        void UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer);

        /// Presents the current TV and DRC color buffers to their scan buffers.
        void PresentScanBuffers();

        /// Tracks whether GX2 resources were initialized successfully.
        bool IsInitialized;

        /// Stores the TV scan buffer pointer returned by the GX2 allocation path.
        void* TvScanBuffer;

        /// Stores the DRC scan buffer pointer returned by the GX2 allocation path.
        void* DrcScanBuffer;

        /// Stores the TV color buffer used for steady-state presentation.
        GX2ColorBuffer TvColorBuffer;

        /// Stores the DRC color buffer used for steady-state presentation.
        GX2ColorBuffer DrcColorBuffer;

        /// Stores the raw scan-buffer size required for TV presentation.
        std::uint32_t TvScanBufferSize;

        /// Stores the raw scan-buffer size required for DRC presentation.
        std::uint32_t DrcScanBufferSize;
    };
}
