#pragma once

#include "RingBuffer.h"

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace airshare {

struct RenderDeviceState {
    std::wstring deviceId;
    std::wstring deviceName;
    int latencyMs = 0;
    int readOffsetFrames = 0;
    size_t consumerReadPos = 0;
    float volume = 1.0f;
    int bufferFillPercent = 0;
    std::atomic<bool> active{ true };
};

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    bool StartDevice(
        const std::wstring& deviceId,
        RingBuffer* ringBuffer,
        int readOffsetFrames,
        int latencyMs,
        float volume,
        int preferredBufferFrames);

    void StopAll();
    void SetDeviceVolume(const std::wstring& deviceId, float gain);
    void SetReadOffset(const std::wstring& deviceId, int offsetFrames);
    int GetBufferFillPercent(const std::wstring& deviceId) const;
    bool IsRunning() const;

    std::vector<std::shared_ptr<RenderDeviceState>> GetDeviceStates() const;

private:
    struct RenderContext;

    void RenderLoop(std::shared_ptr<RenderContext> context);

    RingBuffer* ringBuffer_ = nullptr;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<RenderContext>> contexts_;
    std::atomic<bool> running_{ false };
};

} // namespace airshare
