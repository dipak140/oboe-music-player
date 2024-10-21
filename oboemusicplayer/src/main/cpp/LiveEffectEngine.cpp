#include <cassert>
#include <fstream>
#include <cstring>
#include <android/log.h>
#include "LiveEffectEngine.h"
#include <chrono>

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

LiveEffectEngine::LiveEffectEngine() {
    assert(mOutputChannelCount == mInputChannelCount);
}

void LiveEffectEngine::setRecordingDeviceId(int32_t deviceId) {
    mRecordingDeviceId = deviceId;
}

void LiveEffectEngine::setPlaybackDeviceId(int32_t deviceId) {
    mPlaybackDeviceId = deviceId;
}

bool LiveEffectEngine::isAAudioRecommended() {
    return oboe::AudioStreamBuilder::isAAudioRecommended();
}

bool LiveEffectEngine::setAudioApi(oboe::AudioApi api) {
    if (mIsEffectOn) return false;
    mAudioApi = api;
    return true;
}

bool LiveEffectEngine::setEffectOn(bool isOn) {
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

long long currentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void LiveEffectEngine::startRecording(const char * filePath, oboe::InputPreset inputPreset, long startRecordingTimestamp) {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Starting recording natively at %s", filePath);
    this->isRecording = true;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input);
    builder.setPerformanceMode(oboe::PerformanceMode::None);
    builder.setFormat(oboe::AudioFormat::I16);
    builder.setChannelCount(oboe::ChannelCount::Mono);
    builder.setInputPreset(oboe::InputPreset::Generic);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setSampleRate(mSampleRate);
    builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Best);
    builder.setAudioApi(oboe::AudioApi::AAudio);

    // Wave file generating stuff (from https://www.cplusplus.com/forum/beginner/166954/)
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
    write_word( f,  mSampleRate, 4 );  // samples per second (Hz)
    //write_word( f, 176400, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word( f,(mSampleRate * bitsPerSample * numChannels) / 8, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
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
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Recording started at %lld ms", currentTimeMillis());

    if (a == oboe::StreamState::Started) {
        // Read data from the stream
        __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stream Started %lld ms", currentTimeMillis());
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

            // Capture the precise time when the recording starts
            if (!isTimeRecorded){
                __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stream Started first %lld ms", currentTimeMillis());
                startRecordingDelay = currentTimeMillis()-startRecordingTimestamp;
                isTimeRecorded = true;
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

void LiveEffectEngine::startRecordingWithoutFile(const char * filePath, const char * musicPath,oboe::InputPreset inputPreset, long startRecordTimestamp) {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Starting recording natively at %s", filePath);
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Plaing music natively at %s", musicPath);
    this->isRecording = true;
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Input);
    builder.setPerformanceMode(oboe::PerformanceMode::None);
    builder.setFormat(oboe::AudioFormat::I16);
    builder.setChannelCount(oboe::ChannelCount::Mono);
    builder.setInputPreset(oboe::InputPreset::Generic);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setSampleRate(mSampleRate);
    builder.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Best);
    builder.setAudioApi(oboe::AudioApi::AAudio);

    // Wave file generating stuff (from https://www.cplusplus.com/forum/beginner/166954/)
    int bitsPerSample = 16; // multiple of 8
    int numChannels = 1; // 2 for stereo, 1 for mono

    std::ofstream f;
    const char *path = filePath;
    f.open(path, std::ios::binary);
    // Write the file headers
    f << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
    write_word( f,     16, 4 );  // no extension data
    write_word( f,      1, 2 );  // PCM - integer samples
    write_word( f,      numChannels, 2 );  // one channel (mono) or two channels (stereo file)
    write_word( f,  mSampleRate, 4 );  // samples per second (Hz)
    //write_word( f, 176400, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word( f,(mSampleRate * bitsPerSample * numChannels) / 8, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word( f,      4, 2 );  // data block size (size of two integer samples, one for each channel, in bytes)
    write_word( f,     bitsPerSample, 2 );  // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    size_t data_chunk_pos = f.tellp();
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

    // Open and play the WAV file alongside the recording
     //playWavFile(musicPath);

    auto a = stream->getState();
     //Capture the precise time when the recording starts

    if (a == oboe::StreamState::Started) {
        __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stream Started %lld ms", currentTimeMillis());
        constexpr int kMillisecondsToRecord = 20;
        // Use JNI to notify Java that recording has started
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

            if (!isTimeRecorded){
                __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Stream Started first %lld ms", currentTimeMillis());
                callJavaMethod("onRecordingStarted", "(Ljava/lang/String;)V","34");
                startRecordingDelay = currentTimeMillis()-startRecordTimestamp;
                isTimeRecorded = true;
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

        // (We'll need the final file size to fix the chunk sizes above)
        size_t file_length = f.tellp();

        // Fix the data chunk header to contain the data size
        f.seekp( data_chunk_pos + 4 );
        write_word( f, file_length - data_chunk_pos + 8 );

        // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
        f.seekp( 0 + 4 );
        write_word( f, file_length - 8, 4 );
        f.close();
    }
}

void LiveEffectEngine::playWavFile(const char *wavFilePath) {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioPlayer", "Playing WAV file from %s", wavFilePath);

    std::ifstream wavFile(wavFilePath, std::ios::binary);
    if (!wavFile.is_open()) {
        __android_log_print(ANDROID_LOG_ERROR, "OboeAudioPlayer", "Failed to open WAV file");
        return;
    }

    // Skip the WAV file header
    wavFile.seekg(44, std::ios::beg);
//
//    // Read the PCM audio data from the file into a buffer
    std::vector<int16_t> pcmData;
    int16_t sample;
    while (wavFile.read(reinterpret_cast<char *>(&sample), sizeof(sample))) {
        pcmData.push_back(sample);
    }
//
    wavFile.close();
//
//    // Initialize and start the playback stream
//    oboe::AudioStreamBuilder playbackBuilder;
//    playbackBuilder.setDirection(oboe::Direction::Output)
//            ->setSampleRate(mSampleRate)
//            ->setChannelCount(oboe::ChannelCount::Stereo)
//            ->setFormat(oboe::AudioFormat::I16)
//            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
//            ->setCallback(this);
//
//    oboe::Result result = playbackBuilder.openStream(mPlayStream);
//    if (result == oboe::Result::OK) {
//        mPlayStream->start();
//        __android_log_print(ANDROID_LOG_INFO, "OboeAudioPlayer", "Playback started.");
//    } else {
//        __android_log_print(ANDROID_LOG_ERROR, "OboeAudioPlayer", "Failed to open playback stream.");
//    }
}


int LiveEffectEngine::getStartRecordingDelay(){
    return startRecordingDelay;
}

void LiveEffectEngine::callJavaMethod(const char* methodName, const char* methodSignature, const char* str) {
    if (g_javaVM == nullptr || g_callbackObject == nullptr) {
        // Cannot proceed without JavaVM or callback object
        return;
    }

    JNIEnv* env = nullptr;
    bool didAttachThread = false;

    // Get the current thread's JNIEnv
    jint getEnvResult = g_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvResult == JNI_EDETACHED) {
        // Thread not attached, attach it
        if (g_javaVM->AttachCurrentThread(&env, nullptr) != 0) {
            // Failed to attach, cannot proceed
            return;
        }
        didAttachThread = true;
    } else if (getEnvResult != JNI_OK) {
        // Failed to get JNIEnv
        return;
    }

    // Get the class of the callback object
    jclass callbackClass = env->GetObjectClass(g_callbackObject);
    // Get the method ID
    jmethodID methodID = env->GetMethodID(callbackClass, methodName, methodSignature);
    if (methodID == nullptr) {
        env->DeleteLocalRef(callbackClass);
    }

    // Convert the C string to a Java string
    jstring jstrParam = env->NewStringUTF(str);

    // Call the Java method with the converted string
    env->CallVoidMethod(g_callbackObject, methodID, jstrParam);

    // Clean up local references
    env->DeleteLocalRef(jstrParam);
    env->DeleteLocalRef(callbackClass);

    // Check for exceptions
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

}


