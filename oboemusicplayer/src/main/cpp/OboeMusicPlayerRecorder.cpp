#include <jni.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/log.h>
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>
#include <player/OneShotSampleSource.h>
#include "Engines/RecordingEngine.h"
#include "Engines/EarbackEngine.h"
#include "Engines/SimpleAudioPlayer.h"

static const char* TAG = "MusicPlayerRecorderJNI";
using namespace iolib;
using namespace parselib;
static EarbackEngine * earbackEngine = nullptr;
static SimpleAudioPlayer* sDTPlayer = nullptr; // Dynamically allocated
static RecordingEngine* recordingEngine = nullptr;
static const int kOboeApiAAudio = 0;
static const int kOboeApiOpenSLES = 1;
JavaVM* g_JavaVM = nullptr;
jobject gJavaCallbackObj = nullptr; // global ref to Java callback object
jmethodID gOnAudioDataAvailableMethod = nullptr;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_JavaVM = vm; // Store the JavaVM pointer
    return JNI_VERSION_1_6; // Return the JNI version you're using
}

#ifdef __cplusplus
extern "C" {
#endif

using namespace iolib;
using namespace parselib;

JNIEXPORT jstring JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_onRecordingStarted(
        JNIEnv* env,
        jobject, jstring eventInfo){
    std::string hello = "Hello from C++ 2";
    return env->NewStringUTF(hello.c_str());
}


/**
 * Native (JNI) implementation of MusicPlayer.setupAudioStreamNative()
*/

JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_createPlayer(JNIEnv* env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }

    return (sDTPlayer != nullptr) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setupAudioStreamNative(
        JNIEnv* env, jobject, jint numChannels) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "init()");
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->setupAudioStream(numChannels);
}

JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_startAudioStreamNative(
        JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->startStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.teardownAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_teardownAudioStreamNative(JNIEnv* , jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "deinit()");

    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    // we know in this case that the sample buffers are all 1-channel, 44.1K
    sDTPlayer->teardownAudioStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.loadWavAssetNative()
 */

// the trick is to load the wav files before being asked to play, this way we can make sure that the
// lease possible latency is being introduced.
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_loadWavAssetNative(
        JNIEnv* env, jobject, jbyteArray bytearray, jint index, jfloat pan) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    int len = env->GetArrayLength (bytearray);

    unsigned char* buf = new unsigned char[len];
    env->GetByteArrayRegion (bytearray, 0, len, reinterpret_cast<jbyte*>(buf));

    MemInputStream stream(buf, len);

    WavStreamReader reader(&stream);
    reader.parse();

    reader.getNumChannels();

    SampleBuffer* sampleBuffer = new SampleBuffer();
    sampleBuffer->loadSampleData(&reader);

    OneShotSampleSource* source = new OneShotSampleSource(sampleBuffer, pan);
    sDTPlayer->addSampleSource(source, sampleBuffer);

    delete[] buf;
}

