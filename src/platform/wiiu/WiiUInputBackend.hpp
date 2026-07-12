#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>

#include "IInputBackend.hpp"

class InputFrameState;
template<typename T>
class Array;
class InputGamepadState;

namespace helengine::wiiu {
    /// Captures Wii U GamePad button input and exposes it through the generated core input backend seam.
    class WiiUInputBackend final : public ::IInputBackend {
    public:
        /// Creates the Wii U input backend and allocates the reusable gamepad state buffer returned each frame.
        WiiUInputBackend();

        /// Releases the reusable gamepad state buffer owned by the backend.
        ~WiiUInputBackend();

        /// Captures the current Wii U GamePad state for one generated-core input frame.
        ::InputFrameState CaptureFrame() override;

    private:
        /// Stores the number of alternating snapshot buffers needed so previous and current input frames never alias the same gamepad array.
        static constexpr std::uint32_t SnapshotBufferCount = 2U;

        /// Stores the index of the snapshot buffer that will be filled during the next capture call.
        std::uint32_t ActiveSnapshotIndex;

        /// Stores the alternating gamepad state arrays returned through consecutive input frame captures.
        Array<::InputGamepadState>* GamepadStateSnapshots[SnapshotBufferCount];
    };
}

#endif
