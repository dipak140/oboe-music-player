//
// Created by Dipak Sisodiya on 16/12/24.
// dipak@reconv.in
//

#ifndef OBOE_MP3_PLAYER_RECORDINGENGINE_H
#define OBOE_MP3_PLAYER_RECORDINGENGINE_H

#endif //OBOE_MP3_PLAYER_RECORDINGENGINE_H
#include <jni.h>
#include <memory>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include "../../../../oboe/include/oboe/AudioStreamCallback.h"
#include "../../../../oboe/include/oboe/Oboe.h"
#include "../../../../oboe/include/oboe/Definitions.h"

class RecordingEngine{
public:
    RecordingEngine();
    oboe::AudioStream *stream{};
    bool isRecording = false;
    bool firstFrameHit = false;
    bool isPaused = false;
    void startRecording(const char * filePath, oboe::InputPreset inputPreset, long startRecordingTimestamp);
    void stopRecording();
    void pauseRecording();
    void resumeRecording();
    bool setAudioApi(oboe::AudioApi);
    bool isAAudioRecommended(void);
    int64_t framePosition = 0;
    int64_t presentationTime = 0;
    jlong getFramePosition();
    jlong getFrameTimeStamp();
    jint getAudioSessionId();

private:


};