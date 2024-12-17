#include <cassert>
#include <fstream>
#include <cstring>
#include <android/log.h>
#include "EarbackEngine.h"
#include "../../../../oboe/src/common/AudioClock.h"
#include <chrono>
#include <inttypes.h>  // For PRId64

namespace little_endian_io
{
    template <typename Word>
    std::ostream& write_word( std::ostream& outs, Word value, unsigned size = sizeof( Word ) )
    {
        for (; size; --size, value >>= 8)
            outs.put( static_cast <char> (value & 0xFF) );
        return outs;
    }
}

extern JavaVM* g_javaVM;
extern jobject g_callbackObject;


using namespace little_endian_io;

EarbackEngine::EarbackEngine() {
    assert(mOutputChannelCount == mInputChannelCount);
}

void EarbackEngine::setRecordingDeviceId(int32_t deviceId) {
    mRecordingDeviceId = deviceId;
}

void EarbackEngine::setPlaybackDeviceId(int32_t deviceId) {
    mPlaybackDeviceId = deviceId;
}

bool EarbackEngine::isAAudioRecommended() {
    return oboe::AudioStreamBuilder::isAAudioRecommended();
}

bool EarbackEngine::setAudioApi(oboe::AudioApi api) {
    if (mIsEffectOn) return false;
    mAudioApi = api;
    return true;
}

bool EarbackEngine::setEffectOn(bool isOn) {
    bool success = true;
    if (isOn != mIsEffectOn) {
        if (isOn) {
            // Open audio streams and start recording
            isStreamOpen = openPlaybackStream() == oboe::Result::OK;
            if (isStreamOpen) {
                mIsEffectOn = isOn;
            }
        } else {
            // Stop recording and close streams
            closeStreams();
            mIsEffectOn = isOn;
        }
    }
    return success;
}

oboe::Result EarbackEngine::openPlaybackStream() {
    oboe::AudioStreamBuilder inBuilder, outBuilder;
    setupPlaybackStreamParameters(&outBuilder);
    oboe::Result result = outBuilder.openStream(mPlayStream);
    if (result != oboe::Result::OK) {
        mSampleRate = oboe::kUnspecified;
        return result;
    } else {
        // The input stream needs to run at the same sample rate as the output.
        mSampleRate = mPlayStream->getSampleRate();
    }
    warnIfNotLowLatency(mPlayStream);

    setupRecordingStreamParameters(&inBuilder, mSampleRate);
    result = inBuilder.openStream(mRecordingStream);
    if (result != oboe::Result::OK) {
        closeStream(mPlayStream);
        return result;
    }
    warnIfNotLowLatency(mRecordingStream);

    mDuplexStream = std::make_unique<FullDuplexPass>();
    mDuplexStream->setSharedInputStream(mRecordingStream);
    mDuplexStream->setSharedOutputStream(mPlayStream);
    mDuplexStream->start();
    return result;
}

oboe::AudioStreamBuilder *EarbackEngine::setupRecordingStreamParameters(
        oboe::AudioStreamBuilder *builder, int32_t sampleRate) {
    builder->setDeviceId(mRecordingDeviceId)
            ->setDirection(oboe::Direction::Input)
            ->setSampleRate(sampleRate)
            ->setChannelCount(mInputChannelCount);
    return setupCommonStreamParameters(builder);
}

oboe::AudioStreamBuilder *EarbackEngine::setupPlaybackStreamParameters(
        oboe::AudioStreamBuilder *builder) {
    builder->setDataCallback(this)
            ->setErrorCallback(this)
            ->setDeviceId(mPlaybackDeviceId)
            ->setDirection(oboe::Direction::Output)
            ->setChannelCount(mOutputChannelCount);
    return setupCommonStreamParameters(builder);
}

oboe::AudioStreamBuilder *EarbackEngine::setupCommonStreamParameters(
        oboe::AudioStreamBuilder *builder) {
    builder->setAudioApi(mAudioApi)
            ->setFormat(mFormat)
            ->setFormatConversionAllowed(true)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency);
    return builder;
}

void EarbackEngine::closeStreams() {
    if (mDuplexStream) {
        mDuplexStream->stop();
    }
    closeStream(mPlayStream);
    closeStream(mRecordingStream);
    mDuplexStream.reset();
}

void EarbackEngine::closeStream(std::shared_ptr<oboe::AudioStream> &stream) {
    if (stream) {
        if (stream->getState() != oboe::StreamState::Closed) {
            oboe::Result result = stream->stop();
            if (result != oboe::Result::OK && result != oboe::Result::ErrorClosed) {
                __android_log_print(ANDROID_LOG_ERROR, "EarbackEngine", "Error stopping stream: %s", oboe::convertToText(result));
            }
            result = stream->close();
            if (result != oboe::Result::OK && result != oboe::Result::ErrorClosed) {
                __android_log_print(ANDROID_LOG_ERROR, "EarbackEngine", "Error closing stream: %s", oboe::convertToText(result));
            }
        }
        stream.reset();
    }
}

oboe::DataCallbackResult EarbackEngine::onAudioReady(
        oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {

    static bool firstFrameHit = false;
    static int64_t firstFrameTimestamp = 0;
    if (!firstFrameHit) {
        // Get the current timestamp
        int64_t framePosition;
        int64_t presentationTime;
        oboe::Result result = oboeStream->getTimestamp(CLOCK_REALTIME, &framePosition, &presentationTime);

        if (result == oboe::Result::OK) {
            firstFrameTimestamp = presentationTime;  // In nanoseconds
            __android_log_print(ANDROID_LOG_INFO, "OboeAudio", "First frame timestamp: %" PRId64 " ns", firstFrameTimestamp);
            __android_log_print(ANDROID_LOG_INFO, "OboeAudio", "First frame position: %" PRId64 "", framePosition);
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "OboeAudio", "Failed to get timestamp: %s", oboe::convertToText(result));
        }

        firstFrameHit = true;
    }
    if (audioData == nullptr || numFrames <= 0) {
        return oboe::DataCallbackResult::Stop;
    }

    // Process audio data
    float *floatData = static_cast<float*>(audioData);
    for (int i = 0; i < numFrames; i++) {
        floatData[i] = 0;  // Example of processing audio data
    }

    return mDuplexStream->onAudioReady(oboeStream, audioData, numFrames);
}


void EarbackEngine::warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream> &audioStream) {
    if (audioStream->getPerformanceMode() != oboe::PerformanceMode::LowLatency) {
        __android_log_print(ANDROID_LOG_WARN, "EarbackEngine", "Stream is NOT low latency.");
    }
}

void EarbackEngine::setVolume(float volume) {
    mVolume = volume;
}
