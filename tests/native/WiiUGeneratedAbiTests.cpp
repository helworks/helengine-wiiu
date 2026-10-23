#include <type_traits>
#include "platform/wiiu/WiiUContentStreamSource.hpp"
#include "platform/wiiu/audio/WiiUAudioBackend.hpp"

/// Verifies shared managed-string contracts without constructing SDK-owned voices or mounting content.
int main() {
    static_assert(!std::is_abstract_v<helengine::wiiu::WiiUAudioBackend>);
    static_assert(!std::is_abstract_v<helengine::wiiu::WiiUContentStreamSource>);
    using OpenRead = Stream* (helengine::wiiu::WiiUContentStreamSource::*)(HeCppString);
    static_assert(std::is_same_v<decltype(&helengine::wiiu::WiiUContentStreamSource::OpenRead), OpenRead>);
    return 0;
}