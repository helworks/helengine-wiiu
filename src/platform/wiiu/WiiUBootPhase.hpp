#pragma once

namespace helengine::wiiu {
    /// Identifies the current high-level Wii U boot milestone for the first runtime seam.
    enum class WiiUBootPhase {
        NativeStartup,
        VideoInitialization,
        Running,
        Failed
    };
}
