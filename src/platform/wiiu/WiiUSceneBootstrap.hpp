#pragma once

#include <string>

class RuntimeSceneCatalog;

namespace helengine::wiiu {
    /// Declares the packaged Wii U startup scene and content-root helpers consumed by the application seam.
    class WiiUSceneBootstrap {
    public:
        /// Returns the packaged Wii U content root used by content-backed startup.
        static std::string GetPackagedContentRootPath();

        /// Creates the packaged runtime scene catalog emitted by the Wii U builder.
        static RuntimeSceneCatalog* CreatePackagedSceneCatalog();

        /// Returns the packaged startup scene id emitted by the Wii U builder.
        static std::string GetPackagedStartupSceneId();
    };
}
