#include "platform/wiiu/WiiUSceneBootstrap.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"
#include "runtime/wiiu_runtime_scene_manifest.hpp"
#endif

namespace helengine::wiiu {
    namespace {
        std::string PackagedContentRootPath = "fs:/vol/content";
        std::string PackagedStartupSceneId = "Scenes/rendering/textured_cube_grid.helen";
        std::string PackagedStartupSceneCookedRelativePath = "cooked/scenes/rendering/textured_cube_grid.hasset";
    }

    /// Returns the packaged Wii U content root used by content-backed startup.
    std::string WiiUSceneBootstrap::GetPackagedContentRootPath() {
        return PackagedContentRootPath;
    }

    /// Creates the packaged runtime scene catalog emitted by the Wii U builder.
    RuntimeSceneCatalog* WiiUSceneBootstrap::CreatePackagedSceneCatalog() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        std::size_t entryCount = 0;
        const HEWiiURuntimeSceneEntry* entries = he_get_runtime_wiiu_scene_entries(&entryCount);
        Array<RuntimeSceneCatalogEntry*>* runtimeEntries = new Array<RuntimeSceneCatalogEntry*>(static_cast<int32_t>(entryCount));
        for (std::size_t index = 0; index < entryCount; index++) {
            (*runtimeEntries)[static_cast<int32_t>(index)] = new RuntimeSceneCatalogEntry(entries[index].SceneId, entries[index].CookedRelativePath);
        }

        return new RuntimeSceneCatalog(runtimeEntries);
#else
        return nullptr;
#endif
    }

    /// Returns the packaged startup scene id emitted by the Wii U builder.
    std::string WiiUSceneBootstrap::GetPackagedStartupSceneId() {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
        return he_get_runtime_wiiu_startup_scene_id();
#else
        return PackagedStartupSceneId;
#endif
    }
}
