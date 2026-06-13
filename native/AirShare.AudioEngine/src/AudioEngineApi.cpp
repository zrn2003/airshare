#include "AudioEngineApi.h"
#include "AudioEngine.h"

#include <string>
#include <vector>

namespace {

int ToAirShareResult(int code) {
    return code;
}

} // namespace

AIRSHARE_API int __stdcall AudioEngine_Initialize(void) {
    return ToAirShareResult(airshare::AudioEngine::Instance().Initialize());
}

AIRSHARE_API void __stdcall AudioEngine_Shutdown(void) {
    airshare::AudioEngine::Instance().Shutdown();
}

AIRSHARE_API int __stdcall AudioEngine_IsInitialized(void) {
    return airshare::AudioEngine::Instance().IsInitialized() ? 1 : 0;
}

AIRSHARE_API int __stdcall AudioEngine_EnumerateDevices(AirShareDeviceInfo* devices, int maxCount, int* outCount) {
    return ToAirShareResult(airshare::AudioEngine::Instance().EnumerateDevices(devices, maxCount, outCount));
}

AIRSHARE_API int __stdcall AudioEngine_RefreshDevices(void) {
    return ToAirShareResult(airshare::AudioEngine::Instance().RefreshDevices());
}

AIRSHARE_API void __stdcall AudioEngine_SetDeviceCallback(AirShareDeviceCallback callback, void* userData) {
    airshare::AudioEngine::Instance().SetDeviceCallback(callback, userData);
}

AIRSHARE_API void __stdcall AudioEngine_SetStatsCallback(AirShareStatsCallback callback, void* userData) {
    airshare::AudioEngine::Instance().SetStatsCallback(callback, userData);
}

AIRSHARE_API int __stdcall AudioEngine_StartSharing(const wchar_t** deviceIds, int count, int bufferSizeFrames) {
    if (!deviceIds || count <= 0) {
        return AIRSHARE_ERR_INVALID_ARG;
    }

    std::vector<std::wstring> ids;
    ids.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (deviceIds[i]) {
            ids.emplace_back(deviceIds[i]);
        }
    }

    return ToAirShareResult(airshare::AudioEngine::Instance().StartSharing(ids, bufferSizeFrames));
}

AIRSHARE_API int __stdcall AudioEngine_StopSharing(void) {
    return ToAirShareResult(airshare::AudioEngine::Instance().StopSharing());
}

AIRSHARE_API int __stdcall AudioEngine_IsSharing(void) {
    return airshare::AudioEngine::Instance().IsSharing() ? 1 : 0;
}

AIRSHARE_API int __stdcall AudioEngine_SetMasterVolume(float gain) {
    return ToAirShareResult(airshare::AudioEngine::Instance().SetMasterVolume(gain));
}

AIRSHARE_API int __stdcall AudioEngine_SetDeviceVolume(const wchar_t* deviceId, float gain) {
    if (!deviceId) {
        return AIRSHARE_ERR_INVALID_ARG;
    }
    return ToAirShareResult(airshare::AudioEngine::Instance().SetDeviceVolume(deviceId, gain));
}

AIRSHARE_API int __stdcall AudioEngine_GetDeviceLatency(const wchar_t* deviceId, int* latencyMs) {
    if (!deviceId) {
        return AIRSHARE_ERR_INVALID_ARG;
    }
    return ToAirShareResult(airshare::AudioEngine::Instance().GetDeviceLatency(deviceId, latencyMs));
}

AIRSHARE_API int __stdcall AudioEngine_SetDeviceLatencyOffset(const wchar_t* deviceId, int latencyMs) {
    if (!deviceId) {
        return AIRSHARE_ERR_INVALID_ARG;
    }
    return ToAirShareResult(airshare::AudioEngine::Instance().SetDeviceLatencyOffset(deviceId, latencyMs));
}

AIRSHARE_API int __stdcall AudioEngine_GetVersion(wchar_t* buffer, int bufferChars) {
    if (!buffer || bufferChars <= 0) {
        return AIRSHARE_ERR_INVALID_ARG;
    }
    wcsncpy_s(buffer, bufferChars, L"1.0.0", _TRUNCATE);
    return AIRSHARE_OK;
}
