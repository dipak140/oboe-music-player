//
// Created by Dipak Sisodiya on 19/10/24.
//

#ifndef OBOE_MP3_PLAYER_AUDIOENGINE_H
#define OBOE_MP3_PLAYER_AUDIOENGINE_H

#include <cassert>
#include <fstream>
#include <cstring>
#include <android/log.h>
#include <oboe/Oboe.h>
#include <vector>
#include <memory>
#include <jni.h>
#include "FullDuplexPass.h"

// Let's discuss how this will be done, one class for recording and playback
// So should the methods be same for both as well? possibly, thats the least latency path
// Even if there is one method, to make a call, it would be two methods call, one after the other
// startRecording and startPlayback
// Create two global variable to track time of the first frame hit for both.
// Then find the difference between the two
// Preprocessing - Create recordingEngine, initiate stream, load wav assets, all of the preprocessing in one
// Trigger - trigger startRecordingAndPlaying
// Postprocessing


namespace little_endian_io {
    template <typename Word>
    std::ostream& write_word(std::ostream& outs, Word value, unsigned size = sizeof(Word));
}

extern JavaVM* g_javaVM;
extern jobject g_callbackObject;

class AudioEngine {
    public:
        AudioEngine();
        ~AudioEngine();

        // Public methods
        void startRecording(const char* filePath);
        void startPlayback(const char* wavFilePath);
        void stopRecording();
        void stopPlayback();
        void startRecordingAndPlaying(const char* filePath, const char* wavFilePath);
        void stopRecordingAndPlaying();
        void setRecordingDeviceId(int32_t deviceId);
        void setPlaybackDeviceId(int32_t deviceId);

    private:
        JavaVM* g_javaVM;
        jobject g_callbackObject;
        std::shared_ptr<oboe::AudioStream> mRecordingStream;
        std::shared_ptr<oboe::AudioStream> mPlaybackStream;
        bool isRecording;
        bool isPlaying;
        int32_t mSampleRate;
        int32_t mRecordingDeviceId;
        int32_t mPlaybackDeviceId;
        oboe::AudioApi mAudioApi;
        oboe::AudioFormat mFormat;
        std::unique_ptr<FullDuplexPass> mDuplexStream;

        // Private methods
        oboe::Result openRecordingStream();
        oboe::Result openPlaybackStream();
        void closeRecordingStream();
        void closePlaybackStream();
        oboe::DataCallbackResult onAudioReady(oboe::AudioStream* oboeStream, void* audioData, int32_t numFrames);
        void warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream>& stream);
        void callJavaMethod(const char* methodName, const char* methodSignature, const char* str);
};


#endif //OBOE_MP3_PLAYER_AUDIOENGINE_H
