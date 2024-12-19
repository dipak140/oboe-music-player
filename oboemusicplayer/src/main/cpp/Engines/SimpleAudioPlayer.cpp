//
// Created by Dipak Sisodiya on 19/12/24.
//

#include <android/log.h>

// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>
#include <inttypes.h>
#include <chrono>
// local includes
#include <player/OneShotSampleSource.h>
#include <jni.h>
#include "SimpleAudioPlayer.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/Definitions.h"
#include "../../../../../oboemusicplayer/oboe/include/oboe/AudioStream.h"

static const char* TAG = "SimpleAudioPlayer";
using namespace oboe;
using namespace parselib;

namespace iolib {
    constexpr int32_t kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)

    static JNIEnv* getJNIEnv() {
        if (!g_JavaVM) return nullptr;
        JNIEnv* env = nullptr;
        if (g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (g_JavaVM->AttachCurrentThread(&env, nullptr) != 0) {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to attach thread to JVM");
                return nullptr;
            }
        }
        return env;
    }


    SimpleAudioPlayer::SimpleAudioPlayer()
            : mChannelCount(0), mSampleRate(0),mRingBuffer(2048, 2), mNumSampleBuffers(0),  mOutputReset(false)
    {}

    DataCallbackResult SimpleAudioPlayer::MyDataCallback::onAudioReady(
            AudioStream *oboeStream, void *audioData, int32_t numFrames) {

        StreamState streamState = oboeStream->getState();

        // Capture the first frame's timestamp
        if (!this->mParent->firstFrameHit) {
            mParent->presentationTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            mParent->framePosition = numFrames;
            this->mParent->firstFrameHit = true;
        }

        // Log the stream state
        if (streamState != StreamState::Open && streamState != StreamState::Started) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "StreamState: %d", streamState);
        }

        // Handle disconnected state
        if (streamState == StreamState::Disconnected) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "StreamState::Disconnected");
            return DataCallbackResult::Stop;
        }

        // Handle paused state
        if (streamState == StreamState::Paused) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "StreamState::Paused");
            return DataCallbackResult::Continue;
        }

        // Clear the audio buffer
        memset(audioData, 0, static_cast<size_t>(numFrames) *
                             static_cast<size_t>(mParent->mChannelCount) * sizeof(float));

        // Mix audio from sample sources
        for (int32_t index = 0; index < mParent->mNumSampleBuffers; index++) {
            if (mParent->mSampleSources[index]->isPlaying()) {
                mParent->mSampleSources[index]->mixAudio(
                        static_cast<float *>(audioData), mParent->mChannelCount, numFrames);
            }
        }

        // Send audio data directly to Java callback
        if (gJavaCallbackObj && gOnAudioDataAvailableMethod) {
            JNIEnv *env = getJNIEnv();
            if (env) {
                int32_t numSamples = numFrames * mParent->mChannelCount;

                // Convert float audio data to int16
                std::vector<int16_t> shortAudioData(numSamples);
                float *floatAudioData = static_cast<float *>(audioData);
                for (int i = 0; i < numSamples; ++i) {
                    // Convert float to int16 with clamping
                    shortAudioData[i] = static_cast<int16_t>(
                            std::min(std::max(floatAudioData[i], -1.0f), 1.0f) * 32767);
                }

                // Prepare Java byte array
                int32_t numBytes = numSamples * sizeof(int16_t);
                jbyteArray javaArray = env->NewByteArray(numBytes);
                env->SetByteArrayRegion(javaArray, 0, numBytes,
                                        reinterpret_cast<const jbyte *>(shortAudioData.data()));

                // Call the Java method
                env->CallVoidMethod(gJavaCallbackObj, gOnAudioDataAvailableMethod, javaArray);

                // Clean up local reference
                env->DeleteLocalRef(javaArray);
            }
        }

        return DataCallbackResult::Continue;
    }


    long long currentTimeMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    void SimpleAudioPlayer::MyErrorCallback::onErrorAfterClose(AudioStream *oboeStream, Result error) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "==== onErrorAfterClose() error:%d", error);

        mParent->resetAll();
        if (mParent->openStream() && mParent->startStream()) {
            mParent->mOutputReset = true;
        }
    }

    int64_t SimpleAudioPlayer::getFramePosition() {
        return framePosition;
    }


    int64_t SimpleAudioPlayer::getFrameTimeStamp() {
        return presentationTime;
    }


    void SimpleAudioPlayer::seekTo(int64_t positionMillis, int sampleRate, int channel) {
        if (positionMillis < 0) {
            return;
        }

        int64_t frameOffset = (positionMillis * sampleRate * channel) / 1000;

        // Apply the seek to all active sample sources
        for (int32_t i = 0; i < mNumSampleBuffers; ++i) {
            if (mSampleSources[i]->isPlaying()) {
                mSampleSources[i]->seekToFrame(frameOffset);
            }
        }
    }

    int64_t SimpleAudioPlayer::getPosition(int index, int sampleRate, int channelCount) {
        return mSampleSources[index]->getCurrentPositionInMillis(sampleRate, channelCount);
    }

    int64_t SimpleAudioPlayer::getDuration(int i, int channelCount){
        return mSampleSources[0]->getDurationInMillis(i, channelCount);
    }

    int SimpleAudioPlayer::getAudioSessionId() {
        return mAudioStream->getSessionId();
    }

    bool SimpleAudioPlayer::openStream() {
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
    bool SimpleAudioPlayer::startStream() {
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

    void SimpleAudioPlayer::setupAudioStream(int32_t channelCount) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "setupAudioStream()");
        mChannelCount = channelCount;

        openStream();
    }

    void SimpleAudioPlayer::teardownAudioStream() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "teardownAudioStream()");
        this->firstFrameHit = false;
        // tear down the player
        if (mAudioStream) {
            mAudioStream->stop();
            mAudioStream->close();
            mAudioStream.reset();
        }
    }

    void SimpleAudioPlayer::pauseStream(){
        __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream()");
        if (mAudioStream && !mIsStreamPaused) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "pauseStream() called");
            mAudioStream->pause();
            mAudioStream->flush(); // If available, clear pending buffers
            mIsStreamPaused = true;
            // More stuff needed to be done here
        }
    }

    void SimpleAudioPlayer::resumeStream() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "resumeStream()");
        if (mAudioStream && mIsStreamPaused) {
            mAudioStream->requestStart();  // Restart the stream
            mIsStreamPaused = false;       // Reset paused flag
        }
    }


    void SimpleAudioPlayer::addSampleSource(SampleSource* source, SampleBuffer* buffer) {
        buffer->resampleData(mSampleRate);

        mSampleBuffers.push_back(buffer);
        mSampleSources.push_back(source);
        mNumSampleBuffers++;
    }

    void SimpleAudioPlayer::unloadSampleData() {
        __android_log_print(ANDROID_LOG_INFO, TAG, "unloadSampleData()");
        resetAll();

        for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
            delete mSampleSources[bufferIndex];
        }

        mSampleBuffers.clear();
        mSampleSources.clear();

        mNumSampleBuffers = 0;
    }

    void SimpleAudioPlayer::triggerDown(int32_t index) {
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

    void SimpleAudioPlayer::triggerUp(int32_t index) {
        this->firstFrameHit = false;
        if (index < mNumSampleBuffers) {
            mSampleSources[index]->setStopMode();
        }
    }

    void SimpleAudioPlayer::resetAll() {
        for (int32_t bufferIndex = 0; bufferIndex < mNumSampleBuffers; bufferIndex++) {
            mSampleSources[bufferIndex]->setStopMode();
        }
    }

    void SimpleAudioPlayer::setPan(int index, float pan) {
        mSampleSources[index]->setPan(pan);
    }

    float SimpleAudioPlayer::getPan(int index) {
        return mSampleSources[index]->getPan();
    }

    void SimpleAudioPlayer::setGain(int index, float gain) {
        mSampleSources[index]->setGain(gain);
    }

    float SimpleAudioPlayer::getGain(int index) {
        return mSampleSources[index]->getGain();
    }

}