/**
 * Native (JNI) implementation of MusicPlayer.unloadWavAssetsNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_unloadWavAssetsNative(JNIEnv* env, jobject) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->unloadSampleData();
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_seekToPosition(JNIEnv* env, jobject, jlong position, jint mSampleRate, jint mNumChannels) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    int64_t seekPositionMillis = position;
    sDTPlayer->seekTo(seekPositionMillis, mSampleRate, mNumChannels);
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_trigger(JNIEnv* env, jobject, jint index) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->triggerDown(index);
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_stopTrigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer->triggerUp(index);
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_pauseTrigger(JNIEnv* env, jobject) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->pauseStream();
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_resumeTrigger(JNIEnv* env, jobject ) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->resumeStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getOutputReset(JNIEnv*, jobject) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_clearOutputReset(JNIEnv*, jobject) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->clearOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.restartStream()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_restartStream(JNIEnv*, jobject) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->resetAll();
    if (sDTPlayer->openStream() && sDTPlayer->startStream()){
        __android_log_print(ANDROID_LOG_INFO, TAG, "openStream successful");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "openStream failed");
    }
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setPan(
        JNIEnv *env, jobject thiz, jint index, jfloat pan) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    sDTPlayer->setPan(index, pan);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getPan(
        JNIEnv *env, jobject thiz, jint  index) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getPan(index);
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setGain(
        JNIEnv *env, jobject thiz, jint  index, jfloat gain) {
    if (sDTPlayer == nullptr) {
        return;
    }
    sDTPlayer->setGain(index, gain);
}

JNIEXPORT jlong JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getPosition(
        JNIEnv *env, jobject thiz, jint  index, jint mSampleRate, jint mNumChannels) {
    if (sDTPlayer == nullptr) {
        return 0;
    }
    return sDTPlayer->getPosition(index, mSampleRate, mNumChannels);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getGain(
        JNIEnv *env, jobject thiz, jint index) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getGain(index);
}


#ifdef __cplusplus
}
#endif

extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_create(JNIEnv *env, jobject thiz) {
    if (earbackEngine == nullptr) {
        earbackEngine = new EarbackEngine();
    }
    if (recordingEngine == nullptr){
        recordingEngine = new RecordingEngine();
    }

    return (earbackEngine != nullptr) ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_isAAudioRecommended(JNIEnv *env, jobject thiz) {
    // TODO: implement isAAudioRecommended()
    if (earbackEngine == nullptr) {
        return JNI_FALSE;
    }
    return earbackEngine -> isAAudioRecommended() ? JNI_TRUE : JNI_FALSE;
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setAPI(JNIEnv *env, jobject thiz,
                                                           jint api_type) {
    if (earbackEngine == nullptr) {
        return JNI_FALSE;
    }

    oboe::AudioApi audioApi;
    switch (api_type) {
        case kOboeApiAAudio:
            audioApi = oboe::AudioApi::AAudio;
            break;
        case kOboeApiOpenSLES:
            audioApi = oboe::AudioApi::OpenSLES;
            break;
        default:
            return JNI_FALSE;
    }

    return earbackEngine -> setAudioApi(audioApi) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setEffectOn(JNIEnv *env, jobject thiz,
                                                                jboolean is_effect_on) {
    if (earbackEngine == nullptr){
        return JNI_FALSE;
    }
    return earbackEngine -> setEffectOn(is_effect_on) ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setRecordingDeviceId(JNIEnv *env, jobject thiz,
                                                                         jint device_id) {
    if (earbackEngine == nullptr) {
        return;
    }
    earbackEngine -> setRecordingDeviceId(device_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setPlaybackDeviceId(JNIEnv *env, jobject thiz,
                                                                        jint device_id) {
    if (earbackEngine == nullptr) {
        return;
    }
    earbackEngine -> setPlaybackDeviceId(device_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_delete(JNIEnv *env, jobject thiz) {
    // TODO: implement delete()
    if(earbackEngine){
        earbackEngine ->setEffectOn(false);
        delete earbackEngine;
        earbackEngine = nullptr;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_native_1setDefaultStreamValues(JNIEnv *env,
                                                                                   jobject thiz,
                                                                                   jint default_sample_rate,
                                                                                   jint default_frames_per_burst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) default_sample_rate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) default_frames_per_burst;
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_startRecording(JNIEnv *env, jobject thiz,
                                                                   jstring full_path_tofile,
                                                                   jint input_preset_preference,
                                                                   jlong start_recording_time) {
    if (recordingEngine == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "oboemusicplayer", "engine == nullptr");
        return;
    }

    oboe::InputPreset inputPreset;
    switch (input_preset_preference) {
        case 10:
            inputPreset = oboe::InputPreset::VoicePerformance;
            break;
        case 9:
            inputPreset = oboe::InputPreset::Unprocessed;
            break;
        default:
            return;
    }

    const char *path = (*env).GetStringUTFChars(full_path_tofile, 0);
    recordingEngine -> startRecording(path, inputPreset, start_recording_time);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_stopRecording(JNIEnv *env, jobject thiz) {
    if (recordingEngine == nullptr) {
        return;
    }

    recordingEngine -> stopRecording();
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_resumeRecording(JNIEnv *env, jobject thiz) {
    if (recordingEngine == nullptr) {
        return;
    }
    recordingEngine ->resumeRecording();
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_pauseRecording(JNIEnv *env, jobject thiz) {
    if (recordingEngine == nullptr) {
        return;
    }
    recordingEngine ->pauseRecording();
}


extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_setCallbackObject(JNIEnv *env, jobject thiz,
                                                                      jobject callback_object) {
    if (gJavaCallbackObj != nullptr) {
        env->DeleteGlobalRef(gJavaCallbackObj);
    }
    // Create a new global reference to the callback object
    gJavaCallbackObj = env->NewGlobalRef(callback_object);

    jclass callbackClass = env->GetObjectClass(callback_object);
    gOnAudioDataAvailableMethod = env->GetMethodID(callbackClass, "onAudioDataAvailable", "([B)V");
    if (!gOnAudioDataAvailableMethod) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to find onAudioDataAvailable method");
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getDurationNative(JNIEnv *env, jobject thiz, jint mSampleRate, jint mNumChannels) {
    if (sDTPlayer == nullptr) {
        return 0;
    }
    return sDTPlayer->getDuration(mSampleRate, mNumChannels);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getMusicPlayerTimeStamp(JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getFrameTimeStamp();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getMusicPlayerFramePosition(JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getFramePosition();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getRecorderTimeStamp(JNIEnv *env, jobject thiz) {
    return recordingEngine->getFrameTimeStamp();
}

extern "C"
JNIEXPORT jlong JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getRecorderFramePosition(JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return recordingEngine->getFramePosition();
}

extern "C"
JNIEXPORT jint JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getRecorderAudioSessionId(JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return recordingEngine->getAudioSessionId();
}

extern "C"
JNIEXPORT jint JNICALL
Java_in_reconv_oboemusicplayer_NativeMusicPlayer_getPlayerAudioSessionId(JNIEnv *env, jobject thiz) {
    if (sDTPlayer == nullptr) {
        sDTPlayer = new SimpleAudioPlayer();
    }
    return sDTPlayer->getAudioSessionId();
}