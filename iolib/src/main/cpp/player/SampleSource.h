/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _PLAYER_SAMPLESOURCE_
#define _PLAYER_SAMPLESOURCE_

#include <cstdint>
#include <android/log.h>

#include "DataSource.h"

#include "SampleBuffer.h"

namespace iolib {

/**
 * Defines an interface for audio data provided to a player object.
 * Concrete examples include OneShotSampleBuffer. One could imagine a LoopingSampleBuffer.
 * Supports stereo position via mPan member.
 */
class SampleSource: public DataSource {
public:
    // Pan position of the audio in a stereo mix
    // [left:-1.0f] <- [center: 0.0f] -> -[right: 1.0f]
    static constexpr float PAN_HARDLEFT = -1.0f;
    static constexpr float PAN_HARDRIGHT = 1.0f;
    static constexpr float PAN_CENTER = 0.0f;

    SampleSource(SampleBuffer *sampleBuffer, float pan)
     : mSampleBuffer(sampleBuffer), mCurSampleIndex(0), mIsPlaying(false), mGain(1.0f) {
        setPan(pan);
    }
    virtual ~SampleSource() {}

    void setPlayMode() { mCurSampleIndex = 0; mIsPlaying = true; }
    void setStopMode() { mIsPlaying = false; mCurSampleIndex = 0; }

    bool isPlaying() { return mIsPlaying; }

    void setPan(float pan) {
        if (pan < PAN_HARDLEFT) {
            mPan = PAN_HARDLEFT;
        } else if (pan > PAN_HARDRIGHT) {
            mPan = PAN_HARDRIGHT;
        } else {
            mPan = pan;
        }
        calcGainFactors();
    }

    float getPan() {
        return mPan;
    }

    void setGain(float gain) {
        mGain = gain;
        calcGainFactors();
    }

    float getGain() {
        return mGain;
    }

    void seekToFrame(int32_t frameOffset) {
        if (!mSampleBuffer) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource", "seekToFrame: Sample buffer is null");
            return;
        }

        float* newFramePointer = mSampleBuffer->getPointerToFrame(frameOffset);
        if (!newFramePointer) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource",
                                "seekToFrame: Could not set frame offset %d", frameOffset);
            return;
        }

        // Update the current frame index and internal playback pointer
        mCurSampleIndex = frameOffset;

        __android_log_print(ANDROID_LOG_INFO, "SampleSource",
                            "seekToFrame: Successfully set to frame %d", frameOffset);
    }

    int64_t getCurrentPositionInMillis(int32_t sampleRate, int32_t channelCount) const {
        if (!mSampleBuffer) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource", "getCurrentPositionInMillis: Sample buffer is null");
            return 0;
        }

        // Retrieve sample rate and channel count from SampleBuffer
        if (sampleRate <= 0 || channelCount <= 0) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource",
                                "getCurrentPositionInMillis: Invalid sample rate (%d) or channel count (%d)",
                                sampleRate, channelCount);
            return 0;
        }

        // Calculate the playback position in milliseconds
        int64_t currentTimeMillis = (static_cast<int64_t>(mCurSampleIndex) * 1000) /
                                    (sampleRate * channelCount);

        return currentTimeMillis;
    }


    int64_t getDurationInMillis(int32_t sampleRate, int32_t channelCount) {
        if (!mSampleBuffer) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource", "getDurationInMillis: Sample buffer is null");
            return 0;
        }

        if (sampleRate <= 0 || channelCount <= 0) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource",
                                "getDurationInMillis: Invalid sample rate (%d) or channel count (%d)",
                                sampleRate, channelCount);
            return 0;
        }

        // Retrieve the total number of samples from the SampleBuffer
        int64_t totalSamples = mSampleBuffer->getTotalSamples();
        if (totalSamples <= 0) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleSource",
                                "getDurationInMillis: Invalid total sample count");
            return 0;
        }

        // Calculate the duration in milliseconds
        int64_t durationMillis = (totalSamples * 1000) / (sampleRate * channelCount);

        return durationMillis;
    }

protected:
    SampleBuffer    *mSampleBuffer;

    int32_t mCurSampleIndex;

    bool mIsPlaying;

    // Logical pan value
    float mPan;

    // precomputed channel gains for pan
    float mLeftGain;
    float mRightGain;

    // Overall gain
    float mGain;

private:
    void calcGainFactors() {
        // useful panning information: http://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
        float rightPan = (mPan * 0.5) + 0.5;
        mRightGain = rightPan * mGain;
        mLeftGain = (1.0 - rightPan) * mGain;    }
};


} // namespace wavlib

#endif //_PLAYER_SAMPLESOURCE_
