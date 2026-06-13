#include "DeviceManager.h"

#include <comdef.h>
#include <propvarutil.h>

namespace airshare {

class DeviceNotificationClient : public IMMNotificationClient {
public:
    explicit DeviceNotificationClient(DeviceManager* owner) : refCount_(1), owner_(owner) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount_); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) {
            return E_POINTER;
        }
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppvObject = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override {
        if (owner_) {
            owner_->NotifyDeviceChange(deviceId, true);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override {
        if (owner_) {
            owner_->NotifyDeviceChange(deviceId, false);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override {
        if (!owner_) {
            return S_OK;
        }
        owner_->NotifyDeviceChange(deviceId, newState == DEVICE_STATE_ACTIVE);
        return S_OK;
    }

private:
    LONG refCount_;
    DeviceManager* owner_;
};

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager() {
    Shutdown();
}

bool DeviceManager::Initialize() {
    const HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator_));

    if (FAILED(hr) || !enumerator_) {
        return false;
    }

    notificationClient_ = new DeviceNotificationClient(this);
    enumerator_->RegisterEndpointNotificationCallback(notificationClient_);
    Refresh();
    return true;
}

void DeviceManager::Shutdown() {
    if (enumerator_ && notificationClient_) {
        enumerator_->UnregisterEndpointNotificationCallback(notificationClient_);
        notificationClient_->Release();
        notificationClient_ = nullptr;
    }
    if (enumerator_) {
        enumerator_->Release();
        enumerator_ = nullptr;
    }
    devices_.clear();
}

std::wstring DeviceManager::DetectDeviceType(IMMDevice* device) {
    if (!device) {
        return L"Unknown";
    }

    IPropertyStore* props = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props)) || !props) {
        return L"Unknown";
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring type = L"Wired";

    if (SUCCEEDED(props->GetValue(PKEY_Device_InstanceId, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
        const std::wstring instanceId(value.pwszVal);
        if (instanceId.find(L"BTH") != std::wstring::npos || instanceId.find(L"Bluetooth") != std::wstring::npos) {
            type = L"Bluetooth";
        }
    }

    PropVariantClear(&value);
    props->Release();
    return type;
}

int DeviceManager::EstimateLatencyMs(IMMDevice* device, int sampleRate) {
    if (!device || sampleRate <= 0) {
        return 20;
    }

    IAudioClient* client = nullptr;
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) || !client) {
        return 20;
    }

    REFERENCE_TIME latency = 0;
    client->GetDevicePeriod(nullptr, &latency);
    client->Release();

    const int ms = static_cast<int>((latency / 10000) + 5);
    return ms < 5 ? 5 : ms;
}

std::vector<DeviceRecord> DeviceManager::EnumerateRenderDevices() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DeviceRecord> result;
    result.reserve(devices_.size());
    for (const auto& pair : devices_) {
        if (pair.second.connected) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool DeviceManager::Refresh() {
    if (!enumerator_) {
        return false;
    }

    IMMDeviceCollection* collection = nullptr;
    HRESULT hr = enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) {
        return false;
    }

    UINT count = 0;
    collection->GetCount(&count);

    std::unordered_map<std::wstring, DeviceRecord> refreshed;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device)) || !device) {
            continue;
        }

        LPWSTR id = nullptr;
        device->GetId(&id);

        IPropertyStore* props = nullptr;
        device->OpenPropertyStore(STGM_READ, &props);

        PROPVARIANT friendlyName;
        PropVariantInit(&friendlyName);
        std::wstring name = L"Unknown Device";
        if (props && SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &friendlyName)) && friendlyName.vt == VT_LPWSTR) {
            name = friendlyName.pwszVal;
        }

        DeviceRecord record;
        record.id = id ? id : L"";
        record.name = name;
        record.type = DetectDeviceType(device);
        record.latencyMs = EstimateLatencyMs(device, 48000);
        record.connected = true;

        refreshed[record.id] = record;

        PropVariantClear(&friendlyName);
        if (props) {
            props->Release();
        }
        CoTaskMemFree(id);
        device->Release();
    }

    collection->Release();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_ = std::move(refreshed);
    }

    return true;
}

void DeviceManager::SetCallback(AirShareDeviceCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
    callbackUserData_ = userData;
}

void DeviceManager::NotifyDeviceChange(const std::wstring& id, bool added) {
    Refresh();

    AirShareDeviceCallback callback = nullptr;
    void* userData = nullptr;
    DeviceRecord record;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = callback_;
        userData = callbackUserData_;

        auto it = devices_.find(id);
        if (it != devices_.end()) {
            record = it->second;
        } else {
            record.id = id;
            record.name = L"Disconnected Device";
            record.type = L"Unknown";
            record.connected = added;
        }
    }

    if (callback) {
        AirShareDeviceInfo info{};
        wcsncpy_s(info.id, record.id.c_str(), _TRUNCATE);
        wcsncpy_s(info.name, record.name.c_str(), _TRUNCATE);
        wcsncpy_s(info.type, record.type.c_str(), _TRUNCATE);
        info.latencyMs = record.latencyMs;
        info.isConnected = record.connected ? 1 : 0;
        callback(&info, added ? 1 : 0, userData);
    }
}

} // namespace airshare
