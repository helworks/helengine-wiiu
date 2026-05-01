#pragma once

#include <cstdint>

#include <coreinit/screen.h>

namespace helengine::wiiu {
    /// Owns the first Wii U native screen bootstrap and red-frame presentation loop.
    class WiiUBootHost {
    public:
        /// Initializes the Wii U boot path and keeps the first frame visible until shutdown.
        static int Run();

    private:
        /// Allocates an aligned OSScreen backing buffer for the requested display.
        static void* AllocateScreenBuffer(OSScreenID screen);

        /// Clears the TV and GamePad work buffers to the milestone red color.
        static void ClearScreenBuffers();

        /// Flips the TV and GamePad work buffers to the visible buffers.
        static void PresentScreenBuffers();

        /// Builds the OSScreen big-endian RGBX red color value.
        static std::uint32_t BuildRedColor();
    };
}
