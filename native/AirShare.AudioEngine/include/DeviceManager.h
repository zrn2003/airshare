#pragma once

#include "AudioEngineApi.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace airshare {

struct DeviceRecord {
    std::wstring id;
    std::wstring name;
    std::wstring type;
    int latencyMs = 0;
    bool connected = true;
};

class DeviceNotificationClient;

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    bool Initialize();
    void Shutdown();

    std::vector<DeviceRecord> EnumerateRenderDevices();
    bool Refresh();

    void SetCallback(AirShareDeviceCallback callback, void* userData);
    void NotifyDeviceChange(const std::wstring& id, bool added);

    static std::wstring DetectDeviceType(IMMDevice* device);
    static int EstimateLatencyMs(IMMDevice* device, int sampleRate);

private:
    friend class DeviceNotificationClient;

    IMMDeviceEnumerator* enumerator_ = nullptr;
    DeviceNotificationClient* notificationClient_ = nullptr;
    std::mutex mutex_;
    std::unordered_map<std::wstring, DeviceRecord> devices_;
    AirShareDeviceCallback callback_ = nullptr;
    void* callbackUserData_ = nullptr;
};

} // namespace airshare
