#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace helengine::wiiu {
    /// Resolves integrity probes from the selected runtime scene catalog, including its declared startup scene.
    class WiiUPackagedSceneProbePaths {
    public:
        /// Validates the emitted catalog and returns its selected cooked paths without adding unselected demo assets.
        template<typename SceneEntry>
        static std::vector<std::string> Resolve(const char* startupSceneId, const SceneEntry* entries, std::size_t count) {
            if (startupSceneId == nullptr || startupSceneId[0] == '\0' || entries == nullptr || count == 0U) {
                throw std::logic_error("The packaged scene probe catalog was incomplete.");
            }
            std::vector<std::string> paths;
            paths.reserve(count);
            bool startupFound = false;
            for (std::size_t index = 0; index < count; index++) {
                const SceneEntry& entry = entries[index];
                if (entry.SceneId == nullptr || entry.SceneId[0] == '\0' || entry.CookedRelativePath == nullptr || entry.CookedRelativePath[0] == '\0') {
                    throw std::logic_error("The packaged scene probe entry was incomplete.");
                }
                startupFound = startupFound || std::string(entry.SceneId) == startupSceneId;
                paths.emplace_back(entry.CookedRelativePath);
            }
            if (!startupFound) {
                throw std::logic_error("The packaged startup scene was absent from the selected catalog.");
            }
            return paths;
        }
    };
}
