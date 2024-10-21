//
// Created by Dipak Sisodiya on 19/10/24.
// Email: dipak@reconv.in
//

#include "AudioEngine.h"

using namespace little_endian_io;

AudioEngine::AudioEngine()
        : g_javaVM(nullptr), g_callbackObject(nullptr),
          isRecording(false), isPlaying(false),
          mSampleRate(44100),  // Set a default sample rate
          mRecordingDeviceId(0), mPlaybackDeviceId(0),
          mAudioApi(oboe::AudioApi::AAudio),
          mFormat(oboe::AudioFormat::I16) {}

AudioEngine::~AudioEngine() {
    stopRecordingAndPlaying();  // Ensure everything is stopped in the destructor
}

// Implementation of public methods
void AudioEngine::startRecording(const char* filePath) {
    if (openRecordingStream() == oboe::Result::OK) {
        isRecording = true;
        __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Recording started at %s", filePath);
        // Additional logic to handle notifications and file writing
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", "Failed to start recording.");
    }
}

void AudioEngine::startPlayback(const char* wavFilePath) {
    if (openPlaybackStream() == oboe::Result::OK) {
        isPlaying = true;
        __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Playback started from %s", wavFilePath);
        // Additional logic to handle playback notifications
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", "Failed to start playback.");
    }
}

void AudioEngine::stopRecording() {
    closeRecordingStream();
    isRecording = false;
    __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Recording stopped.");
}

void AudioEngine::stopPlayback() {
    closePlaybackStream();
    isPlaying = false;
    __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Playback stopped.");
}

void AudioEngine::startRecordingAndPlaying(const char* filePath, const char* wavFilePath) {
    startRecording(filePath);   // Start recording
    startPlayback(wavFilePath); // Start playback
    if (isRecording && isPlaying) {
        __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Recording and playback started simultaneously.");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", "Failed to start recording and playback.");
    }
}

void AudioEngine::stopRecordingAndPlaying() {
    stopRecording();  // Stop the recording
    stopPlayback();   // Stop the playback
    __android_log_print(ANDROID_LOG_INFO, "AudioEngine", "Recording and playback stopped.");
}

// Implementation of private methods
oboe::Result AudioEngine::openRecordingStream() {
    // Implementation to open recording stream
    return oboe::Result::OK; // Placeholder
}

oboe::Result AudioEngine::openPlaybackStream() {
    // Implementation to open playback stream
    return oboe::Result::OK; // Placeholder
}

void AudioEngine::closeRecordingStream() {
    // Implementation to close recording stream
}

void AudioEngine::closePlaybackStream() {
    // Implementation to close playback stream
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream* oboeStream, void* audioData, int32_t numFrames) {
    // Implementation of audio processing for recording/playback
    return oboe::DataCallbackResult::Continue; // Placeholder
}

void AudioEngine::warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream>& stream) {
    // Implementation to warn if not low latency
}

void AudioEngine::callJavaMethod(const char* methodName, const char* methodSignature, const char* str) {
    // Implementation for calling Java methods
}
