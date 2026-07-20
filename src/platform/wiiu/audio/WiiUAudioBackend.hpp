#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "IAudioBackend.hpp"

struct AXVoice;

namespace helengine::wiiu {
    /// <summary>
    /// Plays shared Helengine PCM assets through the Wii U AX voice mixer.
    /// </summary>
    class WiiUAudioBackend final : public ::IAudioBackend {
    public:
        /// <summary>
        /// Initializes the Wii U AX mixer and default bus gains.
        /// </summary>
        WiiUAudioBackend();

        /// <summary>
        /// Stops active voices and tears down the Wii U AX mixer.
        /// </summary>
        ~WiiUAudioBackend();

        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;

        void Stop(int32_t voiceId) override;

        void SetBusGain(std::string busId, float gain) override;

        void SetBusPaused(std::string busId, bool paused) override;

        bool IsPlaying(int32_t voiceId) override;

        void Update() override;

    private:
        static constexpr int32_t MaxChannelVoices = 2;

        struct ActiveVoiceState {
            int32_t VoiceId;
            int32_t ChannelCount;
            std::string BusId;
            float BaseGain;
            bool Looping;
            bool Paused;
            AXVoice* NativeVoices[MaxChannelVoices];
            void* ChannelBuffers[MaxChannelVoices];
            int32_t ChannelBufferByteLengths[MaxChannelVoices];
        };

        static std::string NormalizeBusId(std::string busId);

        static float ClampGain(float gain);

        static std::uint16_t ConvertGainToVolume(float gain);

        static std::uint32_t ResolveRendererSampleRate();

        float ResolveCombinedGain(const std::string& busId, float baseGain) const;

        bool IsBusPaused(const std::string& busId) const;

        void ApplyVoicePlaybackState(ActiveVoiceState& voice);

        void ReleaseVoiceState(ActiveVoiceState& voice);

        int32_t NextVoiceId;
        bool AxInitialized;
        std::unordered_map<int32_t, ActiveVoiceState> ActiveVoicesById;
        std::unordered_map<std::string, float> BusGainsById;
        std::unordered_set<std::string> PausedBusIds;
    };
}

#endif
