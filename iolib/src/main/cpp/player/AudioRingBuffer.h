//
// Created by Dipak Sisodiya on 18/12/24.
//

#ifndef AUDIO_RING_BUFFER_H
#define AUDIO_RING_BUFFER_H

#include <vector>
#include <algorithm>

class AudioRingBuffer {
public:
    AudioRingBuffer(int32_t capacityFrames, int32_t channelCount);

    // Write frames into the buffer. Returns how many frames were actually written.
    int32_t write(const float* data, int32_t frames);

    // Read frames from the buffer. Returns how many frames were actually read.
    int32_t read(float* data, int32_t frames);

private:
    int32_t mCapacityFrames;
    int32_t mChannelCount;
    std::vector<float> mBuffer;
    int32_t mWriteIndex;
    int32_t mReadIndex;
    int32_t mFramesAvailable;
};

#endif // AUDIO_RING_BUFFER_H
