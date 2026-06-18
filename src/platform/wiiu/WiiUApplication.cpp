#include "platform/wiiu/WiiUApplication.hpp"

#include <cstring>

#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <whb/proc.h>

namespace helengine::wiiu {
    namespace {
        constexpr std::uint32_t StartupClearColor = 0xFF000000;
    }

    /// Creates the Wii U application with no allocated screen buffers and the startup clear color.
    WiiUApplication::WiiUApplication()
        : TvBuffer(nullptr)
        , DrcBuffer(nullptr)
        , BootPhase(WiiUBootPhase::NativeStartup)
        , ClearColor(StartupClearColor) {
    }

    /// Runs the current Wii U proof-of-life application loop until the process shuts down.
    int WiiUApplication::Run() {
        WHBProcInit();

        SetBootPhase(WiiUBootPhase::VideoInitialization, StartupClearColor);
        if (!InitializeVideo()) {
            SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
            WHBProcShutdown();
            return 1;
        }

        if (!InitializeEngineCore()) {
            SetBootPhase(WiiUBootPhase::Failed, StartupClearColor);
            OSScreenShutdown();
            WHBProcShutdown();
            return 1;
        }

        SetBootPhase(WiiUBootPhase::Running, StartupClearColor);
        while (WHBProcIsRunning()) {
            PresentFrame();
            OSSleepTicks(OSMillisecondsToTicks(16));
        }

        OSScreenShutdown();
        WHBProcShutdown();
        return 0;
    }

    /// Initializes the current OSScreen video path and allocates the display work buffers.
    bool WiiUApplication::InitializeVideo() {
        OSScreenInit();

        TvBuffer = AllocateScreenBuffer(SCREEN_TV);
        DrcBuffer = AllocateScreenBuffer(SCREEN_DRC);
        if (TvBuffer == nullptr || DrcBuffer == nullptr) {
            return false;
        }

        OSScreenSetBufferEx(SCREEN_TV, TvBuffer);
        OSScreenSetBufferEx(SCREEN_DRC, DrcBuffer);
        OSScreenEnableEx(SCREEN_TV, true);
        OSScreenEnableEx(SCREEN_DRC, true);
        return true;
    }

    /// Reserves the engine-core seam without enabling generated core integration yet.
    bool WiiUApplication::InitializeEngineCore() {
        return true;
    }

    /// Presents one frame using the current boot phase clear color on both displays.
    void WiiUApplication::PresentFrame() {
        OSScreenClearBufferEx(SCREEN_TV, ClearColor);
        OSScreenClearBufferEx(SCREEN_DRC, ClearColor);
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    /// Stores the active boot phase and clear color used by the present loop.
    void WiiUApplication::SetBootPhase(WiiUBootPhase phase, std::uint32_t color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Allocates an aligned OSScreen backing buffer for the requested display.
    void* WiiUApplication::AllocateScreenBuffer(OSScreenID screen) {
        const std::uint32_t bufferSize = OSScreenGetBufferSizeEx(screen);
        void* buffer = MEMAllocFromDefaultHeapEx(bufferSize, 0x100);
        if (buffer != nullptr) {
            std::memset(buffer, 0, bufferSize);
        }

        return buffer;
    }
}
