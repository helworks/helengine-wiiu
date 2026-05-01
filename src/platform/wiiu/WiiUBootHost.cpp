#include "platform/wiiu/WiiUBootHost.hpp"

#include <cstring>

#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <whb/proc.h>

namespace helengine::wiiu {
    namespace {
        void* TvBuffer = nullptr;
        void* DrcBuffer = nullptr;
    }

    /// Initializes the Wii U boot path and keeps the first frame visible until shutdown.
    int WiiUBootHost::Run() {
        WHBProcInit();
        OSScreenInit();

        TvBuffer = AllocateScreenBuffer(SCREEN_TV);
        DrcBuffer = AllocateScreenBuffer(SCREEN_DRC);
        if (TvBuffer == nullptr || DrcBuffer == nullptr) {
            OSScreenShutdown();
            WHBProcShutdown();
            return 1;
        }

        OSScreenSetBufferEx(SCREEN_TV, TvBuffer);
        OSScreenSetBufferEx(SCREEN_DRC, DrcBuffer);
        OSScreenEnableEx(SCREEN_TV, true);
        OSScreenEnableEx(SCREEN_DRC, true);

        while (WHBProcIsRunning()) {
            ClearScreenBuffers();
            PresentScreenBuffers();
            OSSleepTicks(OSMillisecondsToTicks(16));
        }

        OSScreenShutdown();
        WHBProcShutdown();
        return 0;
    }

    /// Allocates an aligned OSScreen backing buffer for the requested display.
    void* WiiUBootHost::AllocateScreenBuffer(OSScreenID screen) {
        const std::uint32_t bufferSize = OSScreenGetBufferSizeEx(screen);
        void* buffer = MEMAllocFromDefaultHeapEx(bufferSize, 0x100);
        if (buffer != nullptr) {
            std::memset(buffer, 0, bufferSize);
        }

        return buffer;
    }

    /// Clears the TV and GamePad work buffers to the milestone red color.
    void WiiUBootHost::ClearScreenBuffers() {
        const std::uint32_t red = BuildRedColor();
        OSScreenClearBufferEx(SCREEN_TV, red);
        OSScreenClearBufferEx(SCREEN_DRC, red);
    }

    /// Flips the TV and GamePad work buffers to the visible buffers.
    void WiiUBootHost::PresentScreenBuffers() {
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    /// Builds the OSScreen big-endian RGBX red color value.
    std::uint32_t WiiUBootHost::BuildRedColor() {
        return 0xFF000000;
    }
}
