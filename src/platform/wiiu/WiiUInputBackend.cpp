#include "platform/wiiu/WiiUInputBackend.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>

#include <vpad/input.h>

#include "ButtonState.hpp"
#include "InputFrameState.hpp"
#include "InputGamepadButton.hpp"
#include "InputGamepadState.hpp"
#include "KeyboardState.hpp"
#include "MouseState.hpp"
#include "runtime/array.hpp"

namespace helengine::wiiu {
    namespace {
        /// Applies one native Wii U held-button bit to one generated-core gamepad button.
        void ApplyButtonState(VPADStatus status, std::uint32_t nativeButton, InputGamepadState& gamepadState, InputGamepadButton button) {
            gamepadState.SetButtonDown(button, (status.hold & nativeButton) != 0U);
        }
    }

    /// Creates the Wii U input backend and allocates the reusable gamepad state buffer returned each frame.
    WiiUInputBackend::WiiUInputBackend()
        : ActiveSnapshotIndex(SnapshotBufferCount - 1U)
        , GamepadStateSnapshots {
            new Array<InputGamepadState>(1),
            new Array<InputGamepadState>(1)
        } {
        for (std::uint32_t snapshotIndex = 0; snapshotIndex < SnapshotBufferCount; snapshotIndex++) {
            (*GamepadStateSnapshots[snapshotIndex])[0].set_Connected(false);
        }
    }

    /// Releases the reusable gamepad state buffer owned by the backend.
    WiiUInputBackend::~WiiUInputBackend() {
        for (std::uint32_t snapshotIndex = 0; snapshotIndex < SnapshotBufferCount; snapshotIndex++) {
            delete GamepadStateSnapshots[snapshotIndex];
            GamepadStateSnapshots[snapshotIndex] = nullptr;
        }
    }

    /// Captures the current Wii U GamePad state for one generated-core input frame.
    InputFrameState WiiUInputBackend::CaptureFrame() {
        VPADStatus status {};
        VPADReadError error = VPAD_READ_UNINITIALIZED;
        const int32_t readCount = VPADRead(VPAD_CHAN_0, &status, 1, &error);

        InputGamepadState gamepadState {};
        const bool isConnected = readCount > 0 && error == VPAD_READ_SUCCESS;
        gamepadState.set_Connected(isConnected);

        if (isConnected) {
            ApplyButtonState(status, VPAD_BUTTON_A, gamepadState, InputGamepadButton::South);
            ApplyButtonState(status, VPAD_BUTTON_B, gamepadState, InputGamepadButton::East);
            ApplyButtonState(status, VPAD_BUTTON_X, gamepadState, InputGamepadButton::West);
            ApplyButtonState(status, VPAD_BUTTON_Y, gamepadState, InputGamepadButton::North);
            ApplyButtonState(status, VPAD_BUTTON_L, gamepadState, InputGamepadButton::LeftShoulder);
            ApplyButtonState(status, VPAD_BUTTON_R, gamepadState, InputGamepadButton::RightShoulder);
            ApplyButtonState(status, VPAD_BUTTON_ZL, gamepadState, InputGamepadButton::LeftTrigger);
            ApplyButtonState(status, VPAD_BUTTON_ZR, gamepadState, InputGamepadButton::RightTrigger);
            ApplyButtonState(status, VPAD_BUTTON_STICK_L, gamepadState, InputGamepadButton::LeftStick);
            ApplyButtonState(status, VPAD_BUTTON_STICK_R, gamepadState, InputGamepadButton::RightStick);
            ApplyButtonState(status, VPAD_BUTTON_UP, gamepadState, InputGamepadButton::DPadUp);
            ApplyButtonState(status, VPAD_BUTTON_DOWN, gamepadState, InputGamepadButton::DPadDown);
            ApplyButtonState(status, VPAD_BUTTON_LEFT, gamepadState, InputGamepadButton::DPadLeft);
            ApplyButtonState(status, VPAD_BUTTON_RIGHT, gamepadState, InputGamepadButton::DPadRight);
            ApplyButtonState(status, VPAD_BUTTON_PLUS, gamepadState, InputGamepadButton::Start);
            ApplyButtonState(status, VPAD_BUTTON_MINUS, gamepadState, InputGamepadButton::Select);
            ApplyButtonState(status, VPAD_BUTTON_HOME, gamepadState, InputGamepadButton::Home);
            gamepadState.set_LeftStickX(static_cast<int16_t>(status.leftStick.x * 32767.0f));
            gamepadState.set_LeftStickY(static_cast<int16_t>(-status.leftStick.y * 32767.0f));
        }

        ActiveSnapshotIndex = (ActiveSnapshotIndex + 1U) % SnapshotBufferCount;
        Array<InputGamepadState>* gamepadStates = GamepadStateSnapshots[ActiveSnapshotIndex];
        (*gamepadStates)[0] = gamepadState;

        InputFrameState frameState {};
        frameState.set_Gamepads(gamepadStates);
        frameState.set_GamepadCount(1);
        frameState.set_Keyboard(KeyboardState());
        frameState.set_Mouse(MouseState());
        return frameState;
    }
}

#endif
