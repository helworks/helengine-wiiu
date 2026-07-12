#include "platform/wiiu/WiiUGx2Presenter.hpp"

#include <cstring>
#include <stdexcept>

#include <coreinit/memdefaultheap.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <gx2/temp.h>
#include <gx2r/resource.h>
#include <gx2r/surface.h>

namespace helengine::wiiu {
    namespace {
        constexpr std::uint32_t TvSurfaceWidth = 1280U;
        constexpr std::uint32_t TvSurfaceHeight = 720U;
        constexpr std::uint32_t DrcSurfaceWidth = 854U;
        constexpr std::uint32_t DrcSurfaceHeight = 480U;
        constexpr GX2SurfaceFormat PresentationSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        constexpr GX2TVRenderMode PresentationTvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
        constexpr GX2RResourceFlags NoGx2rResourceFlags = static_cast<GX2RResourceFlags>(0);
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

        GX2DrcRenderMode drcRenderMode = GX2GetSystemDRCMode();
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
        GX2SetTVScale(TvSurfaceWidth, TvSurfaceHeight);
        GX2SetDRCScale(DrcSurfaceWidth, DrcSurfaceHeight);
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
            GX2RDestroySurfaceEx(&TvColorBuffer.surface, NoGx2rResourceFlags);
            TvColorBuffer.surface.image = nullptr;
        }

        if (DrcColorBuffer.surface.image != nullptr) {
            GX2RDestroySurfaceEx(&DrcColorBuffer.surface, NoGx2rResourceFlags);
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
        if (!GX2RCreateSurface(
            &TvColorBuffer.surface,
            GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the TV color buffer.");
        }

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
        if (!GX2RCreateSurface(
            &DrcColorBuffer.surface,
            GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ)) {
            throw std::runtime_error("Wii U GX2 presenter could not allocate the DRC color buffer.");
        }

        GX2InitColorBufferRegs(&DrcColorBuffer);
    }

    /// Uploads one packed ARGB8888 software surface into one GX2 color buffer image.
    void WiiUGx2Presenter::UploadSurface(WiiUSoftwareSurface* sourceSurface, GX2ColorBuffer* destinationBuffer) {
        if (sourceSurface == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid source surface.");
        } else if (destinationBuffer == nullptr || destinationBuffer->surface.image == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter requires a valid destination buffer.");
        }

        std::uint32_t* destinationPixels = static_cast<std::uint32_t*>(GX2RLockSurfaceEx(&destinationBuffer->surface, 0, NoGx2rResourceFlags));
        if (destinationPixels == nullptr) {
            throw std::runtime_error("Wii U GX2 presenter could not lock the destination color buffer surface.");
        }

        const std::uint32_t* sourcePixels = sourceSurface->GetPixels();
        const std::uint32_t sourceWidth = sourceSurface->GetWidth();
        const std::uint32_t sourceHeight = sourceSurface->GetHeight();
        const std::uint32_t destinationPitch = destinationBuffer->surface.pitch;

        for (std::uint32_t row = 0U; row < sourceHeight; row++) {
            for (std::uint32_t column = 0U; column < sourceWidth; column++) {
                std::uint32_t sourcePixel = sourcePixels[(row * sourceWidth) + column];
                destinationPixels[(row * destinationPitch) + column] = (sourcePixel << 8) | (sourcePixel >> 24);
            }
        }

        GX2RUnlockSurfaceEx(&destinationBuffer->surface, 0, NoGx2rResourceFlags);
        GX2RInvalidateSurface(&destinationBuffer->surface, 0, GX2R_RESOURCE_USAGE_CPU_WRITE);
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
