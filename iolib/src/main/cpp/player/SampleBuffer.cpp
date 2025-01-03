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

#include "SampleBuffer.h"

// Resampler Includes
#include <resampler/MultiChannelResampler.h>
#include <android/log.h>

#include "wav/WavStreamReader.h"

using namespace RESAMPLER_OUTER_NAMESPACE::resampler;

namespace iolib {

int32_t SampleBuffer::getFrameCount() const {
        // Total frames = Total samples / Channels
        return mNumSamples / mAudioProperties.channelCount;
}

float* SampleBuffer::getPointerToFrame(int32_t frameOffset) const {
        // Validate frame offset
    int32_t totalFrames = getFrameCount();
    if (frameOffset < 0 || (frameOffset >= totalFrames*mAudioProperties.channelCount)) {
        __android_log_print(ANDROID_LOG_ERROR, "SampleBuffer",
                            "getPointerToFrame: Invalid frame offset %d", frameOffset);
        return nullptr;
    }

    // Calculate the pointer to the start of the desired frame
    return mSampleData + (frameOffset * mAudioProperties.channelCount);
}

void SampleBuffer::loadSampleData(parselib::WavStreamReader* reader) {
    __android_log_print(ANDROID_LOG_ERROR, "SampleBuffer", "loadSampleData called");
//    __android_log_print(ANDROID_LOG_ERROR, "SampleBuffer",
//                        "%s", reinterpret_cast<const char *>(reader->getNumChannels()));
//    __android_log_print(ANDROID_LOG_ERROR, "SampleBuffer",
//                        "%s", reinterpret_cast<const char *>(reader->getSampleRate()));
    mAudioProperties.channelCount = reader->getNumChannels();
    mAudioProperties.sampleRate = reader->getSampleRate();

    reader->positionToAudio();

    mNumSamples = reader->getNumSampleFrames() * reader->getNumChannels();
    mSampleData = new float[mNumSamples];

    reader->getDataFloat(mSampleData, reader->getNumSampleFrames());
}

void SampleBuffer::unloadSampleData() {
    if (mSampleData != nullptr) {
        delete[] mSampleData;
        mSampleData = nullptr;
    }
    mNumSamples = 0;
}

class ResampleBlock {
public:
    int32_t mSampleRate;
    float*  mBuffer;
    int32_t mNumSamples;
};

void resampleData(const ResampleBlock& input, ResampleBlock* output, int numChannels) {
    // Calculate output buffer size
    double temp =
            ((double)input.mNumSamples * (double)output->mSampleRate) / (double)input.mSampleRate;

    // round up
    int32_t numOutFramesAllocated = (int32_t)(temp + 0.5);
    // We iterate thousands of times through the loop. Roundoff error could accumulate
    // so add a few more frames for padding
    numOutFramesAllocated += 8;

    MultiChannelResampler *resampler = MultiChannelResampler::make(
            numChannels, // channel count
            input.mSampleRate, // input sampleRate
            output->mSampleRate, // output sampleRate
            MultiChannelResampler::Quality::Medium); // conversion quality

    float *inputBuffer = input.mBuffer;;     // multi-channel buffer to be consumed
    float *outputBuffer = new float[numOutFramesAllocated];    // multi-channel buffer to be filled
    output->mBuffer = outputBuffer;

    int numOutputSamples = 0;
    int inputSamplesLeft = input.mNumSamples;
    while ((inputSamplesLeft > 0) && (numOutputSamples < numOutFramesAllocated)) {
        if(resampler->isWriteNeeded()) {
            resampler->writeNextFrame(inputBuffer);
            inputBuffer += numChannels;
            inputSamplesLeft -= numChannels;
        } else {
            resampler->readNextFrame(outputBuffer);
            outputBuffer += numChannels;
            numOutputSamples += numChannels;
        }
    }
    output->mNumSamples = numOutputSamples;

    delete resampler;
}

void SampleBuffer::resampleData(int sampleRate) {
    if (mAudioProperties.sampleRate == sampleRate) {
        // nothing to do
        return;
    }

    ResampleBlock inputBlock;
    inputBlock.mBuffer = mSampleData;
    inputBlock.mNumSamples = mNumSamples;
    inputBlock.mSampleRate = mAudioProperties.sampleRate;

    ResampleBlock outputBlock;
    outputBlock.mSampleRate = sampleRate;
    iolib::resampleData(inputBlock, &outputBlock, mAudioProperties.channelCount);

    // delete previous samples
    delete[] mSampleData;

    // install the resampled data
    mSampleData = outputBlock.mBuffer;
    mNumSamples = outputBlock.mNumSamples;
    mAudioProperties.sampleRate = outputBlock.mSampleRate;
}

    int64_t SampleBuffer::getTotalSamples() {
        // Ensure the sample data has been loaded
        if (mSampleData == nullptr || mNumSamples <= 0) {
            __android_log_print(ANDROID_LOG_ERROR, "SampleBuffer",
                                "getTotalSamples: No valid sample data available");
            return 0;
        }

        // Return the total number of samples
        return static_cast<int64_t>(mNumSamples);
    }

} // namespace iolib
