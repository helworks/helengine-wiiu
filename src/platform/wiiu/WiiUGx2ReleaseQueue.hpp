#pragma once

#include <algorithm>
#include <vector>
#include <gx2/event.h>

namespace helengine::wiiu {
    /// Retains released resources referenced by captured GX2 commands until one completed GPU fence.
    template<class T> class WiiUGx2ReleaseQueue {
    public:
        /// Adds a release request once so repeated disposal never destroys a resource twice.
        void Add(T* resource) {
            if (!Contains(resource)) Pending.push_back(resource);
        }
        /// Checks whether a resource is no longer eligible for updates or new draw requests.
        bool Contains(T* resource) const {
            return std::find(Pending.begin(), Pending.end(), resource) != Pending.end();
        }
        /// Completes earlier submissions before invoking the actual resource destruction for each retained request.
        template<class TRelease> void Drain(TRelease release) {
            if (Pending.empty()) return;
            GX2DrawDone();
            for (T* resource : Pending) release(resource);
            Pending.clear();
        }
    private:
        /// Stores borrowed resources whose destruction is delegated to their owning renderer after the fence.
        std::vector<T*> Pending;
    };
}
