#pragma once

#include <cstdint>

#include <coreinit/screen.h>

#include "platform/wiiu/WiiUBootPhase.hpp"

namespace helengine::wiiu {
    /// Owns the first Wii U steady-state application seam after the boot host transfers control.
    class WiiUApplication {
    public:
        /// Creates the Wii U application with no allocated screen buffers and the startup clear color.
        WiiUApplication();

        /// Runs the current Wii U proof-of-life application loop until the process shuts down.
        int Run();

    private:
        /// Initializes the current OSScreen video path and allocates the display work buffers.
        bool InitializeVideo();

        /// Reserves the engine-core seam without enabling generated core integration yet.
        bool InitializeEngineCore();

        /// Presents one frame using the current boot phase clear color on both displays.
        void PresentFrame();

        /// Stores the active boot phase and clear color used by the present loop.
        void SetBootPhase(WiiUBootPhase phase, std::uint32_t color);

        /// Allocates an aligned OSScreen backing buffer for the requested display.
        void* AllocateScreenBuffer(OSScreenID screen);

        /// Stores the allocated TV screen backing buffer.
        void* TvBuffer;

        /// Stores the allocated GamePad screen backing buffer.
        void* DrcBuffer;

        /// Stores the current boot phase for the application loop.
        WiiUBootPhase BootPhase;

        /// Stores the active clear color for frame presentation.
        std::uint32_t ClearColor;
    };
}
