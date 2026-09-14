#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include <string>

#include "IContentStreamSource.hpp"

class HostFileSystemContentStreamSource;

namespace helengine::wiiu {
    /// <summary>
    /// Resolves Wii U packaged content through the host source while preserving the canonical engine path contract.
    /// </summary>
    class WiiUContentStreamSource final : public ::IContentStreamSource {
    public:
        /// <summary>
        /// Creates a Wii U content source rooted at the packaged content volume.
        /// </summary>
        /// <param name="contentRootPath">Wii U virtual content root.</param>
        explicit WiiUContentStreamSource(const std::string& contentRootPath);

        /// <summary>
        /// Releases the borrowed host source owned by this adapter.
        /// </summary>
        ~WiiUContentStreamSource() override;

        /// <summary>
        /// Opens a packaged asset while mapping the canonical Standard material path to its Wii U package alias.
        /// </summary>
        /// <param name="assetPath">Canonical cooked-relative asset path.</param>
        /// <returns>Readable stream for the mapped package path.</returns>
        Stream* OpenRead(std::string assetPath) override;

    private:
        /// <summary>
        /// Stores the generated host source used for ordinary Wii U paths.
        /// </summary>
        HostFileSystemContentStreamSource* HostSource;
    };
}
#endif
