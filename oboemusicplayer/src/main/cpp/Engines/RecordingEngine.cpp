//
// Created by Dipak Sisodiya on 16/12/24.
// dipak@reconv.in
//

#include <android/log.h>
#include "RecordingEngine.h"
#include "../../../../oboe/include/oboe/Oboe.h"
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

using namespace little_endian_io;

long long currentTimeMillisRecording() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

RecordingEngine::RecordingEngine() {
}

void RecordingEngine::startRecording(const char * filePath, oboe::InputPreset inputPreset, long startRecordingTimestamp) {
    this->isRecording = true;
    this->firstFrameHit = false;

    // create an audio builder;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input);
    builder.setPerformanceMode(oboe::PerformanceMode::None);
    builder.setFormat(oboe::AudioFormat::I16);
    builder.setChannelCount(oboe::ChannelCount::Mono);
    builder.setInputPreset(oboe::InputPreset::Generic);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    // TODO: Change sample rate
    builder.setSampleRate(44100);
    builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Best);
    builder.setAudioApi(oboe::AudioApi::AAudio);

    int bitsPerSample = 16; // multiple of 8
    int numChannels = 1; // 2 for stereo, 1 for mono

    std::ofstream f;
    //const char *path = "/storage/emulated/0/Music/record.wav";
    const char *path = filePath;
    f.open(path, std::ios::binary);
    // Write the file headers
    f << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
    write_word( f,     16, 4 );  // no extension data
    write_word( f,      1, 2 );  // PCM - integer samples
    write_word( f,      numChannels, 2 );  // one channel (mono) or two channels (stereo file)
    write_word( f,  44100, 4 );  // samples per second (Hz)
    //write_word( f, 176400, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word( f,(44100 * bitsPerSample * numChannels) / 8, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word( f,      4, 2 );  // data block size (size of two integer samples, one for each channel, in bytes)
    write_word( f,     bitsPerSample, 2 );  // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    f << "data----";  // (chunk size to be filled in later)
    // f.flush();

    oboe::Result r = builder.openStream(&stream);
    if (r != oboe::Result::OK) {
        return;
    }

    r = stream->requestStart();
    if (r != oboe::Result::OK) {
        return;
    }

    auto a = stream->getState();
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Recording started at %lld ms",
                        currentTimeMillisRecording());

    if (a == oboe::StreamState::Started) {
        // Read data from the stream
        __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stream Started %lld ms",
                            currentTimeMillisRecording());
        constexpr int kMillisecondsToRecord = 20;
        auto requestedFrames = (int32_t) (kMillisecondsToRecord * (stream->getSampleRate() / oboe::kMillisPerSecond));
        __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "requestedFrames = %d", requestedFrames);

        int16_t mybuffer[requestedFrames];
        constexpr int64_t kTimeoutValue = 3 * oboe::kNanosPerMillisecond;

        int framesRead = 0;
        do {
            auto result = stream->read(mybuffer, requestedFrames, 0);
            if (result != oboe::Result::OK) {
                break;
            }
            framesRead = result.value();
            __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "framesRead = %d", framesRead);
            if (framesRead > 0) {
                break;
            }
        } while (framesRead != 0);

        while (isRecording) {
            auto result = stream->read(mybuffer, requestedFrames, kTimeoutValue * 1000);
            if (!firstFrameHit && framePosition == 0 && presentationTime == 0) {
                // Get the current timestamp
                oboe::Result results = stream->getTimestamp(CLOCK_BOOTTIME, &framePosition, &presentationTime);
                if (results == oboe::Result::OK) {
                    presentationTime = currentTimeMillisRecording();
                    __android_log_print(ANDROID_LOG_INFO, "OboeAudio", "First frame timestamp: %" PRId64 " ns", presentationTime);
                    __android_log_print(ANDROID_LOG_INFO, "OboeAudio", "Frame position: %" PRId64 ", Presentation time: %" PRId64 " ns", framePosition, presentationTime);
                } else {
                    __android_log_print(ANDROID_LOG_ERROR, "OboeAudio", "Failed to get timestamp: %s", oboe::convertToText(results));
                }
                this->firstFrameHit = true;
            }

            if(isPaused) {
                __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Recording is paused...");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Sleep briefly to prevent busy-waiting
                continue;  // Skip to the next loop iteration
            }

            if (result == oboe::Result::OK) {
                auto nbFramesRead = result.value();
                for (int i = 0; i < nbFramesRead; i++) {
                    write_word( f, (int)(mybuffer[i]), 2 );
                }
            } else {
                auto error = convertToText(result.error());
                __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "error = %s", error);
            }
        }
        __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Requesting stop");
    }
}


void RecordingEngine::stopRecording() {
    this->isRecording = false;
    stream->requestStop();
    stream->close();
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stopped recording");
}

void RecordingEngine::pauseRecording() {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Pausing recording");
    stream->requestPause();
    this->isPaused = true;
}

void RecordingEngine::resumeRecording() {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Resuming recording");
    this->isPaused = false;
}

jlong RecordingEngine::getFramePosition() {
    return framePosition;
}

jlong RecordingEngine::getFrameTimeStamp() {
    return presentationTime;
}

jint RecordingEngine::getAudioSessionId(){
    return stream->getSessionId();
}
