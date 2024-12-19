//
// Created by Dipak Sisodiya on 19/12/24.
//

#ifndef OBOE_MP3_PLAYER_SIMPLEAUDIOPLAYER_H
#define OBOE_MP3_PLAYER_SIMPLEAUDIOPLAYER_H

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

#include <vector>

#include <oboe/Oboe.h>

#include <player/OneShotSampleSource.h>
#include <player/SampleBuffer.h>
#include "AudioRingBuffer.h"
extern JavaVM* g_JavaVM;
extern jobject gJavaCallbackObj;
extern jmethodID gOnAudioDataAvailableMethod;

namespace iolib {
/**
 * A simple streaming player for multiple SampleBuffers.
 */
    class SimpleAudioPlayer  {
    public:
        SimpleAudioPlayer();

        void setupAudioStream(int32_t channelCount);
        void teardownAudioStream();

        bool openStream();
        bool startStream();
        bool  firstFrameHit = false;

        int getSampleRate() { return mSampleRate; }

        // Wave Sample Loading...
        /**
         * Adds the SampleSource/SampleBuffer pair to the list of source channels.
         * Transfers ownership of those objects so that they can be deleted/unloaded.
         * The indexes associated with each source channel is the order in which they
         * are added.
         */
        void addSampleSource(SampleSource* source, SampleBuffer* buffer);
        /**
         * Deallocates and deletes all added source/buffer (see addSampleSource()).
         */
        void unloadSampleData();

        void triggerDown(int32_t index);
        void triggerUp(int32_t index);

        void resetAll();

        bool getOutputReset() { return mOutputReset; }
        void clearOutputReset() { mOutputReset = false; }

        void setPan(int index, float pan);
        float getPan(int index);

        void setGain(int index, float gain);
        float getGain(int index);
        void pauseStream();
        void resumeStream();
        void seekTo(int64_t positionMillis, int mSampleRate, int mNumChannels);
        int64_t getPosition(int index, int mSampleRate, int mNumChannels);
        int64_t getDuration(int i, int channelCount);
        int64_t getFramePosition();
        int64_t getFrameTimeStamp();
        int64_t framePosition = 0;
        int64_t presentationTime = 0;
        int getAudioSessionId();

    private:
        class MyDataCallback : public oboe::AudioStreamDataCallback {
        public:
            MyDataCallback(SimpleAudioPlayer *parent) : mParent(parent) {}

            oboe::DataCallbackResult onAudioReady(
                    oboe::AudioStream *audioStream,
                    void *audioData,
                    int32_t numFrames) override;

        private:
            SimpleAudioPlayer *mParent;
        };

        class MyErrorCallback : public oboe::AudioStreamErrorCallback {
        public:
            MyErrorCallback(SimpleAudioPlayer *parent) : mParent(parent) {}

            virtual ~MyErrorCallback() {
            }

            void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

        private:
            SimpleAudioPlayer *mParent;
        };

        // Oboe Audio Stream
        std::shared_ptr<oboe::AudioStream> mAudioStream;

        // Playback Audio attributes
        int32_t mChannelCount;
        int32_t mSampleRate;
        AudioRingBuffer mRingBuffer;

        // Sample Data
        int32_t mNumSampleBuffers;
        std::vector<SampleBuffer*>  mSampleBuffers;
        std::vector<SampleSource*>  mSampleSources;

        bool    mOutputReset;

        std::shared_ptr<MyDataCallback> mDataCallback;
        std::shared_ptr<MyErrorCallback> mErrorCallback;
        bool mIsStreamPaused;

        void callJavaMethod(const char *methodName, const char *methodSignature, const char *str);
    };

}
#endif //_PLAYER_SimpleAudioPlayer_H_

