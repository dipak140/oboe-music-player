/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef OBOE_LIVEEFFECTENGINE_H
#define OBOE_LIVEEFFECTENGINE_H

#include <jni.h>
#include "oboe/Oboe.h"
#include <memory>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include "FullDuplexPass.h"

class EarbackEngine : public oboe::AudioStreamCallback {
public:
    EarbackEngine();

    void setRecordingDeviceId(int32_t deviceId);
    void setPlaybackDeviceId(int32_t deviceId);
    oboe::AudioStream *stream{};
    int64_t framePosition = 0;
    /**
     * @param isOn
     * @return true if it succeeds
     */
    bool setEffectOn(bool isOn);
    /*
     * oboe::AudioStreamDataCallback interface implementation
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
                                          void *audioData, int32_t numFrames) override;

    bool setAudioApi(oboe::AudioApi);
    bool isAAudioRecommended(void);
    void setVolume(float volume);  // Add this line


private:
    bool              mIsEffectOn = false;
    int32_t           mRecordingDeviceId = oboe::kUnspecified;
    int32_t           mPlaybackDeviceId = oboe::kUnspecified;
    const oboe::AudioFormat mFormat = oboe::AudioFormat::Float; // for easier processing
    oboe::AudioApi    mAudioApi = oboe::AudioApi::AAudio;
    int32_t           mSampleRate = 44100;
    const int32_t     mInputChannelCount = oboe::ChannelCount::Mono;
    const int32_t     mOutputChannelCount = oboe::ChannelCount::Mono;
    size_t data_chunk_pos = 0;
    std::vector<int16_t> pcmData;  // Buffer to store PCM data
    size_t playbackIndex = 0;

    std::unique_ptr<FullDuplexPass> mDuplexStream;
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    std::shared_ptr<oboe::AudioStream> mPlayStream;
    oboe::Result openPlaybackStream();
    void closeStreams();
    void closeStream(std::shared_ptr<oboe::AudioStream> &stream);
    oboe::AudioStreamBuilder *setupCommonStreamParameters(
        oboe::AudioStreamBuilder *builder);
    oboe::AudioStreamBuilder *setupRecordingStreamParameters(
        oboe::AudioStreamBuilder *builder, int32_t sampleRate);
    oboe::AudioStreamBuilder *setupPlaybackStreamParameters(
        oboe::AudioStreamBuilder *builder);
    void warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream> &audioStream);

    // WAV file memberss
    std::ofstream mWavFile;
    std::string mWavFilePath;
    bool isStreamOpen = false;
};

#endif  // OBOE_LIVEEFFECTENGINE_H
