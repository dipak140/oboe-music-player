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
#include <player/SimpleMultiPlayer.h>
#include "LiveEffectEngine.h"

static const char* TAG = "MusicPlayerRecorderJNI";
JavaVM* g_javaVM = nullptr; // Define JavaVM pointer
jobject g_callbackObject = nullptr; // Define g_callbackObject
using namespace iolib;
using namespace parselib;
static LiveEffectEngine * engine = nullptr;
static const int kOboeApiAAudio = 0;
static const int kOboeApiOpenSLES = 1;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_javaVM = vm; // Store the JavaVM pointer
    return JNI_VERSION_1_6; // Return the JNI version you're using
}

#ifdef __cplusplus
extern "C" {
#endif

using namespace iolib;
using namespace parselib;
static SimpleMultiPlayer sDTPlayer;

JNIEXPORT jstring JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


JNIEXPORT jstring JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_onRecordingStarted(
        JNIEnv* env,
        jobject, jstring eventInfo){
    std::string hello = "Hello from C++ 2";
    return env->NewStringUTF(hello.c_str());
}


/**
 * Native (JNI) implementation of MusicPlayer.setupAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_setupAudioStreamNative(
        JNIEnv* env, jobject, jint numChannels) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "init()");
    sDTPlayer.setupAudioStream(numChannels);
}

JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_startAudioStreamNative(
        JNIEnv *env, jobject thiz) {
    sDTPlayer.startStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.teardownAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_teardownAudioStreamNative(JNIEnv* , jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "deinit()");

    // we know in this case that the sample buffers are all 1-channel, 44.1K
    sDTPlayer.teardownAudioStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.loadWavAssetNative()
 */

// the trick is to load the wav files before being asked to play, this way we can make sure that the
// lease possible latency is being introduced.
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_loadWavAssetNative(
        JNIEnv* env, jobject, jbyteArray bytearray, jint index, jfloat pan) {
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
    sDTPlayer.addSampleSource(source, sampleBuffer);

    delete[] buf;
}

/**
 * Native (JNI) implementation of MusicPlayer.unloadWavAssetsNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_unloadWavAssetsNative(JNIEnv* env, jobject) {
    sDTPlayer.unloadSampleData();
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_trigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer.triggerDown(index);
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_stopTrigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer.triggerUp(index);
}

/**
 * Native (JNI) implementation of MusicPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL Java_in_reconv_oboemusicplayer_NativeLib_getOutputReset(JNIEnv*, jobject) {
    return sDTPlayer.getOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_clearOutputReset(JNIEnv*, jobject) {
    sDTPlayer.clearOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.restartStream()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_restartStream(JNIEnv*, jobject) {
    sDTPlayer.resetAll();
    if (sDTPlayer.openStream() && sDTPlayer.startStream()){
        __android_log_print(ANDROID_LOG_INFO, TAG, "openStream successful");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "openStream failed");
    }
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_setPan(
        JNIEnv *env, jobject thiz, jint index, jfloat pan) {
    sDTPlayer.setPan(index, pan);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayer_NativeLib_getPan(
        JNIEnv *env, jobject thiz, jint  index) {
    return sDTPlayer.getPan(index);
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayer_NativeLib_setGain(
        JNIEnv *env, jobject thiz, jint  index, jfloat gain) {
    sDTPlayer.setGain(index, gain);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayer_NativeLib_getGain(
        JNIEnv *env, jobject thiz, jint index) {
    return sDTPlayer.getGain(index);
}


#ifdef __cplusplus
}
#endif

extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_create(JNIEnv *env, jobject thiz) {
    if (engine == nullptr) {
        engine = new LiveEffectEngine();
    }

    return (engine != nullptr) ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_isAAudioRecommended(JNIEnv *env, jobject thiz) {
    // TODO: implement isAAudioRecommended()
    if (engine == nullptr) {
        return JNI_FALSE;
    }
    return engine -> isAAudioRecommended() ? JNI_TRUE : JNI_FALSE;
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setAPI(JNIEnv *env, jobject thiz,
                                                           jint api_type) {
    if (engine == nullptr) {
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

    return engine -> setAudioApi(audioApi) ? JNI_TRUE : JNI_FALSE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setEffectOn(JNIEnv *env, jobject thiz,
                                                                jboolean is_effect_on) {
    if (engine == nullptr){
        return JNI_FALSE;
    }
    return engine -> setEffectOn(is_effect_on) ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setRecordingDeviceId(JNIEnv *env, jobject thiz,
                                                                         jint device_id) {
    if (engine == nullptr) {
        return;
    }
    engine -> setRecordingDeviceId(device_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setPlaybackDeviceId(JNIEnv *env, jobject thiz,
                                                                        jint device_id) {
    if (engine == nullptr) {
        return;
    }
    engine -> setPlaybackDeviceId(device_id);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_delete(JNIEnv *env, jobject thiz) {
    // TODO: implement delete()
    if(engine){
        engine ->setEffectOn(false);
        delete engine;
        engine = nullptr;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_native_1setDefaultStreamValues(JNIEnv *env,
                                                                                   jobject thiz,
                                                                                   jint default_sample_rate,
                                                                                   jint default_frames_per_burst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) default_sample_rate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) default_frames_per_burst;
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setVolume(JNIEnv *env, jobject thiz,
                                                              jfloat volume) {
    engine ->setVolume(volume);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_startRecording(JNIEnv *env, jobject thiz,
                                                                   jstring full_path_tofile,
                                                                   jint input_preset_preference,
                                                                   jlong start_recording_time) {
    if (engine == nullptr) {
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
    engine -> startRecording(path, inputPreset, start_recording_time);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_startRecordingWithoutFile(JNIEnv *env,
                                                                              jobject thiz,
                                                                              jstring full_path_tofile,
                                                                              jstring music_file_path,
                                                                              jint input_preset_preference,
                                                                              jlong start_recording_time) {
    // TODO: implement startRecordingWithoutFile()
    if (engine == nullptr) {
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
    const char *musicPath = (*env).GetStringUTFChars(music_file_path, 0);
    engine -> startRecordingWithoutFile(path, musicPath, inputPreset, start_recording_time);
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_stopRecording(JNIEnv *env, jobject thiz) {
    if (engine == nullptr) {
        return;
    }

    engine -> stopRecording();
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_resumeRecording(JNIEnv *env, jobject thiz) {
    if (engine == nullptr) {
        return;
    }
    engine ->resumeRecording();
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_pauseRecording(JNIEnv *env, jobject thiz) {
    if (engine == nullptr) {
        return;
    }
    engine ->pauseRecording();
}

extern "C"
JNIEXPORT jint JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_getRecordingDelay(JNIEnv *env, jobject thiz) {
    if (engine == nullptr) {
        return 0;
    }
    return engine ->getStartRecordingDelay();
}

extern "C"
JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayer_NativeLib_setCallbackObject(JNIEnv *env, jobject thiz,
                                                                      jobject callback_object) {
    if (g_callbackObject != nullptr) {
        env->DeleteGlobalRef(g_callbackObject);
    }
    // Create a new global reference to the callback object
    g_callbackObject = env->NewGlobalRef(callback_object);
}