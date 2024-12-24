//
// Created by Dipak Sisodiya on 18/12/24.
//

#include "AudioRingBuffer.h"

AudioRingBuffer::AudioRingBuffer(int32_t capacityFrames, int32_t channelCount)
        : mCapacityFrames(capacityFrames),
          mChannelCount(channelCount),
          mBuffer(static_cast<size_t>(capacityFrames * channelCount), 0.0f),
          mWriteIndex(0),
          mReadIndex(0),
          mFramesAvailable(0) {
}

int32_t AudioRingBuffer::write(const float* data, int32_t frames) {
    int32_t framesToWrite = std::min(frames, mCapacityFrames - mFramesAvailable);
    int32_t samplesToWrite = framesToWrite * mChannelCount;

    int32_t writePos = mWriteIndex * mChannelCount;
    int32_t capacitySamples = mCapacityFrames * mChannelCount;

    for (int32_t i = 0; i < samplesToWrite; ++i) {
        mBuffer[(writePos + i) % capacitySamples] = data[i];
    }

    mWriteIndex = (mWriteIndex + framesToWrite) % mCapacityFrames;
    mFramesAvailable += framesToWrite;

    return framesToWrite;
}

int32_t AudioRingBuffer::read(float* data, int32_t frames) {
    int32_t framesToRead = std::min(frames, mFramesAvailable);
    int32_t samplesToRead = framesToRead * mChannelCount;

    int32_t readPos = mReadIndex * mChannelCount;
    int32_t capacitySamples = mCapacityFrames * mChannelCount;

    for (int32_t i = 0; i < samplesToRead; ++i) {
        data[i] = mBuffer[(readPos + i) % capacitySamples];
    }

    mReadIndex = (mReadIndex + framesToRead) % mCapacityFrames;
    mFramesAvailable -= framesToRead;

    // If not enough data was available, the remainder of 'data' is unchanged.
    // Callers should handle less data read if needed.

    return framesToRead;
}
