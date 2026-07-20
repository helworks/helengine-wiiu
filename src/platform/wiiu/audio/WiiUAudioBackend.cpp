#include "platform/wiiu/audio/WiiUAudioBackend.hpp"

#if HELENGINE_WIIU_HAS_GENERATED_CORE

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/voice.h>

namespace helengine::wiiu {
    namespace {
        constexpr int32_t RequiredAudioAlignment = 0x100;
        constexpr std::uint16_t MaxVoiceVolume = 0x8000;
        constexpr std::uint32_t VoicePriority = 31;
        constexpr int32_t DeviceMixChannelCount = 6;
        constexpr int32_t PrimaryMixBusIndex = 0;

        std::int16_t ReadLittleEndianPcm16Sample(const std::uint8_t* sourceBytes) {
            if (sourceBytes == nullptr) {
                return 0;
            }

            const std::uint16_t unsignedSample =
                static_cast<std::uint16_t>(sourceBytes[0])
                | (static_cast<std::uint16_t>(sourceBytes[1]) << 8);
            return static_cast<std::int16_t>(unsignedSample);
        }

        void PopulateDeviceMixData(
            AXVoiceDeviceMixData (&mixData)[DeviceMixChannelCount],
            int32_t assetChannelCount,
            int32_t voiceChannelIndex) {
            std::memset(mixData, 0, sizeof(mixData));

            if (assetChannelCount <= 1) {
                mixData[0].bus[PrimaryMixBusIndex].volume = MaxVoiceVolume;
                mixData[1].bus[PrimaryMixBusIndex].volume = MaxVoiceVolume;
                return;
            }

            mixData[voiceChannelIndex].bus[PrimaryMixBusIndex].volume = MaxVoiceVolume;
        }

        void ConfigureVoiceDeviceMix(AXVoice* nativeVoice, int32_t assetChannelCount, int32_t voiceChannelIndex) {
            AXVoiceDeviceMixData tvMixData[DeviceMixChannelCount] = {};
            AXVoiceDeviceMixData drcMixData[DeviceMixChannelCount] = {};
            PopulateDeviceMixData(tvMixData, assetChannelCount, voiceChannelIndex);
            PopulateDeviceMixData(drcMixData, assetChannelCount, voiceChannelIndex);

            if (AXSetVoiceDeviceMix(nativeVoice, AX_DEVICE_TYPE_TV, 0, tvMixData) != AX_RESULT_SUCCESS) {
                throw std::runtime_error("Wii U audio playback could not configure the TV device mix.");
            }

            if (AXSetVoiceDeviceMix(nativeVoice, AX_DEVICE_TYPE_DRC, 0, drcMixData) != AX_RESULT_SUCCESS) {
                throw std::runtime_error("Wii U audio playback could not configure the DRC device mix.");
            }
        }
    }

    WiiUAudioBackend::WiiUAudioBackend()
        : NextVoiceId(0)
        , AxInitialized(false)
        , ActiveVoicesById()
        , BusGainsById()
        , PausedBusIds() {
        BusGainsById.emplace("master", 1.0f);
        BusGainsById.emplace("music", 1.0f);
        BusGainsById.emplace("sfx", 1.0f);

        if (!AXIsInit()) {
            AXInitParams initParams = {};
            initParams.renderer = AX_INIT_RENDERER_48KHZ;
            initParams.pipeline = AX_INIT_PIPELINE_SINGLE;
            AXInitWithParams(&initParams);
            AxInitialized = true;
        }
    }

    WiiUAudioBackend::~WiiUAudioBackend() {
        for (auto& voiceEntry : ActiveVoicesById) {
            ReleaseVoiceState(voiceEntry.second);
        }

        ActiveVoicesById.clear();
        if (AxInitialized && AXIsInit()) {
            AXQuit();
        }
    }

