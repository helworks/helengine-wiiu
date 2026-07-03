#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include <cstring>
#include <stdexcept>

#include <coreinit/memdefaultheap.h>
#include <gx2/display.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/swap.h>

namespace helengine::wiiu {
    namespace {
        constexpr std::uint32_t TvSurfaceWidth = 1280U;
        constexpr std::uint32_t TvSurfaceHeight = 720U;
        constexpr std::uint32_t DrcSurfaceWidth = 854U;
        constexpr std::uint32_t DrcSurfaceHeight = 480U;
        constexpr GX2SurfaceFormat PresentationSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        constexpr GX2TVRenderMode PresentationTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
    }

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
        if (IsInitialized) {
            return true;
        }

        std::uint32_t initAttributes[] = {
            GX2_INIT_ARGC, 0,
            GX2_INIT_ARGV, 0,
            GX2_INIT_END
        };
        GX2Init(initAttributes);

        GX2DRCMode drcRenderMode = GX2GetSystemDRCMode();
        std::uint32_t unusedSize = 0U;
        GX2CalcTVSize(PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &TvScanBufferSize, &unusedSize);
        GX2CalcDRCSize(drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &DrcScanBufferSize, &unusedSize);

        TvScanBuffer = MEMAllocFromDefaultHeapEx(TvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        DrcScanBuffer = MEMAllocFromDefaultHeapEx(DrcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
        if (TvScanBuffer == nullptr || DrcScanBuffer == nullptr) {
            Shutdown();
            return false;
        }

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvScanBuffer, TvScanBufferSize);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, DrcScanBuffer, DrcScanBufferSize);
        GX2SetTVBuffer(TvScanBuffer, TvScanBufferSize, PresentationTvRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);
        GX2SetDRCBuffer(DrcScanBuffer, DrcScanBufferSize, drcRenderMode, PresentationSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

        InitializeTvColorBuffer();
        InitializeDrcColorBuffer();
        GX2SetTVScale(static_cast<float>(TvSurfaceWidth), static_cast<float>(TvSurfaceHeight));
        GX2SetDRCScale(static_cast<float>(DrcSurfaceWidth), static_cast<float>(DrcSurfaceHeight));
        GX2SetSwapInterval(1);
        IsInitialized = true;
        return true;
    }

    /// Uploads and presents the supplied TV and DRC software surfaces.
    void WiiUGx2Presenter::Present(WiiUSoftwareSurface* tvSurface, WiiUSoftwareSurface* drcSurface) {
        if (tvSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid TV software surface.");
        } else if (drcSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid DRC software surface.");
        } else if (!IsInitialized) {
            throw std::runtime_error("Wii U GX2 presenter must be initialized before Present.");
        }

        UploadSurface(tvSurface, &TvColorBuffer);
        UploadSurface(drcSurface, &DrcColorBuffer);
        PresentScanBuffers();
    }

    /// Releases all allocated GX2 resources and returns the presenter to the uninitialized state.
    void WiiUGx2Presenter::Shutdown() {
        if (TvColorBuffer.surface.image != nullptr) {
            MEMFreeToDefaultHeap(TvColorBuffer.surface.image);
            TvColorBuffer.surface.image = nullptr;
        }

        if (DrcColorBuffer.surface.image != nullptr) {
            MEMFreeToDefaultHeap(DrcColorBuffer.surface.image);
            DrcColorBuffer.surface.image = nullptr;
        }

        if (TvScanBuffer != nullptr) {
            MEMFreeToDefaultHeap(TvScanBuffer);
        }

        if (DrcScanBuffer != nullptr) {
            MEMFreeToDefaultHeap(DrcScanBuffer);
        }

        if (IsInitialized) {
            GX2Shutdown();
        }

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
        std::memset(&TvColorBuffer, 0, sizeof(TvColorBuffer));
        TvColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        TvColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        TvColorBuffer.surface.width = TvSurfaceWidth;
        TvColorBuffer.surface.height = TvSurfaceHeight;
        TvColorBuffer.surface.depth = 1U;
        TvColorBuffer.surface.mipLevels = 1U;
        TvColorBuffer.surface.format = PresentationSurfaceFormat;
        TvColorBuffer.surface.aa = GX2_AA_MODE1X;
        TvColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        TvColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&TvColorBuffer.surface);
        TvColorBuffer.surface.image = MEMAllocFromDefaultHeapEx(TvColorBuffer.surface.imageSize, TvColorBuffer.surface.alignment);
        if (TvColorBuffer.surface.image == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the TV color buffer.");
        }

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, TvColorBuffer.surface.image, TvColorBuffer.surface.imageSize);
        GX2InitColorBufferRegs(&TvColorBuffer);
    }

    /// Initializes the DRC color buffer used for software-surface upload and presentation.
    void WiiUGx2Presenter::InitializeDrcColorBuffer() {
        std::memset(&DrcColorBuffer, 0, sizeof(DrcColorBuffer));
        DrcColorBuffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
        DrcColorBuffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        DrcColorBuffer.surface.width = DrcSurfaceWidth;
        DrcColorBuffer.surface.height = DrcSurfaceHeight;
        DrcColorBuffer.surface.depth = 1U;
        DrcColorBuffer.surface.mipLevels = 1U;
        DrcColorBuffer.surface.format = PresentationSurfaceFormat;
        DrcColorBuffer.surface.aa = GX2_AA_MODE1X;
        DrcColorBuffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
        DrcColorBuffer.viewNumSlices = 1U;
        GX2CalcSurfaceSizeAndAlignment(&DrcColorBuffer.surface);
        DrcColorBuffer.surface.image = MEMAllocFromDefaultHeapEx(DrcColorBuffer.surface.imageSize, DrcColorBuffer.surface.alignment);
        if (DrcColorBuffer.surface.image == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the DRC color buffer.");
        }

        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, DrcColorBuffer.surface.image, DrcColorBuffer.surface.imageSize);
        GX2InitColorBufferRegs(&DrcColorBuffer);
    }

    /// Uploads one packed ARGB8888 software surface into one GX2 color buffer image.
    void WiiUGx2Presenter::UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer) {
        if (sourceSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid source surface.");
        } else if (destinationBuffer == nullptr || destinationBuffer->surface.image == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid destination buffer.");
        }

        const std::size_t sourceByteCount =
            static_cast<std::size_t>(sourceSurface->GetWidth())
            * static_cast<std::size_t>(sourceSurface->GetHeight())
            * sizeof(std::uint32_t);
        std::memcpy(destinationBuffer->surface.image, sourceSurface->GetPixels(), sourceByteCount);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU, destinationBuffer->surface.image, destinationBuffer->surface.imageSize);
    }

    /// Presents the current TV and DRC color buffers to their scan buffers.
    void WiiUGx2Presenter::PresentScanBuffers() {
        GX2CopyColorBufferToScanBuffer(&TvColorBuffer, GX2_SCAN_TARGET_TV);
        GX2CopyColorBufferToScanBuffer(&DrcColorBuffer, GX2_SCAN_TARGET_DRC);
        GX2SwapScanBuffers();
        GX2Flush();
        GX2DrawDone();
        GX2SetTVEnable(TRUE);
        GX2SetDRCEnable(TRUE);
    }
}
