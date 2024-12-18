//
// Created by Dipak Sisodiya on 18/12/24.
//

#include "VocalMusicPlayer.h"
// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>
#include <inttypes.h>
#include <chrono>

// local includes
#include "OneShotSampleSource.h"
#include "VocalMusicPlayer.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/Definitions.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/AudioStream.h"

static const char* TAG = "VocalMusicPlayer";

using namespace oboe;
using namespace parselib;

namespace iolib{
    constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)
    VocalMusicPlayer::VocalMusicPlayer() :
    mChannelCount(0),
    mOutputReset(false),
    mSampleRate(0),
    mNumSampleBuffers(0),
    mMicRingBuffer(2048, 2), // example: stereo, capacity 2048 frames
    mRunning(false)
    {}

    oboe::DataCallbackResult VocalMusicPlayer::MyDataCallback::onAudioReady(
            oboe::AudioStream *oboeStream,
            void *audioData,
            int32_t numFrames) {
        float *out = static_cast<float*>(audioData);
        memset(out, 0, numFrames * mParent->mChannelCount * sizeof(float));

        // Mix sample sources
        for (int32_t i = 0; i < mParent->mNumSampleBuffers; i++) {
            if (mParent->mSampleSources[i]->isPlaying()) {
                mParent->mSampleSources[i]->mixAudio(out, mParent->mChannelCount, numFrames);
            }
        }

        std::vector<float> micData(numFrames * mParent->mChannelCount, 0.0f);
        int framesRead = mParent->mMicRingBuffer.read(micData.data(), numFrames);
        for (int i = 0; i < framesRead * mParent->mChannelCount; i++) {
            out[i] += micData[i];
        }

        return DataCallbackResult::Continue;
    }

    bool VocalMusicPlayer::openOutputStream() {
        mDataCallback = std::make_shared<MyDataCallback>(this);
        mErrorCallback = std::make_shared<MyErrorCallback>(this);

        AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output)
                ->setChannelCount(mChannelCount)
                ->setDataCallback(mDataCallback)
                ->setErrorCallback(mErrorCallback)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Shared);

        auto result = builder.openStream(mOutputStream);
        if (result != oboe::Result::OK) {
                    __android_log_print(ANDROID_LOG_ERROR, TAG, "openOutputStream failed");
                    return false;
        }

        mSampleRate = mOutputStream->getSampleRate();
        return true;
    }

    bool VocalMusicPlayer::openInputStream() {
        AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Input)
                ->setChannelCount(mChannelCount)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Shared)
                ->setSampleRate(mSampleRate) // match output if possible
                ->setFormat(oboe::AudioFormat::Float);
        auto result = builder.openStream(mInputStream);
        if (result != oboe::Result::OK) {
                    __android_log_print(ANDROID_LOG_ERROR, TAG, "openInputStream failed");
                    return false;
        }
        return true;
    }

    long long currentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    void VocalMusicPlayer::MyErrorCallback::onErrorAfterClose(oboe::AudioStream *oboeStream,
                                                              oboe::Result error) {
        mParent->resetAll();
        if (mParent->openStream() && mParent->startStream()) {
            mParent->mOutputReset = true;
        }
    }

    int64_t VocalMusicPlayer::getFramePosition() {
        return framePosition;
    }


    int64_t VocalMusicPlayer::getFrameTimeStamp() {
        return presentationTime;
    }

    void VocalMusicPlayer::seekTo(int64_t positionMillis, int sampleRate, int channel) {
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
    int64_t VocalMusicPlayer::getPosition(int index, int sampleRate, int channelCount) {
        return mSampleSources[index]->getCurrentPositionInMillis(sampleRate, channelCount);
    }

    int64_t VocalMusicPlayer::getDuration(int mSampleRate, int mChannelCount){
        return mSampleSources[0]->getDurationInMillis(mSampleRate, mChannelCount);
    }

    int VocalMusicPlayer::getAudioSessionId() {
        return mOutputStream->getSessionId();
    }
    bool VocalMusicPlayer::openStream() {
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

        Result result = builder.openStream(mOutputStream);
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
        result = mOutputStream->setBufferSizeInFrames(
                mOutputStream->getFramesPerBurst() * kBufferSizeInBursts);
        if (result != Result::OK) {
            __android_log_print(
                    ANDROID_LOG_WARN,
                    TAG,
                    "setBufferSizeInFrames failed. Error: %s", convertToText(result));
        }

        mSampleRate = mOutputStream->getSampleRate();

        return true;
    }

// Just
// trying to open the stream if it is not already open.
//    bool VocalMusicPlayer::startStream() {
//        int tryCount = 0;
//        while (tryCount < 3) {
//            bool wasOpenSuccessful = true;
//            // Assume that apenStream() was called successfully before startStream() call.
//            if (tryCount > 0) {
//                usleep(20 * 1000); // Sleep between tries to give the system time to settle.
//                wasOpenSuccessful = openStream(); // Try to open the stream again after the first try.
//            }
//            if (wasOpenSuccessful) {
//                Result result = mOutputStream->requestStart();
//                if (result != Result::OK){
//                    __android_log_print(
//                            ANDROID_LOG_ERROR,
//                            TAG,
//                            "requestStart failed. Error: %s", convertToText(result));
//                    mOutputStream->close();
//                    mOutputStream.reset();
//                } else {
//                    return true;
//                }
//                if (mOutputStream && mOutputStream->requestStart() != oboe::Result::OK) {
//                    __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to start output stream");
//                    return false;
//                }
//            }
//            tryCount++;
//        }
//
//        return false;
//    }

    bool VocalMusicPlayer::startStream() {
        if (mOutputStream && mOutputStream->requestStart() != oboe::Result::OK) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to start output stream");
                return false;
            }
        if (mInputStream && mInputStream->requestStart() != oboe::Result::OK) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to start input stream");
                return false;
            }
        mRunning = true;
        mInputThread = std::thread(&VocalMusicPlayer::readInputLoop, this);
        return true;
    }

    void VocalMusicPlayer::stopStreams() {
        mRunning = false;
        if (mInputThread.joinable()) {
                    mInputThread.join();
        }
        if (mInputStream) {
            mInputStream->requestStop();
            mInputStream->close();
            mInputStream.reset();
        }
        if (mOutputReset){
            mOutputStream->requestStop();
            mOutputStream->close();
            mOutputStream.reset();
        }
    }

    void VocalMusicPlayer::readInputLoop() {
        int32_t bufferSize = 256;
        std::vector<float> inputBuffer(bufferSize * mChannelCount, 0.0f);
        while (mRunning) {
                auto result = mInputStream->read(inputBuffer.data(), bufferSize, 1000000L);
                if (result.value() > 0) {
                        mMicRingBuffer.write(inputBuffer.data(), result.value());
                    }
            }
    }



    void VocalMusicPlayer::setupAudioStream(int32_t channelCount) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
        mChannelCount = channelCount;

        openOutputStream();
        openInputStream();
    }

    void VocalMusicPlayer::teardownAudioStream() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "teardownAudioStream()");
        this->firstFrameHit = false;
        // tear down the player
        if (mOutputStream) {
            mOutputStream->stop();
            mOutputStream->close();
            mOutputStream.reset();
        }
    }

    void VocalMusicPlayer::pauseStream(){
        __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream()");
        if (mOutputStream && !mIsStreamPaused) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream() called");
            mOutputStream->pause();
            mOutputStream->flush(); // If available, clear pending buffers
            mIsStreamPaused = true;
            // More stuff needed to be done here
        }
    }

    void VocalMusicPlayer::resumeStream() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "resumeStream()");
        if (mOutputStream && mIsStreamPaused) {
            mOutputStream->requestStart();  // Restart the stream
            mIsStreamPaused = false;       // Reset paused flag
        }
    }


    void VocalMusicPlayer::addSampleSource(SampleSource* source, SampleBuffer* buffer) {
        buffer->resampleData(mSampleRate);

        mSampleBuffers.push_back(buffer);
        mSampleSources.push_back(source);
        mNumSampleBuffers++;
    }

    void VocalMusicPlayer::unloadSampleData() {
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

    void VocalMusicPlayer::triggerDown(int32_t index) {
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

    void VocalMusicPlayer::triggerUp(int32_t index) {
        this->firstFrameHit = false;
        if (index < mNumSampleBuffers) {
            mSampleSources[index]->setStopMode();
        }
    }

    void VocalMusicPlayer::resetAll() {
        for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
            mSampleSources[bufferIndex]->setStopMode();
        }
    }

    void VocalMusicPlayer::setPan(int index, float pan) {
        mSampleSources[index]->setPan(pan);
    }

    float VocalMusicPlayer::getPan(int index) {
        return mSampleSources[index]->getPan();
    }

    void VocalMusicPlayer::setGain(int index, float gain) {
        mSampleSources[index]->setGain(gain);
    }

    float VocalMusicPlayer::getGain(int index) {
        return mSampleSources[index]->getGain();
    }


}