    int32_t WiiUAudioBackend::Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }
        if (asset->SampleRate <= 0) {
            throw std::runtime_error("Wii U audio playback requires a positive sample rate.");
        }
        if (asset->EncodingFamilyId != "pcm-streamed") {
            throw std::runtime_error("Wii U audio playback currently requires shared pcm-streamed assets.");
        }
        if (asset->Channels <= 0 || asset->Channels > MaxChannelVoices) {
            throw std::runtime_error("Wii U audio playback currently supports only mono or stereo 16-bit PCM assets.");
        }
        if (asset->EncodedBytes == nullptr || asset->EncodedBytes->Length <= 0 || asset->EncodedBytes->Data == nullptr) {
            throw std::runtime_error("Wii U audio playback requires one non-empty encoded payload.");
        }
        if ((asset->EncodedBytes->Length % static_cast<int32_t>(sizeof(std::int16_t) * asset->Channels)) != 0) {
            throw std::runtime_error("Wii U audio playback requires 16-bit PCM sample alignment.");
        }

        const int32_t sampleFrameCount =
            asset->EncodedBytes->Length / static_cast<int32_t>(sizeof(std::int16_t) * asset->Channels);
        if (sampleFrameCount <= 0) {
            throw std::runtime_error("Wii U audio playback requires one non-empty PCM frame payload.");
        }

        ActiveVoiceState voice = {};
        voice.VoiceId = NextVoiceId++;
        voice.ChannelCount = asset->Channels;
        voice.BusId = NormalizeBusId(
            request != nullptr && !request->BusId.empty()
                ? request->BusId
                : asset->DefaultBusId);
        voice.BaseGain = ClampGain(request != nullptr ? request->Gain : 1.0f);
        voice.Looping = request != nullptr ? request->Loop : asset->DefaultLoop;
        voice.Paused = false;

        const std::uint8_t* sourceBytes = static_cast<const std::uint8_t*>(asset->EncodedBytes->Data);
        const int32_t monoChannelByteLength = sampleFrameCount * static_cast<int32_t>(sizeof(std::int16_t));
        const std::uint32_t rendererSampleRate = ResolveRendererSampleRate();

        try {
            if (voice.ChannelCount == 1) {
                void* channelBuffer = MEMAllocFromDefaultHeapEx(monoChannelByteLength, RequiredAudioAlignment);
                if (channelBuffer == nullptr) {
                    throw std::runtime_error("Wii U audio playback could not allocate one mono PCM buffer.");
                }

                std::int16_t* destinationSamples = static_cast<std::int16_t*>(channelBuffer);
                for (int32_t sampleIndex = 0; sampleIndex < sampleFrameCount; sampleIndex++) {
                    destinationSamples[sampleIndex] = ReadLittleEndianPcm16Sample(
                        sourceBytes + (sampleIndex * static_cast<int32_t>(sizeof(std::int16_t))));
                }

                DCFlushRange(channelBuffer, static_cast<std::uint32_t>(monoChannelByteLength));
                voice.ChannelBuffers[0] = channelBuffer;
                voice.ChannelBufferByteLengths[0] = monoChannelByteLength;
            } else {
                for (int32_t channelIndex = 0; channelIndex < voice.ChannelCount; channelIndex++) {
                    void* channelBuffer = MEMAllocFromDefaultHeapEx(monoChannelByteLength, RequiredAudioAlignment);
                    if (channelBuffer == nullptr) {
                        throw std::runtime_error("Wii U audio playback could not allocate one stereo channel buffer.");
                    }

                    voice.ChannelBuffers[channelIndex] = channelBuffer;
                    voice.ChannelBufferByteLengths[channelIndex] = monoChannelByteLength;
                }

                std::int16_t* leftChannelSamples = static_cast<std::int16_t*>(voice.ChannelBuffers[0]);
                std::int16_t* rightChannelSamples = static_cast<std::int16_t*>(voice.ChannelBuffers[1]);
                for (int32_t frameIndex = 0; frameIndex < sampleFrameCount; frameIndex++) {
                    const int32_t sourceByteOffset = frameIndex * static_cast<int32_t>(sizeof(std::int16_t) * 2);
                    leftChannelSamples[frameIndex] = ReadLittleEndianPcm16Sample(sourceBytes + sourceByteOffset);
                    rightChannelSamples[frameIndex] = ReadLittleEndianPcm16Sample(sourceBytes + sourceByteOffset + static_cast<int32_t>(sizeof(std::int16_t)));
                }

                DCFlushRange(voice.ChannelBuffers[0], static_cast<std::uint32_t>(monoChannelByteLength));
                DCFlushRange(voice.ChannelBuffers[1], static_cast<std::uint32_t>(monoChannelByteLength));
            }

            for (int32_t channelIndex = 0; channelIndex < voice.ChannelCount; channelIndex++) {
                AXVoice* nativeVoice = AXAcquireVoice(VoicePriority, nullptr, nullptr);
                if (nativeVoice == nullptr) {
                    throw std::runtime_error("Wii U audio playback could not reserve one AX voice.");
                }

                AXVoiceOffsets offsets = {};
                offsets.currentOffset = 0;
                offsets.loopOffset = 0;
                offsets.endOffset = static_cast<std::uint32_t>(sampleFrameCount - 1);
                offsets.loopingEnabled = voice.Looping ? AX_VOICE_LOOP_ENABLED : AX_VOICE_LOOP_DISABLED;
                offsets.dataType = AX_VOICE_FORMAT_LPCM16;
                offsets.data = voice.ChannelBuffers[channelIndex];
                if (!AXCheckVoiceOffsets(&offsets)) {
                    AXFreeVoice(nativeVoice);
                    throw std::runtime_error("Wii U audio playback produced invalid AX voice offsets.");
                }

                AXSetVoiceOffsets(nativeVoice, &offsets);
                AXSetVoiceLoop(nativeVoice, voice.Looping ? AX_VOICE_LOOP_ENABLED : AX_VOICE_LOOP_DISABLED);

                if (asset->SampleRate == static_cast<int32_t>(rendererSampleRate)) {
                    AXSetVoiceSrcType(nativeVoice, AX_VOICE_SRC_TYPE_NONE);
                    if (AXSetVoiceSrcRatio(nativeVoice, 1.0f) != AX_VOICE_RATIO_RESULT_SUCCESS) {
                        AXFreeVoice(nativeVoice);
                        throw std::runtime_error("Wii U audio playback could not configure a unity sample-rate ratio.");
                    }
                } else {
                    AXSetVoiceSrcType(nativeVoice, AX_VOICE_SRC_TYPE_LINEAR);
                    if (AXSetVoiceSrcRatio(
                            nativeVoice,
                            static_cast<float>(asset->SampleRate) / static_cast<float>(rendererSampleRate))
                        != AX_VOICE_RATIO_RESULT_SUCCESS) {
                        AXFreeVoice(nativeVoice);
                        throw std::runtime_error("Wii U audio playback could not configure one sample-rate conversion ratio.");
                    }
                }

                AXVoiceVeData volume = { MaxVoiceVolume, 0 };
                AXSetVoiceVe(nativeVoice, &volume);
                ConfigureVoiceDeviceMix(nativeVoice, voice.ChannelCount, channelIndex);
                AXSetVoiceState(nativeVoice, AX_VOICE_STATE_STOPPED);
                voice.NativeVoices[channelIndex] = nativeVoice;
            }
        } catch (...) {
            ReleaseVoiceState(voice);
            throw;
        }

        ActiveVoicesById.emplace(voice.VoiceId, voice);
        ApplyVoicePlaybackState(ActiveVoicesById.at(voice.VoiceId));
        return voice.VoiceId;
    }

    void WiiUAudioBackend::Stop(int32_t voiceId) {
        auto voiceIterator = ActiveVoicesById.find(voiceId);
        if (voiceIterator == ActiveVoicesById.end()) {
            return;
        }

        ReleaseVoiceState(voiceIterator->second);
        ActiveVoicesById.erase(voiceIterator);
    }

    void WiiUAudioBackend::SetBusGain(std::string busId, float gain) {
        BusGainsById[NormalizeBusId(std::move(busId))] = ClampGain(gain);
        for (auto& voiceEntry : ActiveVoicesById) {
            ApplyVoicePlaybackState(voiceEntry.second);
        }
    }

    void WiiUAudioBackend::SetBusPaused(std::string busId, bool paused) {
        std::string normalizedBusId = NormalizeBusId(std::move(busId));
        if (paused) {
            PausedBusIds.insert(normalizedBusId);
        } else {
            PausedBusIds.erase(normalizedBusId);
        }

        for (auto& voiceEntry : ActiveVoicesById) {
            ApplyVoicePlaybackState(voiceEntry.second);
        }
    }

    bool WiiUAudioBackend::IsPlaying(int32_t voiceId) {
        auto voiceIterator = ActiveVoicesById.find(voiceId);
        if (voiceIterator == ActiveVoicesById.end()) {
            return false;
        }

        if (voiceIterator->second.Paused || voiceIterator->second.Looping) {
            return true;
        }

        for (int32_t channelIndex = 0; channelIndex < voiceIterator->second.ChannelCount; channelIndex++) {
            AXVoice* nativeVoice = voiceIterator->second.NativeVoices[channelIndex];
            if (nativeVoice != nullptr && AXIsVoiceRunning(nativeVoice)) {
                return true;
            }
        }

        return false;
    }

    void WiiUAudioBackend::Update() {
        for (auto voiceIterator = ActiveVoicesById.begin(); voiceIterator != ActiveVoicesById.end();) {
            if (voiceIterator->second.Looping || voiceIterator->second.Paused) {
                ++voiceIterator;
                continue;
            }

            bool isAnyNativeVoiceRunning = false;
            for (int32_t channelIndex = 0; channelIndex < voiceIterator->second.ChannelCount; channelIndex++) {
                AXVoice* nativeVoice = voiceIterator->second.NativeVoices[channelIndex];
                if (nativeVoice != nullptr && AXIsVoiceRunning(nativeVoice)) {
                    isAnyNativeVoiceRunning = true;
                    break;
                }
            }

            if (!isAnyNativeVoiceRunning) {
                ReleaseVoiceState(voiceIterator->second);
                voiceIterator = ActiveVoicesById.erase(voiceIterator);
                continue;
            }

            ++voiceIterator;
        }
    }

    std::string WiiUAudioBackend::NormalizeBusId(std::string busId) {
        if (busId.empty()) {
            return "master";
        }

        std::transform(
            busId.begin(),
            busId.end(),
            busId.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return busId;
    }

    float WiiUAudioBackend::ClampGain(float gain) {
        if (!(gain >= 0.0f) || gain != gain) {
            return 0.0f;
        }

        return std::clamp(gain, 0.0f, 1.0f);
    }

    std::uint16_t WiiUAudioBackend::ConvertGainToVolume(float gain) {
        return static_cast<std::uint16_t>(ClampGain(gain) * static_cast<float>(MaxVoiceVolume));
    }

    std::uint32_t WiiUAudioBackend::ResolveRendererSampleRate() {
        const std::uint32_t samplesPerSecond = AXGetInputSamplesPerSec();
        return samplesPerSecond > 0U ? samplesPerSecond : 48000U;
    }

    float WiiUAudioBackend::ResolveCombinedGain(const std::string& busId, float baseGain) const {
        float masterGain = 1.0f;
        auto masterGainIterator = BusGainsById.find("master");
        if (masterGainIterator != BusGainsById.end()) {
            masterGain = masterGainIterator->second;
        }

        float busGain = 1.0f;
        auto busGainIterator = BusGainsById.find(busId);
        if (busGainIterator != BusGainsById.end()) {
            busGain = busGainIterator->second;
        }

        return ClampGain(masterGain * busGain * baseGain);
    }

    bool WiiUAudioBackend::IsBusPaused(const std::string& busId) const {
        return PausedBusIds.contains("master") || PausedBusIds.contains(busId);
    }

    void WiiUAudioBackend::ApplyVoicePlaybackState(ActiveVoiceState& voice) {
        AXVoiceVeData volume = {
            ConvertGainToVolume(ResolveCombinedGain(voice.BusId, voice.BaseGain)),
            0
        };

        const bool shouldPause = IsBusPaused(voice.BusId);
        for (int32_t channelIndex = 0; channelIndex < voice.ChannelCount; channelIndex++) {
            AXVoice* nativeVoice = voice.NativeVoices[channelIndex];
            if (nativeVoice == nullptr) {
                continue;
            }

            AXSetVoiceVe(nativeVoice, &volume);
            AXSetVoiceState(nativeVoice, shouldPause ? AX_VOICE_STATE_STOPPED : AX_VOICE_STATE_PLAYING);
        }
        voice.Paused = shouldPause;
    }

    void WiiUAudioBackend::ReleaseVoiceState(ActiveVoiceState& voice) {
        for (int32_t channelIndex = 0; channelIndex < MaxChannelVoices; channelIndex++) {
            if (voice.NativeVoices[channelIndex] != nullptr) {
                AXSetVoiceState(voice.NativeVoices[channelIndex], AX_VOICE_STATE_STOPPED);
                AXFreeVoice(voice.NativeVoices[channelIndex]);
                voice.NativeVoices[channelIndex] = nullptr;
            }
        }

        for (int32_t channelIndex = 0; channelIndex < MaxChannelVoices; channelIndex++) {
            if (voice.ChannelBuffers[channelIndex] != nullptr) {
                MEMFreeToDefaultHeap(voice.ChannelBuffers[channelIndex]);
                voice.ChannelBuffers[channelIndex] = nullptr;
            }

            voice.ChannelBufferByteLengths[channelIndex] = 0;
        }

        voice.ChannelCount = 0;
        voice.Looping = false;
        voice.Paused = false;
    }
}

#endif
