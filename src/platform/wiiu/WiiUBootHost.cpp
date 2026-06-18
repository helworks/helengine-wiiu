#include "platform/wiiu/WiiUBootHost.hpp"
#include "platform/wiiu/WiiUApplication.hpp"

namespace helengine::wiiu {
    /// Transfers control into the Wii U application seam.
    int WiiUBootHost::Run() {
        WiiUApplication application {};
        return application.Run();
    }
}
