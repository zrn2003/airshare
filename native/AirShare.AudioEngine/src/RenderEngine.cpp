#include "RenderEngine.h"

#include <Windows.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <algorithm>
#include <vector>

#pragma comment(lib, "Avrt.lib")

namespace {

void WriteFloatSamplesToBuffer(
    float* floatSamples,
    size_t sampleCount,
    BYTE* renderData,
    const WAVEFORMATEX* format,
    float volume) {

    if (!format || !renderData || !floatSamples) {
        return;
    }

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->wBitsPerSample == 32)) {
        float* output = reinterpret_cast<float*>(renderData);
        for (size_t i = 0; i < sampleCount; ++i) {
            output[i] = floatSamples[i] * volume;
        }
        return;
    }

    if (format->wFormatTag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16) {
        int16_t* output = reinterpret_cast<int16_t*>(renderData);
        for (size_t i = 0; i < sampleCount; ++i) {
            const float scaled = std::clamp(floatSamples[i] * volume, -1.0f, 1.0f);
            output[i] = static_cast<int16_t>(scaled * 32767.0f);
        }
    }
}

} // namespace

namespace airshare {

struct RenderEngine::RenderContext {
    std::wstring deviceId;
    std::wstring deviceName;
    std::shared_ptr<RenderDeviceState> state;

    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioRenderClient* renderClient = nullptr;
    WAVEFORMATEX* mixFormat = nullptr;

    std::thread thread;
    std::atomic<bool> stopRequested{ false };

    int sampleRate = 48000;
    int channels = 2;
    int bufferFrames = 480;
};

RenderEngine::RenderEngine() = default;

RenderEngine::~RenderEngine() {
    StopAll();
}

bool RenderEngine::StartDevice(
    const std::wstring& deviceId,
    RingBuffer* ringBuffer,
    int readOffsetFrames,
    int latencyMs,
    float volume,
    int preferredBufferFrames) {

    if (!ringBuffer) {
        return false;
    }

    ringBuffer_ = ringBuffer;

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

    auto context = std::make_shared<RenderContext>();
    context->deviceId = deviceId;
    context->state = std::make_shared<RenderDeviceState>();
    context->state->deviceId = deviceId;
    context->state->latencyMs = latencyMs;
    context->state->readOffsetFrames = readOffsetFrames;
    context->state->volume = volume;
    context->bufferFrames = preferredBufferFrames > 0 ? preferredBufferFrames : 480;

    hr = enumerator->GetDevice(deviceId.c_str(), &context->device);
    enumerator->Release();

    if (FAILED(hr) || !context->device) {
        return false;
    }

    IPropertyStore* props = nullptr;
    if (SUCCEEDED(context->device->OpenPropertyStore(STGM_READ, &props)) && props) {
        PROPVARIANT friendlyName;
        PropVariantInit(&friendlyName);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &friendlyName)) && friendlyName.vt == VT_LPWSTR) {
            context->deviceName = friendlyName.pwszVal;
            context->state->deviceName = context->deviceName;
        }
        PropVariantClear(&friendlyName);
        props->Release();
    }

    hr = context->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&context->audioClient));
    if (FAILED(hr) || !context->audioClient) {
        return false;
    }

    hr = context->audioClient->GetMixFormat(&context->mixFormat);
    if (FAILED(hr) || !context->mixFormat) {
        return false;
    }

    context->sampleRate = static_cast<int>(context->mixFormat->nSamplesPerSec);
    context->channels = static_cast<int>(context->mixFormat->nChannels);

    REFERENCE_TIME bufferDuration = static_cast<REFERENCE_TIME>(context->bufferFrames) * 10000000LL / context->sampleRate;
    hr = context->audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        bufferDuration,
        0,
        context->mixFormat,
        nullptr);

    if (FAILED(hr)) {
        CoTaskMemFree(context->mixFormat);
        context->device->Release();
        return false;
    }

    UINT32 actualBufferFrames = 0;
    if (SUCCEEDED(context->audioClient->GetBufferSize(&actualBufferFrames))) {
        context->bufferFrames = actualBufferFrames;
    }

    hr = context->audioClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&context->renderClient));
    if (FAILED(hr)) {
        CoTaskMemFree(context->mixFormat);
        context->audioClient->Release();
        context->device->Release();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        contexts_.push_back(context);
    }

    running_.store(true);
    context->thread = std::thread(&RenderEngine::RenderLoop, this, context);
    return true;
}

