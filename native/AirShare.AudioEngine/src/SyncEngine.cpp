#include "SyncEngine.h"

#include <Windows.h>
#include <algorithm>
#include <thread>

namespace airshare {

SyncEngine::SyncEngine() = default;

SyncEngine::~SyncEngine() {
    Stop();
}

void SyncEngine::Start(RenderEngine* renderEngine) {
    if (running_.load() || !renderEngine) {
        return;
    }

    renderEngine_ = renderEngine;
    running_.store(true);
    thread_ = std::thread(&SyncEngine::SyncLoop, this);
}

void SyncEngine::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    renderEngine_ = nullptr;
}

void SyncEngine::SetLatencyOffset(const std::wstring& deviceId, int latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    latencyOffsetsMs_[deviceId] = latencyMs;
}

int SyncEngine::GetLatency(const std::wstring& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = latencyOffsetsMs_.find(deviceId);
    return it != latencyOffsetsMs_.end() ? it->second : 0;
}

void SyncEngine::SetStatsCallback(StatsCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(mutex_);
    statsCallback_ = callback;
    statsUserData_ = userData;
}

void SyncEngine::SyncLoop() {
    while (running_.load()) {
        if (!renderEngine_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        const auto states = renderEngine_->GetDeviceStates();
        if (states.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        int maxFill = 0;
        int minFill = 100;
        std::shared_ptr<RenderDeviceState> maxFillState;
        std::shared_ptr<RenderDeviceState> minFillState;

        for (const auto& state : states) {
            if (!state || !state->active.load()) {
                continue;
            }

            const int fill = state->bufferFillPercent;
            if (fill > maxFill) {
                maxFill = fill;
                maxFillState = state;
            }
            if (fill < minFill) {
                minFill = fill;
                minFillState = state;
            }

            StatsCallback callback = nullptr;
            void* userData = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                callback = statsCallback_;
                userData = statsUserData_;
            }

            if (callback) {
                callback(state->deviceId.c_str(), state->latencyMs, fill, userData);
            }
        }

        // We no longer aggressively adjust readOffsetFrames here.
        // Differences in bufferFillPercent are often caused by different hardware endpoint buffer sizes,
        // not actual clock drift. Adjusting offsets dynamically here creates massive sync issues/echoes.

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace airshare
