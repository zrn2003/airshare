#include "CaptureEngine.h"

#include <Windows.h>
#include <avrt.h>
#include <cmath>
#include <vector>

#pragma comment(lib, "Avrt.lib")

namespace airshare {

namespace {

void ApplyGain(float* samples, size_t sampleCount, float gain) {
    for (size_t i = 0; i < sampleCount; ++i) {
        samples[i] *= gain;
    }
}

bool ConvertToFloat(const BYTE* data, UINT32 numFrames, const WAVEFORMATEX* format, std::vector<float>& output) {
    if (!format || !data) {
        return false;
    }

    const UINT32 channels = format->nChannels;
    output.resize(numFrames * channels);

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->wBitsPerSample == 32)) {
        const float* src = reinterpret_cast<const float*>(data);
        std::memcpy(output.data(), src, output.size() * sizeof(float));
        return true;
    }

    if (format->wFormatTag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16) {
        const int16_t* src = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = static_cast<float>(src[i]) / 32768.0f;
        }
        return true;
    }

    return false;
}

} // namespace

CaptureEngine::CaptureEngine() = default;

CaptureEngine::~CaptureEngine() {
    Stop();
}

bool CaptureEngine::Start(RingBuffer* ringBuffer, int preferredBufferFrames, float masterVolume) {
    if (running_.load() || !ringBuffer) {
        return false;
    }

    ringBuffer_ = ringBuffer;
    masterVolume_.store(masterVolume);
    bufferFrames_ = preferredBufferFrames > 0 ? preferredBufferFrames : 480;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));

    if (FAILED(hr)) {
        return false;
    }

    IMMDevice* defaultDevice = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
    enumerator->Release();

    if (FAILED(hr) || !defaultDevice) {
        return false;
    }

    hr = defaultDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient_));
    defaultDevice->Release();

    if (FAILED(hr) || !audioClient_) {
        return false;
    }

    hr = audioClient_->GetMixFormat(&mixFormat_);
    if (FAILED(hr) || !mixFormat_) {
        return false;
    }

    sampleRate_ = static_cast<int>(mixFormat_->nSamplesPerSec);
    channels_ = static_cast<int>(mixFormat_->nChannels);

    REFERENCE_TIME bufferDuration = static_cast<REFERENCE_TIME>(bufferFrames_) * 10000000LL / sampleRate_;
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        bufferDuration,
        0,
        mixFormat_,
        nullptr);

    if (FAILED(hr)) {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
        audioClient_->Release();
        audioClient_ = nullptr;
        return false;
    }

    UINT32 actualBufferFrames = 0;
    if (SUCCEEDED(audioClient_->GetBufferSize(&actualBufferFrames))) {
        bufferFrames_ = actualBufferFrames;
    }

    hr = audioClient_->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient_));
    if (FAILED(hr)) {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
        audioClient_->Release();
        audioClient_ = nullptr;
        return false;
    }

    hr = audioClient_->Start();
    if (FAILED(hr)) {
        captureClient_->Release();
        captureClient_ = nullptr;
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
        audioClient_->Release();
        audioClient_ = nullptr;
        return false;
    }

    stopRequested_.store(false);
    running_.store(true);
    thread_ = std::thread(&CaptureEngine::CaptureLoop, this);
    return true;
}

void CaptureEngine::Stop() {
    if (!running_.load()) {
        return;
    }

    stopRequested_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }

    if (audioClient_) {
        audioClient_->Stop();
    }
    if (captureClient_) {
        captureClient_->Release();
        captureClient_ = nullptr;
    }
    if (mixFormat_) {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }

    running_.store(false);
    ringBuffer_ = nullptr;
}

void CaptureEngine::CaptureLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    std::vector<float> converted;
    std::vector<float> scratch;

    while (!stopRequested_.load()) {
        Sleep(1);

        UINT32 packetLength = 0;
        if (FAILED(captureClient_->GetNextPacketSize(&packetLength)) || packetLength == 0) {
            continue;
        }

        BYTE* data = nullptr;
        UINT32 numFrames = 0;
        DWORD flags = 0;
        if (FAILED(captureClient_->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr))) {
            continue;
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            scratch.assign(numFrames * channels_, 0.0f);
            ApplyGain(scratch.data(), scratch.size(), masterVolume_.load());
            ringBuffer_->Write(scratch.data(), numFrames);
        } else if (ConvertToFloat(data, numFrames, mixFormat_, converted)) {
            ApplyGain(converted.data(), converted.size(), masterVolume_.load());
            ringBuffer_->Write(converted.data(), numFrames);
        }

        captureClient_->ReleaseBuffer(numFrames);
    }

    if (task) {
        AvRevertMmThreadCharacteristics(task);
    }
    CoUninitialize();
}

} // namespace airshare