void RenderEngine::StopAll() {
    std::vector<std::shared_ptr<RenderContext>> localContexts;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        localContexts.swap(contexts_);
    }

    for (auto& context : localContexts) {
        context->stopRequested.store(true);
        if (context->thread.joinable()) {
            context->thread.join();
        }
        if (context->audioClient) {
            context->audioClient->Stop();
        }
        if (context->renderClient) {
            context->renderClient->Release();
        }
        if (context->mixFormat) {
            CoTaskMemFree(context->mixFormat);
        }
        if (context->audioClient) {
            context->audioClient->Release();
        }
        if (context->device) {
            context->device->Release();
        }
    }

    running_.store(false);
}

void RenderEngine::SetDeviceVolume(const std::wstring& deviceId, float gain) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& context : contexts_) {
        if (context->deviceId == deviceId && context->state) {
            context->state->volume = gain;
        }
    }
}

void RenderEngine::SetReadOffset(const std::wstring& deviceId, int offsetFrames) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& context : contexts_) {
        if (context->deviceId == deviceId && context->state) {
            context->state->readOffsetFrames = offsetFrames;
        }
    }
}

int RenderEngine::GetBufferFillPercent(const std::wstring& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& context : contexts_) {
        if (context->deviceId == deviceId && context->state) {
            return context->state->bufferFillPercent;
        }
    }
    return 0;
}

bool RenderEngine::IsRunning() const {
    return running_.load();
}

std::vector<std::shared_ptr<RenderDeviceState>> RenderEngine::GetDeviceStates() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<RenderDeviceState>> states;
    states.reserve(contexts_.size());
    for (const auto& context : contexts_) {
        states.push_back(context->state);
    }
    return states;
}

void RenderEngine::RenderLoop(std::shared_ptr<RenderContext> context) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    const int prebufferFrames = context->bufferFrames * 3;
    while (!context->stopRequested.load() && ringBuffer_ &&
           ringBuffer_->AvailableForConsumer(
               context->state->consumerReadPos,
               static_cast<size_t>(context->state->readOffsetFrames)) <
               static_cast<size_t>(prebufferFrames)) {
        Sleep(5);
    }

    context->audioClient->Start();

    std::vector<float> readBuffer(context->bufferFrames * context->channels);
    std::vector<float> silence(context->bufferFrames * context->channels, 0.0f);

    while (!context->stopRequested.load()) {
        UINT32 padding = 0;
        if (FAILED(context->audioClient->GetCurrentPadding(&padding))) {
            Sleep(1);
            continue;
        }

        const UINT32 availableFrames = context->bufferFrames - padding;
        if (availableFrames == 0) {
            Sleep(1);
            continue;
        }

        const size_t read = ringBuffer_->ReadForConsumer(
            readBuffer.data(),
            availableFrames,
            context->state->consumerReadPos,
            static_cast<size_t>(context->state->readOffsetFrames));

        BYTE* renderData = nullptr;
        if (FAILED(context->renderClient->GetBuffer(availableFrames, &renderData))) {
            continue;
        }

        float* output = reinterpret_cast<float*>(renderData);
        const size_t sampleCount = static_cast<size_t>(availableFrames) * context->channels;
        const float volume = context->state->volume;

        if (read > 0) {
            const size_t readSamples = read * context->channels;
            if (read < availableFrames) {
                std::fill(readBuffer.begin() + readSamples, readBuffer.begin() + sampleCount, 0.0f);
            }
            WriteFloatSamplesToBuffer(readBuffer.data(), sampleCount, renderData, context->mixFormat, volume);
            context->state->consumerReadPos += read;
        } else {
            WriteFloatSamplesToBuffer(silence.data(), sampleCount, renderData, context->mixFormat, volume);
        }

        context->renderClient->ReleaseBuffer(availableFrames, 0);
        context->state->bufferFillPercent = ringBuffer_->FillPercent(
            context->state->consumerReadPos,
            static_cast<size_t>(context->state->readOffsetFrames));
    }

    if (task) {
        AvRevertMmThreadCharacteristics(task);
    }
    CoUninitialize();
}

} // namespace airshare
