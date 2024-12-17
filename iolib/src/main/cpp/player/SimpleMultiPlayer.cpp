/*
 * Copyright 2019 The Android Open Source Project
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

#include <android/log.h>

// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>
#include <inttypes.h>
#include <chrono>

// local includes
#include "OneShotSampleSource.h"
#include "SimpleMultiPlayer.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/Definitions.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/AudioStream.h"

static const char* TAG = "SimpleMultiPlayer";

using namespace oboe;
using namespace parselib;

namespace iolib {

constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)

SimpleMultiPlayer::SimpleMultiPlayer()
  : mChannelCount(0), mOutputReset(false), mSampleRate(0), mNumSampleBuffers(0)
{}

DataCallbackResult SimpleMultiPlayer::MyDataCallback::onAudioReady(AudioStream *oboeStream,
                                                                   void *audioData,
                                                                   int32_t numFrames) {

    StreamState streamState = oboeStream->getState();

    if (!this->mParent->firstFrameHit) {
        // Get the current timestamp
        oboe::Result result = oboeStream->getTimestamp(CLOCK_BOOTTIME, &mParent->framePosition, &mParent->presentationTime);
        mParent->presentationTime =std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        mParent->framePosition = numFrames;
        this->mParent->firstFrameHit = true;
    }

    if (streamState != StreamState::Open && streamState != StreamState::Started) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState:%d", streamState);
    }
    if (streamState == StreamState::Disconnected) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState::Disconnected");
    }

    // Testing changes
    if (streamState == StreamState::Paused){
        __android_log_print(ANDROID_LOG_ERROR, TAG, "  streamState::Paused");
        return DataCallbackResult::Continue;
    }

    memset(audioData, 0, static_cast<size_t>(numFrames) * static_cast<size_t>
            (mParent->mChannelCount) * sizeof(float));

    // OneShotSampleSource* sources = mSampleSources.get();
    for(int32_t index = 0; index < mParent->mNumSampleBuffers; index++) {
        if (mParent->mSampleSources[index]->isPlaying()) {
            mParent->mSampleSources[index]->mixAudio((float*)audioData, mParent->mChannelCount,
                                                     numFrames);
        }
    }

    return DataCallbackResult::Continue;
}

    long long currentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

void SimpleMultiPlayer::MyErrorCallback::onErrorAfterClose(AudioStream *oboeStream, Result error) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "==== onErrorAfterClose() error:%d", error);

    mParent->resetAll();
    if (mParent->openStream() && mParent->startStream()) {
        mParent->mOutputReset = true;
    }
}

int64_t SimpleMultiPlayer::getFramePosition() {
    return framePosition;
}


int64_t SimpleMultiPlayer::getFrameTimeStamp() {
    return presentationTime;
}


void SimpleMultiPlayer::seekTo(int64_t positionMillis, int sampleRate, int channel) {
    if (positionMillis < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "seekTo: Invalid position %lld ms", positionMillis);
        return;
    }

    int64_t frameOffset = (positionMillis * sampleRate * channel) / 1000;
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "seekTo: mSampleRate to %lld (%lld channelcount)", sampleRate, channel);

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "seekTo: Seeking to %lld ms (%lld frames)",
                        positionMillis, frameOffset);

    // Apply the seek to all active sample sources
    for (int32_t i = 0; i < mNumSampleBuffers; ++i) {
        if (mSampleSources[i]->isPlaying()) {
            mSampleSources[i]->seekToFrame(frameOffset);
        }
    }
}

    int64_t SimpleMultiPlayer::getPosition(int index, int sampleRate, int channelCount) {
        return mSampleSources[index]->getCurrentPositionInMillis(sampleRate, channelCount);
}

int64_t SimpleMultiPlayer::getDuration(int mSampleRate, int mChannelCount){
    return mSampleSources[0]->getDurationInMillis(mSampleRate, mChannelCount);
}

int SimpleMultiPlayer::getAudioSessionId() {
    return mAudioStream->getSessionId();
}

bool SimpleMultiPlayer::openStream() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "openStream()");

    // Use shared_ptr to prevent use of a deleted callback.
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);

    // Create an audio stream
    AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount);
    // we will resample source data to device rate, so take default sample rate
    builder.setDataCallback(mDataCallback);
    builder.setErrorCallback(mErrorCallback);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Shared);
    builder.setSampleRateConversionQuality(SampleRateConversionQuality::Medium);

    Result result = builder.openStream(mAudioStream);
    if (result != Result::OK){
        __android_log_print(
                ANDROID_LOG_ERROR,
                TAG,
                "openStream failed. Error: %s", convertToText(result));
        return false;
    }

    // Reduce stream latency by setting the buffer size to a multiple of the burst size
    // Note: this will fail with ErrorUnimplemented if we are using a callback with OpenSL ES
    // See oboe::AudioStreamBuffered::setBufferSizeInFrames
    result = mAudioStream->setBufferSizeInFrames(
            mAudioStream->getFramesPerBurst() * kBufferSizeInBursts);
    if (result != Result::OK) {
        __android_log_print(
                ANDROID_LOG_WARN,
                TAG,
                "setBufferSizeInFrames failed. Error: %s", convertToText(result));
    }

    mSampleRate = mAudioStream->getSampleRate();

    return true;
}

// Just trying to open the stream if it is not already open.
bool SimpleMultiPlayer::startStream() {
    int tryCount = 0;
    while (tryCount < 3) {
        bool wasOpenSuccessful = true;
        // Assume that apenStream() was called successfully before startStream() call.
        if (tryCount > 0) {
            usleep(20 * 1000); // Sleep between tries to give the system time to settle.
            wasOpenSuccessful = openStream(); // Try to open the stream again after the first try.
        }
        if (wasOpenSuccessful) {
            Result result = mAudioStream->requestStart();
            if (result != Result::OK){
                __android_log_print(
                        ANDROID_LOG_ERROR,
                        TAG,
                        "requestStart failed. Error: %s", convertToText(result));
                mAudioStream->close();
                mAudioStream.reset();
            } else {
                return true;
            }
        }
        tryCount++;
    }

    return false;
}

void SimpleMultiPlayer::setupAudioStream(int32_t channelCount) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
    mChannelCount = channelCount;

    openStream();
}

void SimpleMultiPlayer::teardownAudioStream() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "teardownAudioStream()");
    this->firstFrameHit = false;
    // tear down the player
    if (mAudioStream) {
        mAudioStream->stop();
        mAudioStream->close();
        mAudioStream.reset();
    }
}

void SimpleMultiPlayer::pauseStream(){
    __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream()");
    if (mAudioStream && !mIsStreamPaused) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream() called");
        mAudioStream->pause();
        mAudioStream->flush(); // If available, clear pending buffers
        mIsStreamPaused = true;
        // More stuff needed to be done here
    }
}

void SimpleMultiPlayer::resumeStream() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "resumeStream()");
        if (mAudioStream && mIsStreamPaused) {
            mAudioStream->requestStart();  // Restart the stream
            mIsStreamPaused = false;       // Reset paused flag
        }
}


void SimpleMultiPlayer::addSampleSource(SampleSource* source, SampleBuffer* buffer) {
    buffer->resampleData(mSampleRate);

    mSampleBuffers.push_back(buffer);
    mSampleSources.push_back(source);
    mNumSampleBuffers++;
}

void SimpleMultiPlayer::unloadSampleData() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "unloadSampleData()");
    resetAll();

    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        delete mSampleBuffers[bufferIndex];
        delete mSampleSources[bufferIndex];
    }

    mSampleBuffers.clear();
    mSampleSources.clear();

    mNumSampleBuffers = 0;
}

void SimpleMultiPlayer::triggerDown(int32_t index) {
    if (index < mNumSampleBuffers) {
        // print timestamp of sample source
        struct timespec ts;
        // Get the time from CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &ts);
        // Convert to milliseconds
        long long currentTimeMillis = (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
        __android_log_print(ANDROID_LOG_INFO, TAG, "timestamp at triggerDown(): %lld", currentTimeMillis);
        __android_log_print(ANDROID_LOG_INFO, TAG, "triggerDown(%d)", index);

        mSampleSources[index]->setPlayMode();
    }
}

void SimpleMultiPlayer::triggerUp(int32_t index) {
    this->firstFrameHit = false;
    if (index < mNumSampleBuffers) {
        mSampleSources[index]->setStopMode();
    }
}

void SimpleMultiPlayer::resetAll() {
    for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
        mSampleSources[bufferIndex]->setStopMode();
    }
}

void SimpleMultiPlayer::setPan(int index, float pan) {
    mSampleSources[index]->setPan(pan);
}

float SimpleMultiPlayer::getPan(int index) {
    return mSampleSources[index]->getPan();
}

void SimpleMultiPlayer::setGain(int index, float gain) {
    mSampleSources[index]->setGain(gain);
}

float SimpleMultiPlayer::getGain(int index) {
    return mSampleSources[index]->getGain();
}

}