void LiveEffectEngine::pauseRecording() {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Pausing recording");
    stream->requestPause();
    this->isPaused = true;
}

void LiveEffectEngine::resumeRecording() {
    __android_log_print(ANDROID_LOG_INFO, "OboeAudioRecorder", "Resuming recording");
    this->isPaused = false;
}

void LiveEffectEngine::stopRecording() {
    this->isRecording = false;
    stream->requestStop();
    stream->close();
    isTimeRecorded = false;
}


oboe::Result LiveEffectEngine::openPlaybackStream() {
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

oboe::AudioStreamBuilder *LiveEffectEngine::setupRecordingStreamParameters(
        oboe::AudioStreamBuilder *builder, int32_t sampleRate) {
    builder->setDeviceId(mRecordingDeviceId)
            ->setDirection(oboe::Direction::Input)
            ->setSampleRate(sampleRate)
            ->setChannelCount(mInputChannelCount);
    return setupCommonStreamParameters(builder);
}

oboe::AudioStreamBuilder *LiveEffectEngine::setupPlaybackStreamParameters(
        oboe::AudioStreamBuilder *builder) {
    builder->setDataCallback(this)
            ->setErrorCallback(this)
            ->setDeviceId(mPlaybackDeviceId)
            ->setDirection(oboe::Direction::Output)
            ->setChannelCount(mOutputChannelCount);
    return setupCommonStreamParameters(builder);
}

oboe::AudioStreamBuilder *LiveEffectEngine::setupCommonStreamParameters(
        oboe::AudioStreamBuilder *builder) {
    builder->setAudioApi(mAudioApi)
            ->setFormat(mFormat)
            ->setFormatConversionAllowed(true)
            ->setPerformanceMode(oboe::PerformanceMode::None);
    return builder;
}

void LiveEffectEngine::closeStreams() {
    if (mDuplexStream) {
        mDuplexStream->stop();
    }
    closeStream(mPlayStream);
    closeStream(mRecordingStream);
    mDuplexStream.reset();
}

void LiveEffectEngine::closeStream(std::shared_ptr<oboe::AudioStream> &stream) {
    if (stream) {
        if (stream->getState() != oboe::StreamState::Closed) {
            oboe::Result result = stream->stop();
            if (result != oboe::Result::OK && result != oboe::Result::ErrorClosed) {
                __android_log_print(ANDROID_LOG_ERROR, "LiveEffectEngine", "Error stopping stream: %s", oboe::convertToText(result));
            }
            result = stream->close();
            if (result != oboe::Result::OK && result != oboe::Result::ErrorClosed) {
                __android_log_print(ANDROID_LOG_ERROR, "LiveEffectEngine", "Error closing stream: %s", oboe::convertToText(result));
            }
        }
        stream.reset();
    }
}

oboe::DataCallbackResult LiveEffectEngine::onAudioReady(
        oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames) {

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


void LiveEffectEngine::warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream> &stream) {
    if (stream->getPerformanceMode() != oboe::PerformanceMode::LowLatency) {
        __android_log_print(ANDROID_LOG_WARN, "LiveEffectEngine", "Stream is NOT low latency.");
    }
}

void LiveEffectEngine::setVolume(float volume) {
    mVolume = volume;
}
