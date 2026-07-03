#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include <cstring>
#include <stdexcept>

namespace helengine::wiiu {
    /// Creates one uninitialized GX2 presenter.
    WiiUGx2Presenter::WiiUGx2Presenter()
        : IsInitialized(false)
        , TvScanBuffer(nullptr)
        , DrcScanBuffer(nullptr)
        , TvColorBuffer()
        , DrcColorBuffer()
        , TvScanBufferSize(0U)
        , DrcScanBufferSize(0U) {
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
    }

    /// Releases all GX2-owned presentation resources.
    WiiUGx2Presenter::~WiiUGx2Presenter() {
        Shutdown();
    }

    /// Initializes the GX2 presentation path for TV and DRC output.
    bool WiiUGx2Presenter::Initialize() {
        return false;
    }

    /// Uploads and presents the supplied TV and DRC software surfaces.
    void WiiUGx2Presenter::Present(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface) {
        if (tvSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid TV software surface.");
        } else if (drcSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid DRC software surface.");
        }
    }

    /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
    void WiiUGx2Presenter::Shutdown() {
        IsInitialized = false;
        TvScanBuffer = nullptr;
        DrcScanBuffer = nullptr;
        TvScanBufferSize = 0U;
        DrcScanBufferSize = 0U;
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
    }

    /// Initializes the TV color buffer used for software-surface upload and presentation.
    void WiiUGx2Presenter::InitializeTvColorBuffer() {
    }

    /// Initializes the DRC color buffer used for software-surface upload and presentation.
    void WiiUGx2Presenter::InitializeDrcColorBuffer() {
    }

    /// Uploads one packed ARGB8888 software surface into one GX2 color buffer image.
    void WiiUGx2Presenter::UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer) {
        if (sourceSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid source surface.");
        } else if (destinationBuffer == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid destination buffer.");
        }
    }

    /// Presents the current TV and DRC color buffers to their scan buffers.
    void WiiUGx2Presenter::PresentScanBuffers() {
    }
}
