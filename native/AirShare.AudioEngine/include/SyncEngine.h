#pragma once

#include "RenderEngine.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace airshare {

class SyncEngine {
public:
    SyncEngine();
    ~SyncEngine();

    void Start(RenderEngine* renderEngine);
    void Stop();

    void SetLatencyOffset(const std::wstring& deviceId, int latencyMs);
    int GetLatency(const std::wstring& deviceId) const;

    using StatsCallback = void(__stdcall*)(const wchar_t* deviceId, int latencyMs, int bufferFillPercent, void* userData);

    void SetStatsCallback(StatsCallback callback, void* userData);

private:
    void SyncLoop();

    RenderEngine* renderEngine_ = nullptr;
    mutable std::mutex mutex_;
    std::unordered_map<std::wstring, int> latencyOffsetsMs_;
    StatsCallback statsCallback_ = nullptr;
    void* statsUserData_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{ false };
};

} // namespace airshare
