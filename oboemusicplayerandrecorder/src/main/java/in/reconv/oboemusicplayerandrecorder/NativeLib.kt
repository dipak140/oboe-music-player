package `in`.reconv.oboemusicplayerandrecorder

import android.content.res.AssetManager
import android.util.Log
import java.io.IOException

class NativeLib {
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
}