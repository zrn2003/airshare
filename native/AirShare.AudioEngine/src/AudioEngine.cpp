#include "AudioEngine.h"

#include <Windows.h>
#include <algorithm>
#include <objbase.h>
#include <unordered_map>

namespace airshare {

namespace {

void __stdcall DeviceCallbackBridge(const AirShareDeviceInfo* device, int added, void* userData) {
    auto* engine = static_cast<AudioEngine*>(userData);
    if (engine) {
        engine->InvokeDeviceCallback(device, added);
    }
}

void __stdcall StatsCallbackBridge(const wchar_t* deviceId, int latencyMs, int bufferFillPercent, void* userData) {
    auto* engine = static_cast<AudioEngine*>(userData);
    if (engine) {
        engine->InvokeStatsCallback(deviceId, latencyMs, bufferFillPercent);
    }
}

} // namespace

AudioEngine& AudioEngine::Instance() {
    static AudioEngine engine;
    return engine;
}

int AudioEngine::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return AIRSHARE_OK;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return AIRSHARE_ERR_ENGINE;
    }

    deviceManager_ = std::make_unique<DeviceManager>();
    captureEngine_ = std::make_unique<CaptureEngine>();
    renderEngine_ = std::make_unique<RenderEngine>();
    syncEngine_ = std::make_unique<SyncEngine>();

    if (!deviceManager_->Initialize()) {
        return AIRSHARE_ERR_DEVICE;
    }

    deviceManager_->SetCallback(DeviceCallbackBridge, this);
    syncEngine_->SetStatsCallback(StatsCallbackBridge, this);

    initialized_ = true;
    return AIRSHARE_OK;
}

void AudioEngine::Shutdown() {
    StopSharing();

    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }

    if (deviceManager_) {
        deviceManager_->Shutdown();
    }

    deviceManager_.reset();
    captureEngine_.reset();
    renderEngine_.reset();
    syncEngine_.reset();
    ringBuffer_.reset();

    initialized_ = false;
    CoUninitialize();
}

int AudioEngine::EnumerateDevices(AirShareDeviceInfo* devices, int maxCount, int* outCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !deviceManager_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }

    const auto records = deviceManager_->EnumerateRenderDevices();
    const int count = static_cast<int>(std::min(records.size(), static_cast<size_t>(maxCount)));
    for (int i = 0; i < count; ++i) {
        wcsncpy_s(devices[i].id, records[i].id.c_str(), _TRUNCATE);
        wcsncpy_s(devices[i].name, records[i].name.c_str(), _TRUNCATE);
        wcsncpy_s(devices[i].type, records[i].type.c_str(), _TRUNCATE);
        devices[i].latencyMs = records[i].latencyMs;
        devices[i].isConnected = records[i].connected ? 1 : 0;
    }

    if (outCount) {
        *outCount = count;
    }
    return AIRSHARE_OK;
}

int AudioEngine::RefreshDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !deviceManager_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }
    return deviceManager_->Refresh() ? AIRSHARE_OK : AIRSHARE_ERR_DEVICE;
}

void AudioEngine::SetDeviceCallback(AirShareDeviceCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(mutex_);
    deviceCallback_ = callback;
    deviceCallbackUserData_ = userData;
}

void AudioEngine::SetStatsCallback(AirShareStatsCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(mutex_);
    statsCallback_ = callback;
    statsCallbackUserData_ = userData;
}

int AudioEngine::ComputeReadOffsets(const std::vector<std::wstring>& deviceIds) {
    if (!deviceManager_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));

    if (FAILED(hr)) {
        return AIRSHARE_ERR_DEVICE;
    }

    int maxLatencyMs = 0;
    std::unordered_map<std::wstring, int> latencies;

    for (const auto& id : deviceIds) {
        IMMDevice* device = nullptr;
        if (FAILED(enumerator->GetDevice(id.c_str(), &device)) || !device) {
            continue;
        }

        int latencyMs = syncEngine_->GetLatency(id);
        if (latencyMs <= 0) {
            latencyMs = DeviceManager::EstimateLatencyMs(device, 48000);
        }

        latencies[id] = latencyMs;
        maxLatencyMs = std::max(maxLatencyMs, latencyMs);
        device->Release();
    }

    enumerator->Release();

    const int sampleRate = 48000;
    for (const auto& id : deviceIds) {
        const int deviceLatency = latencies[id];
        const int offsetMs = maxLatencyMs - deviceLatency;
        const int offsetFrames = (offsetMs * sampleRate) / 1000;

        if (!renderEngine_->StartDevice(
                id,
                ringBuffer_.get(),
                offsetFrames,
                deviceLatency,
                1.0f,
                bufferSizeFrames_)) {
            return AIRSHARE_ERR_DEVICE;
        }
    }

    return AIRSHARE_OK;
}

