package `in`.reconv.oboemusicplayer

import android.content.Context
import android.content.res.AssetManager
import android.media.AudioManager
import android.os.Build
import android.util.Log
import java.io.File
import java.io.FileInputStream
import java.io.IOException

class NativeMusicPlayer {
    /**
     * A native method that is implemented by the 'oboemusicplayerandrecorder' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    external fun onRecordingStarted(eventInfo: String): String

    companion object {
        val NUM_PLAY_CHANNELS: Int = 2
        // Used to load the 'oboemusicplayerandrecorder' library on application startup.
        init {
            System.loadLibrary("oboemusicplayerandrecorder")
        }

        val TAG: String = "MusicPlayer"
    }

    private fun loadWavAsset(assetMgr: AssetManager, assetName: String, index: Int, pan: Float) {
        try {
            val assetFD = assetMgr.openFd(assetName)
            val dataStream = assetFD.createInputStream()
            val dataLen = assetFD.getLength().toInt()
            val dataBytes = ByteArray(dataLen)
            dataStream.read(dataBytes, 0, dataLen)
            loadWavAssetNative(dataBytes, index, pan)
            assetFD.close()
        } catch (ex: IOException) {
            Log.i(TAG, "IOException$ex")
        }
    }

    public fun loadWavFile(filePath: String, index: Int, pan: Float) {
        try {
            // Open the file using FileInputStream
            val file = File(filePath)
            val dataStream = FileInputStream(file)

            // Get the file size
            val dataLen = file.length().toInt()

            // Read the file data into a byte array
            val dataBytes = ByteArray(dataLen)
            dataStream.read(dataBytes, 0, dataLen)

            // Call your native function with the data
            loadWavAssetNative(dataBytes, index, pan)

            // Close the stream
            dataStream.close()
        } catch (ex: IOException) {
            Log.i(TAG, "IOException: $ex")
        }
    }


    fun setupAudioStream() {
        setupAudioStreamNative(NUM_PLAY_CHANNELS)
    }

    fun startAudioStream() {
        startAudioStreamNative()
    }

    fun teardownAudioStream() {
        teardownAudioStreamNative()
    }

    // asset-based samples
    fun loadWavAssets(assetMgr: AssetManager) {
        loadWavAsset(assetMgr, "Karoke_aaj_se_teri.wav", 0, 0f)
        loadWavAsset(assetMgr, "Karoke_baaton_ko_teri.wav", 1, 0f)
        loadWavAsset(assetMgr, "Karoke_chahun_mei_ya_naa.wav", 2, 0f)
        loadWavAsset(assetMgr, "Karoke_tum_he_ho.wav", 3, 0f)
    }

    fun unloadWavAssets() {
        unloadWavAssetsNative()
    }

    private external fun setupAudioStreamNative(numChannels: Int)
    private external fun startAudioStreamNative()
    private external fun teardownAudioStreamNative()

    private external fun loadWavAssetNative(wavBytes: ByteArray, index: Int, pan: Float)
    private external fun unloadWavAssetsNative()

    external fun trigger(drumIndex: Int)
    external fun stopTrigger(drumIndex: Int)

    external fun setPan(index: Int, pan: Float)
    external fun getPan(index: Int): Float

    external fun setGain(index: Int, gain: Float)
    external fun getGain(index: Int): Float

    external fun getOutputReset() : Boolean
    external fun clearOutputReset()

    external fun restartStream()
    external fun pauseTrigger()
    external fun resumeTrigger()
    external fun seekToPosition(position: Long)
    external fun getPosition(index: Int): Long

    // External functions for Recording
    external fun create(): Boolean
    external fun isAAudioRecommended(): Boolean
    external fun setAPI(apiType: Int): Boolean
    external fun setEffectOn(isEffectOn: Boolean): Boolean
    external fun setRecordingDeviceId(deviceId: Int)
    external fun setPlaybackDeviceId(deviceId: Int)
    external fun delete()
    external fun native_setDefaultStreamValues(defaultSampleRate: Int, defaultFramesPerBurst: Int)
    external fun setVolume(volume: Float)
    external fun startRecording(
        fullPathTofile: String?,
        inputPresetPreference: Int,
        startRecordingTime: Long
    )

    external fun startRecordingWithoutFile(
        fullPathTofile: String?,
        musicFilePath: String?,
        inputPresetPreference: Int,
        startRecordingTime: Long
    )

    external fun stopRecording()
    external fun resumeRecording()
    external fun pauseRecording()
    external fun getRecordingDelay(): Int
    external fun setCallbackObject(callbackObject: Any?)
    fun setDefaultStreamValues(context: Context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            val myAudioMgr = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
            val defaultSampleRate = sampleRateStr.toInt()
            val framesPerBurstStr =
                myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)
            val defaultFramesPerBurst = framesPerBurstStr.toInt()

            native_setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst)
        }
    }
}