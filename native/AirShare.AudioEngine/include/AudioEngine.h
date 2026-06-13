#pragma once

#include "AudioEngineApi.h"
#include "CaptureEngine.h"
#include "DeviceManager.h"
#include "RenderEngine.h"
#include "RingBuffer.h"
#include "SyncEngine.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace airshare {

class AudioEngine {
public:
    static AudioEngine& Instance();

    int Initialize();
    void Shutdown();

    int EnumerateDevices(AirShareDeviceInfo* devices, int maxCount, int* outCount);
    int RefreshDevices();

    void SetDeviceCallback(AirShareDeviceCallback callback, void* userData);
    void SetStatsCallback(AirShareStatsCallback callback, void* userData);

    int StartSharing(const std::vector<std::wstring>& deviceIds, int bufferSizeFrames);
    int StopSharing();
    bool IsSharing() const { return sharing_; }
    bool IsInitialized() const { return initialized_; }

    int SetMasterVolume(float gain);
    int SetDeviceVolume(const std::wstring& deviceId, float gain);
    int GetDeviceLatency(const std::wstring& deviceId, int* latencyMs);
    int SetDeviceLatencyOffset(const std::wstring& deviceId, int latencyMs);

    void InvokeDeviceCallback(const AirShareDeviceInfo* device, int added);
    void InvokeStatsCallback(const wchar_t* deviceId, int latencyMs, int bufferFillPercent);

private:
    AudioEngine() = default;
    int ComputeReadOffsets(const std::vector<std::wstring>& deviceIds);

    std::mutex mutex_;
    bool initialized_ = false;
    bool sharing_ = false;

    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<CaptureEngine> captureEngine_;
    std::unique_ptr<RenderEngine> renderEngine_;
    std::unique_ptr<SyncEngine> syncEngine_;
    std::unique_ptr<RingBuffer> ringBuffer_;

    float masterVolume_ = 1.0f;
    int bufferSizeFrames_ = 480;
    std::vector<std::wstring> activeDeviceIds_;

    AirShareDeviceCallback deviceCallback_ = nullptr;
    void* deviceCallbackUserData_ = nullptr;
    AirShareStatsCallback statsCallback_ = nullptr;
    void* statsCallbackUserData_ = nullptr;
};

} // namespace airshare