int AudioEngine::StartSharing(const std::vector<std::wstring>& deviceIds, int bufferSizeFrames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }
    if (sharing_) {
        return AIRSHARE_ERR_ALREADY_RUNNING;
    }
    if (deviceIds.empty()) {
        return AIRSHARE_ERR_INVALID_ARG;
    }

    bufferSizeFrames_ = bufferSizeFrames > 0 ? bufferSizeFrames : 480;
    activeDeviceIds_ = deviceIds;

    ringBuffer_ = std::make_unique<RingBuffer>(16384, 2);
    ringBuffer_->Clear();

    if (!captureEngine_->Start(ringBuffer_.get(), bufferSizeFrames_, masterVolume_)) {
        ringBuffer_.reset();
        return AIRSHARE_ERR_ENGINE;
    }

    Sleep(100);

    if (ComputeReadOffsets(deviceIds) != AIRSHARE_OK) {
        captureEngine_->Stop();
        renderEngine_->StopAll();
        ringBuffer_.reset();
        return AIRSHARE_ERR_DEVICE;
    }

    syncEngine_->Start(renderEngine_.get());
    sharing_ = true;
    return AIRSHARE_OK;
}

int AudioEngine::StopSharing() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sharing_) {
        return AIRSHARE_OK;
    }

    syncEngine_->Stop();
    renderEngine_->StopAll();
    captureEngine_->Stop();
    ringBuffer_.reset();
    activeDeviceIds_.clear();
    sharing_ = false;
    return AIRSHARE_OK;
}

int AudioEngine::SetMasterVolume(float gain) {
    std::lock_guard<std::mutex> lock(mutex_);
    masterVolume_ = std::clamp(gain, 0.0f, 1.0f);
    if (captureEngine_) {
        captureEngine_->SetMasterVolume(masterVolume_);
    }
    return AIRSHARE_OK;
}

int AudioEngine::SetDeviceVolume(const std::wstring& deviceId, float gain) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!renderEngine_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }
    renderEngine_->SetDeviceVolume(deviceId, std::clamp(gain, 0.0f, 1.0f));
    return AIRSHARE_OK;
}

int AudioEngine::GetDeviceLatency(const std::wstring& deviceId, int* latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!latencyMs || !syncEngine_) {
        return AIRSHARE_ERR_INVALID_ARG;
    }

    *latencyMs = syncEngine_->GetLatency(deviceId);
    if (*latencyMs <= 0 && deviceManager_) {
        IMMDeviceEnumerator* enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                reinterpret_cast<void**>(&enumerator)))) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(enumerator->GetDevice(deviceId.c_str(), &device)) && device) {
                *latencyMs = DeviceManager::EstimateLatencyMs(device, 48000);
                device->Release();
            }
            enumerator->Release();
        }
    }
    return AIRSHARE_OK;
}

int AudioEngine::SetDeviceLatencyOffset(const std::wstring& deviceId, int latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!syncEngine_) {
        return AIRSHARE_ERR_NOT_INITIALIZED;
    }
    syncEngine_->SetLatencyOffset(deviceId, latencyMs);
    return AIRSHARE_OK;
}

void AudioEngine::InvokeDeviceCallback(const AirShareDeviceInfo* device, int added) {
    if (deviceCallback_) {
        deviceCallback_(device, added, deviceCallbackUserData_);
    }
}

void AudioEngine::InvokeStatsCallback(const wchar_t* deviceId, int latencyMs, int bufferFillPercent) {
    if (statsCallback_) {
        statsCallback_(deviceId, latencyMs, bufferFillPercent, statsCallbackUserData_);
    }
}

} // namespace airshare
