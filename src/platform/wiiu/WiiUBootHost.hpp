#pragma once

namespace helengine::wiiu {
    /// Owns the top-level Wii U entry seam before control transfers into the application loop.
    class WiiUBootHost {
    public:
        /// Transfers control into the Wii U application seam.
        static int Run();
    };
}
