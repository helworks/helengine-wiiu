#pragma once

#include <vector>

#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wiiu/WiiUGx2RenderFrame.hpp"

namespace helengine::wiiu {
    class WiiURuntimeModel;
    class WiiURuntimeMaterial;

    /// Stores one captured Wii U camera state consumed by the GX2 presenter.
    struct WiiUGx23DCameraState {
        /// The world-to-view transform resolved from the active scene camera.
        float4x4 ViewMatrix;

        /// The viewport bounds requested by the active scene camera.
        float4 Viewport;

        /// The active scene camera near plane distance.
        float NearPlaneDistance;

        /// The active scene camera far plane distance.
        float FarPlaneDistance;
    };

    /// Stores one captured Wii U directional light state consumed by the GX2 presenter.
    struct WiiUGx23DDirectionalLightState {
        /// The linear directional-light radiance captured for the current frame.
        float4 Color;

        /// The world-space light direction captured for the current frame.
        float4 Direction;
    };

    /// Stores one captured Wii U 3D draw command consumed by the GX2 presenter.
    struct WiiUGx23DDrawCommand {
        /// The runtime model resolved by the shared engine for this drawable submission.
        const WiiURuntimeModel* RuntimeModel;

        /// The runtime material resolved by the shared engine for this drawable submission.
        const WiiURuntimeMaterial* RuntimeMaterial;

        /// The world transform resolved from the drawable owner entity.
        float4x4 WorldMatrix;
    };

    /// Stores one full Wii U 3D frame captured from the generated-core scene state.
    class WiiUGx23DRenderFrame {
    public:
        /// Creates one empty captured 3D frame with an opaque black clear color.
        WiiUGx23DRenderFrame()
            : ClearColor { 0U, 0U, 0U, 255U }
            , HasCameraState(false)
            , CameraState()
            , AmbientLightColor(0.0f, 0.0f, 0.0f, 0.0f)
            , HasDirectionalLightState(false)
            , DirectionalLightState()
            , DrawCommands() {
        }

        /// Resets the captured 3D frame to its default empty state.
        void Clear() {
            ClearColor = WiiUGx2Color { 0U, 0U, 0U, 255U };
            HasCameraState = false;
            CameraState = WiiUGx23DCameraState();
            AmbientLightColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
            HasDirectionalLightState = false;
            DirectionalLightState = WiiUGx23DDirectionalLightState();
            DrawCommands.clear();
        }

        /// Stores the clear color that should be used before rendering 3D geometry.
        void SetClearColor(WiiUGx2Color color) {
            ClearColor = color;
        }

        /// Returns the clear color captured for the current frame.
        const WiiUGx2Color& GetClearColor() const {
            return ClearColor;
        }

        /// Stores the active camera state used for the current frame.
        void SetCamera(const WiiUGx23DCameraState& cameraState) {
            CameraState = cameraState;
            HasCameraState = true;
        }

        /// Returns whether the current frame captured one active camera.
        bool GetHasCamera() const {
            return HasCameraState;
        }

        /// Returns the captured camera state for the current frame.
        const WiiUGx23DCameraState& GetCamera() const {
            return CameraState;
        }

        /// Stores the accumulated ambient light color captured for the current frame.
        void SetAmbientLightColor(float4 color) {
            AmbientLightColor = color;
        }

        /// Returns the accumulated ambient light color captured for the current frame.
        const float4& GetAmbientLightColor() const {
            return AmbientLightColor;
        }

        /// Stores the first directional light captured for the current frame.
        void SetDirectionalLight(const WiiUGx23DDirectionalLightState& directionalLightState) {
            DirectionalLightState = directionalLightState;
            HasDirectionalLightState = true;
        }

        /// Returns whether one directional light was captured for the current frame.
        bool GetHasDirectionalLight() const {
            return HasDirectionalLightState;
        }

        /// Returns the first directional light captured for the current frame.
        const WiiUGx23DDirectionalLightState& GetDirectionalLight() const {
            return DirectionalLightState;
        }

        /// Appends one draw command in render order.
        void AddDrawCommand(const WiiUGx23DDrawCommand& drawCommand) {
            DrawCommands.push_back(drawCommand);
        }

        /// Returns the captured draw commands in render order.
        const std::vector<WiiUGx23DDrawCommand>& GetDrawCommands() const {
            return DrawCommands;
        }

    private:
        /// Stores the clear color used before drawing 3D geometry.
        WiiUGx2Color ClearColor;

        /// Tracks whether one active camera state was captured.
        bool HasCameraState;

        /// Stores the active scene camera resolved for the current frame.
        WiiUGx23DCameraState CameraState;

        /// Stores the accumulated ambient light color captured for the current frame.
        float4 AmbientLightColor;

        /// Tracks whether the current frame captured one directional light.
        bool HasDirectionalLightState;

        /// Stores the first directional light captured for the current frame.
        WiiUGx23DDirectionalLightState DirectionalLightState;

        /// Stores captured 3D draw commands in render order.
        std::vector<WiiUGx23DDrawCommand> DrawCommands;
    };
}
