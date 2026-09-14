#include "platform/wiiu/WiiUContentStreamSource.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include "HostFileSystemContentStreamSource.hpp"

namespace helengine::wiiu {
    namespace {
        constexpr const char* CanonicalStandardMaterialPath = "cooked/engine/materials/standard.hasset";
        constexpr const char* PackagedStandardMaterialAliasPath = "wiiu_standard_material.hasset";
    }

    /// <summary>
    /// Creates a Wii U content source rooted at the packaged content volume.
    /// </summary>
    /// <param name="contentRootPath">Wii U virtual content root.</param>
    WiiUContentStreamSource::WiiUContentStreamSource(const std::string& contentRootPath)
        : HostSource(new HostFileSystemContentStreamSource(contentRootPath)) {
    }

    /// <summary>
    /// Releases the generated host source owned by this adapter.
    /// </summary>
    WiiUContentStreamSource::~WiiUContentStreamSource() {
        delete HostSource;
        HostSource = nullptr;
    }

    /// <summary>
    /// Opens a packaged asset while mapping the canonical Standard material path to its Wii U package alias.
    /// </summary>
    /// <param name="assetPath">Canonical cooked-relative asset path.</param>
    /// <returns>Readable stream for the mapped package path.</returns>
    Stream* WiiUContentStreamSource::OpenRead(std::string assetPath) {
        if (assetPath == CanonicalStandardMaterialPath) {
            assetPath = PackagedStandardMaterialAliasPath;
        }

        return HostSource->OpenRead(assetPath);
    }
}
#endif
