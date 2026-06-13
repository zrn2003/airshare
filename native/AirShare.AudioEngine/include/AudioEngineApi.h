#pragma once

#ifdef AIRSHARE_AUDIOENGINE_EXPORTS
#define AIRSHARE_API __declspec(dllexport)
#else
#define AIRSHARE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AIRSHARE_OK 0
#define AIRSHARE_ERR_NOT_INITIALIZED -1
#define AIRSHARE_ERR_ALREADY_RUNNING -2
#define AIRSHARE_ERR_INVALID_ARG -3
#define AIRSHARE_ERR_DEVICE -4
#define AIRSHARE_ERR_ENGINE -5

typedef struct AirShareDeviceInfo {
    wchar_t id[256];
    wchar_t name[256];
    wchar_t type[32];
    int latencyMs;
    int isConnected;
} AirShareDeviceInfo;

typedef void(__stdcall* AirShareDeviceCallback)(const AirShareDeviceInfo* device, int added, void* userData);
typedef void(__stdcall* AirShareStatsCallback)(const wchar_t* deviceId, int latencyMs, int bufferFillPercent, void* userData);

AIRSHARE_API int __stdcall AudioEngine_Initialize(void);
AIRSHARE_API void __stdcall AudioEngine_Shutdown(void);
AIRSHARE_API int __stdcall AudioEngine_IsInitialized(void);

AIRSHARE_API int __stdcall AudioEngine_EnumerateDevices(AirShareDeviceInfo* devices, int maxCount, int* outCount);
AIRSHARE_API int __stdcall AudioEngine_RefreshDevices(void);
AIRSHARE_API void __stdcall AudioEngine_SetDeviceCallback(AirShareDeviceCallback callback, void* userData);
AIRSHARE_API void __stdcall AudioEngine_SetStatsCallback(AirShareStatsCallback callback, void* userData);

AIRSHARE_API int __stdcall AudioEngine_StartSharing(const wchar_t** deviceIds, int count, int bufferSizeFrames);
AIRSHARE_API int __stdcall AudioEngine_StopSharing(void);
AIRSHARE_API int __stdcall AudioEngine_IsSharing(void);

AIRSHARE_API int __stdcall AudioEngine_SetMasterVolume(float gain);
AIRSHARE_API int __stdcall AudioEngine_SetDeviceVolume(const wchar_t* deviceId, float gain);
AIRSHARE_API int __stdcall AudioEngine_GetDeviceLatency(const wchar_t* deviceId, int* latencyMs);
AIRSHARE_API int __stdcall AudioEngine_SetDeviceLatencyOffset(const wchar_t* deviceId, int latencyMs);

AIRSHARE_API int __stdcall AudioEngine_GetVersion(wchar_t* buffer, int bufferChars);

#ifdef __cplusplus
}
#endif
