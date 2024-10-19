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
Java_in_reconv_oboemusicplayerandrecorder_NativeLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


JNIEXPORT jstring JNICALL
Java_in_reconv_oboemusicplayerandrecorder_NativeLib_onRecordingStarted(
        JNIEnv* env,
        jobject, jstring eventInfo){
    std::string hello = "Hello from C++ 2";
    return env->NewStringUTF(hello.c_str());
}


/**
 * Native (JNI) implementation of MusicPlayer.setupAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_setupAudioStreamNative(
        JNIEnv* env, jobject, jint numChannels) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "init()");
    sDTPlayer.setupAudioStream(numChannels);
}

JNIEXPORT void JNICALL
Java_in_reconv_oboemusicplayerandrecorder_NativeLib_startAudioStreamNative(
        JNIEnv *env, jobject thiz) {
    sDTPlayer.startStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.teardownAudioStreamNative()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_teardownAudioStreamNative(JNIEnv* , jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "deinit()");

    // we know in this case that the sample buffers are all 1-channel, 44.1K
    sDTPlayer.teardownAudioStream();
}

/**
 * Native (JNI) implementation of MusicPlayer.loadWavAssetNative()
 */

// the trick is to load the wav files before being asked to play, this way we can make sure that the
// lease possible latency is being introduced.
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_loadWavAssetNative(
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
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_unloadWavAssetsNative(JNIEnv* env, jobject) {
    sDTPlayer.unloadSampleData();
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_trigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer.triggerDown(index);
}

/**
 * Native (JNI) implementation of MusicPlayer.trigger()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_stopTrigger(JNIEnv* env, jobject, jint index) {
    sDTPlayer.triggerUp(index);
}

/**
 * Native (JNI) implementation of MusicPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_getOutputReset(JNIEnv*, jobject) {
    return sDTPlayer.getOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_clearOutputReset(JNIEnv*, jobject) {
    sDTPlayer.clearOutputReset();
}

/**
 * Native (JNI) implementation of MusicPlayer.restartStream()
 */
JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_restartStream(JNIEnv*, jobject) {
    sDTPlayer.resetAll();
    if (sDTPlayer.openStream() && sDTPlayer.startStream()){
        __android_log_print(ANDROID_LOG_INFO, TAG, "openStream successful");
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "openStream failed");
    }
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_setPan(
        JNIEnv *env, jobject thiz, jint index, jfloat pan) {
    sDTPlayer.setPan(index, pan);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_getPan(
        JNIEnv *env, jobject thiz, jint  index) {
    return sDTPlayer.getPan(index);
}

JNIEXPORT void JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_setGain(
        JNIEnv *env, jobject thiz, jint  index, jfloat gain) {
    sDTPlayer.setGain(index, gain);
}

JNIEXPORT jfloat JNICALL Java_in_reconv_oboemusicplayerandrecorder_NativeLib_getGain(
        JNIEnv *env, jobject thiz, jint index) {
    return sDTPlayer.getGain(index);
}


#ifdef __cplusplus
}
#endif
