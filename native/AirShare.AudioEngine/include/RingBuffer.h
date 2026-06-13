#pragma once

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace airshare {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacityFrames, size_t channels)
        : channels_(channels),
          capacity_(NextPowerOfTwo(capacityFrames)),
          mask_(capacity_ - 1),
          data_(capacity_ * channels_, 0.0f),
          writePos_(0),
          minConsumerReadPos_(0) {}

    size_t CapacityFrames() const { return capacity_; }
    size_t Channels() const { return channels_; }

    size_t WritePos() const { return writePos_.load(std::memory_order_acquire); }

    size_t AvailableToWrite() const {
        return capacity_;
    }

    void SetMinConsumerReadPos(size_t pos) {
        minConsumerReadPos_.store(pos, std::memory_order_release);
    }

    size_t AvailableForConsumer(size_t consumerReadPos, size_t syncOffsetFrames) const {
        const size_t w = writePos_.load(std::memory_order_acquire);
        if (w <= consumerReadPos + syncOffsetFrames) {
            return 0;
        }
        
        const size_t available = w - consumerReadPos - syncOffsetFrames;
        if (available > capacity_) {
            return capacity_;
        }
        return available;
    }

    size_t Write(const float* samples, size_t frameCount) {
        const size_t toWrite = frameCount;
        if (toWrite == 0) {
            return 0;
        }

        const size_t w = writePos_.load(std::memory_order_relaxed);
        const size_t first = std::min(toWrite, capacity_ - (w & mask_));

        CopyFrames(samples, 0, w & mask_, first);
        if (toWrite > first) {
            CopyFrames(samples, first, 0, toWrite - first);
        }

        writePos_.store(w + toWrite, std::memory_order_release);
        return toWrite;
    }

    size_t ReadForConsumer(
        float* destination,
        size_t frameCount,
        size_t& consumerReadPos,
        size_t syncOffsetFrames) const {

        const size_t available = AvailableForConsumer(consumerReadPos, syncOffsetFrames);
        const size_t toRead = std::min(frameCount, available);
        if (toRead == 0) {
            return 0;
        }

        const size_t w = writePos_.load(std::memory_order_acquire);
        size_t actualReadPos = consumerReadPos + syncOffsetFrames;
        
        // If the reader fell too far behind and was overwritten, jump to the oldest valid data
        if (w > actualReadPos + capacity_) {
            const size_t skipped = w - capacity_ - actualReadPos;
            actualReadPos += skipped;
            consumerReadPos += skipped;
        }

        const size_t first = std::min(toRead, capacity_ - (actualReadPos & mask_));
        ReadFrames(destination, 0, actualReadPos & mask_, first);
        if (toRead > first) {
            ReadFrames(destination, first, 0, toRead - first);
        }
        return toRead;
    }

    int FillPercent(size_t consumerReadPos, size_t syncOffsetFrames) const {
        const size_t available = AvailableForConsumer(consumerReadPos, syncOffsetFrames);
        return static_cast<int>((available * 100) / capacity_);
    }

    void Clear() {
        writePos_.store(0, std::memory_order_release);
        minConsumerReadPos_.store(0, std::memory_order_release);
        std::fill(data_.begin(), data_.end(), 0.0f);
    }

private:
    static size_t NextPowerOfTwo(size_t value) {
        size_t v = 1;
        while (v < value) {
            v <<= 1;
        }
        return v;
    }

    void CopyFrames(const float* src, size_t srcFrameOffset, size_t dstFrameOffset, size_t frameCount) {
        const size_t sampleCount = frameCount * channels_;
        std::memcpy(
            data_.data() + dstFrameOffset * channels_,
            src + srcFrameOffset * channels_,
            sampleCount * sizeof(float));
    }

    void ReadFrames(float* dst, size_t dstFrameOffset, size_t srcFrameOffset, size_t frameCount) const {
        const size_t sampleCount = frameCount * channels_;
        std::memcpy(
            dst + dstFrameOffset * channels_,
            data_.data() + srcFrameOffset * channels_,
            sampleCount * sizeof(float));
    }

    size_t channels_;
    size_t capacity_;
    size_t mask_;
    std::vector<float> data_;
    std::atomic<size_t> writePos_;
    std::atomic<size_t> minConsumerReadPos_;
};

} // namespace airshare
