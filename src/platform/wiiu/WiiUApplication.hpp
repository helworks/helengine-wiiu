#pragma once

#include <cstdint>

#include <coreinit/screen.h>

#include "platform/wiiu/WiiUBootPhase.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE
class Core;
class HostFileSystemContentStreamSource;
class PlatformInfo;
class StandardPlatformInputConfiguration;
#endif

namespace helengine::wiiu {
    class WiiUGx2Presenter;

#if HELENGINE_WIIU_HAS_GENERATED_CORE
    class WiiUInputBackend;
    class WiiURenderManager2D;
    class WiiURenderManager3D;
#endif

    /// Owns the first Wii U steady-state application seam after the boot host transfers control.
    class WiiUApplication {
    public:
        /// Creates the Wii U application with no allocated screen buffers and the startup clear color.
        WiiUApplication();

        /// Releases generated-core bridge objects and native screen buffers after the application loop finishes.
        ~WiiUApplication();

        /// Runs the current Wii U proof-of-life application loop until the process shuts down.
        int Run();

    private:
        /// Initializes the current OSScreen video path and allocates the display work buffers.
        bool InitializeVideo();

        /// Initializes the Wii U generated core and queues the packaged startup scene.
        bool InitializeEngineCore();

        /// Initializes the Wii U GX2 presenter used for steady-state rendered output.
        bool InitializeGx2Presenter();

        /// Builds the generated standard-platform input configuration emitted by the Wii U builder so gameplay Accept and Return actions work at runtime.
        StandardPlatformInputConfiguration* CreateStandardPlatformInputConfiguration() const;

        /// Advances one generated-core update tick for the packaged Wii U runtime.
        bool UpdateEngineCore();

        /// Draws one generated-core frame for the packaged Wii U runtime.
        bool DrawEngineCore();

        /// Presents one frame using the current boot phase clear color on both displays.
        void PresentFrame();

        /// Presents one boot-phase frame using the current diagnostic clear color on both displays.
        void PresentBootPhaseFrame();

        /// Presents one renderer-owned frame after the generated core has initialized.
        void PresentRenderedFrame();

        /// Appends one host-readable Wii U runtime trace line to every supported trace sink.
        void AppendRuntimeTrace(const char* format, ...);

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

        /// Stores the GX2 presenter used for steady-state software-surface presentation.
        WiiUGx2Presenter* Gx2Presenter;

#if HELENGINE_WIIU_HAS_GENERATED_CORE
        /// Tracks whether the generated core initialized far enough to enter the steady-state frame loop.
        bool EngineInitialized;

        /// Stores the generated core instance that owns scene loading and frame updates.
        Core* EngineCore;

        /// Stores the Wii U 3D render bridge used by the generated core.
        WiiURenderManager3D* EngineRenderManager3D;

        /// Stores the Wii U 2D render bridge used by the generated core.
        WiiURenderManager2D* EngineRenderManager2D;

        /// Stores the Wii U input backend used by the generated core.
        WiiUInputBackend* EngineInputBackend;

        /// Stores the current Wii U platform information record exposed to gameplay code.
        PlatformInfo* EnginePlatformInfo;

        /// Stores the content stream source that backs core-owned runtime asset reads for the packaged Wii U build.
        HostFileSystemContentStreamSource* EngineContentStreamSource;
#endif
    };
}
