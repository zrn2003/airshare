#pragma once

#include "RingBuffer.h"

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <memory>
#include <thread>

namespace airshare {

class CaptureEngine {
public:
    CaptureEngine();
    ~CaptureEngine();

    bool Start(RingBuffer* ringBuffer, int preferredBufferFrames, float masterVolume);
    void Stop();
    bool IsRunning() const { return running_.load(); }

    void SetMasterVolume(float gain) { masterVolume_.store(gain); }
    int SampleRate() const { return sampleRate_; }
    int Channels() const { return channels_; }

private:
    void CaptureLoop();

    RingBuffer* ringBuffer_ = nullptr;
    IMMDevice* device_ = nullptr;
    IAudioClient* audioClient_ = nullptr;
    IAudioCaptureClient* captureClient_ = nullptr;
    WAVEFORMATEX* mixFormat_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> stopRequested_{ false };
    std::atomic<float> masterVolume_{ 1.0f };

    int sampleRate_ = 48000;
    int channels_ = 2;
    int bufferFrames_ = 480;
};

} // namespace airshare
