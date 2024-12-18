//
// Created by Dipak Sisodiya on 18/12/24.
//

#ifndef OBOE_MP3_PLAYER_VOCALMUSICPLAYER_H
#define OBOE_MP3_PLAYER_VOCALMUSICPLAYER_H


#include <vector>
#include <oboe/Oboe.h>
#include "OneShotSampleSource.h"
#include "SampleBuffer.h"
#include "AudioRingBuffer.h"
#include <atomic>
#include <thread>

namespace iolib{

class VocalMusicPlayer {
public:
    VocalMusicPlayer();
    void setupAudioStream(int32_t channelCount);
    void teardownAudioStream();

    bool openStream();
    bool startStream();
    bool firstFrameHit = false;

    int getSampleRate() {return mSampleRate;}
    void addSampleSource(SampleSource* source, SampleBuffer* buffer);
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
    int64_t getDuration(int mSampleRate, int mNumChannels);
    int64_t getFramePosition();
    int64_t getFrameTimeStamp();
    int64_t framePosition = 0;
    int64_t presentationTime = 0;
    int getAudioSessionId();

    // New changes
    bool openOutputStream();
    bool openInputStream();
    void stopStreams();

private:
    class MyDataCallback : public oboe::AudioStreamDataCallback{
    public:
        MyDataCallback(VocalMusicPlayer *parent): mParent(parent){}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;
    private:
        VocalMusicPlayer *mParent;
    };
    class MyErrorCallback : public oboe::AudioStreamErrorCallback {
    public:
        MyErrorCallback(VocalMusicPlayer *parent) : mParent(parent) {}

        virtual ~MyErrorCallback() {
        }

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

    private:
        VocalMusicPlayer *mParent;
    };

    std::shared_ptr<oboe::AudioStream> mOutputStream;
    std::shared_ptr<oboe::AudioStream> mInputStream;

    // Playback Audio attributes
    int32_t mChannelCount;
    int32_t mSampleRate;

    // Sample Data
    int32_t mNumSampleBuffers;
    std::vector<SampleBuffer*>  mSampleBuffers;
    std::vector<SampleSource*>  mSampleSources;

    bool    mOutputReset;

    std::shared_ptr<MyDataCallback> mDataCallback;
    std::shared_ptr<MyErrorCallback> mErrorCallback;
    bool mIsStreamPaused;
    AudioRingBuffer mMicRingBuffer;
    std::atomic_bool mRunning;
    std::thread mInputThread;
    void readInputLoop();
};
};

#endif //OBOE_MP3_PLAYER_VOCALMUSICPLAYER_H
