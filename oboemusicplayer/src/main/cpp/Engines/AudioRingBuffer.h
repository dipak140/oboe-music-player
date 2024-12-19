//
// Created by Dipak Sisodiya on 19/12/24.
//

#ifndef OBOE_MP3_PLAYER_AUDIORINGBUFFER_H
#define OBOE_MP3_PLAYER_AUDIORINGBUFFER_H

#include <vector>
#include <algorithm>

class AudioRingBuffer {
public:
    AudioRingBuffer(int32_t capacityFrames, int32_t channelCount)
            : mCapacityFrames(capacityFrames),
              mChannelCount(channelCount),
              mBuffer(capacityFrames * channelCount, 0.0f),
              mWriteIndex(0), mReadIndex(0), mFramesAvailable(0) {}

    int32_t write(const float* data, int32_t frames) {
        int32_t framesToWrite = std::min(frames, mCapacityFrames - mFramesAvailable);
        int32_t samplesToWrite = framesToWrite * mChannelCount;
        int32_t capacitySamples = mCapacityFrames * mChannelCount;
        int32_t writePos = mWriteIndex * mChannelCount;

        for (int32_t i = 0; i < samplesToWrite; ++i) {
            mBuffer[(writePos + i) % capacitySamples] = data[i];
        }

        mWriteIndex = (mWriteIndex + framesToWrite) % mCapacityFrames;
        mFramesAvailable += framesToWrite;
        return framesToWrite;
    }

    int32_t read(float* data, int32_t frames) {
        int32_t framesToRead = std::min(frames, mFramesAvailable);
        int32_t samplesToRead = framesToRead * mChannelCount;
        int32_t capacitySamples = mCapacityFrames * mChannelCount;
        int32_t readPos = mReadIndex * mChannelCount;

        for (int32_t i = 0; i < samplesToRead; ++i) {
            data[i] = mBuffer[(readPos + i) % capacitySamples];
        }

        mReadIndex = (mReadIndex + framesToRead) % mCapacityFrames;
        mFramesAvailable -= framesToRead;
        return framesToRead;
    }

    int32_t getFramesAvailable() const { return mFramesAvailable; }

private:
    int32_t mCapacityFrames;
    int32_t mChannelCount;
    std::vector<float> mBuffer;
    int32_t mWriteIndex;
    int32_t mReadIndex;
    int32_t mFramesAvailable;
};

#endif // AUDIO_RING_BUFFER_H

