package com.plausiblesoftware.drumthumper
import android.content.Context
import android.media.AudioFormat
import android.util.Log
import io.livekit.android.audio.MixerAudioBufferCallback
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.LinkedBlockingQueue

class CustomMusicEffectCallback() : MixerAudioBufferCallback()  {
    private val TAG = "AudioCallback"
    private var isMusicPlaying = false
    private var musicVolume: Float = 0.5f
    private var originalVolume: Float = 1.0f
    private val ringBuffer = BlockingRingBuffer(48000 * 2)

    override fun onBufferRequest(
        originalBuffer: ByteBuffer,
        audioFormat: Int,
        channelCount: Int,
        sampleRate: Int,
        bytesRead: Int,
        captureTimeNs: Long
    ): BufferResponse? {
        if (audioFormat != AudioFormat.ENCODING_PCM_16BIT) {
            return BufferResponse(originalBuffer)
        }

        if(!isMusicPlaying){
            return BufferResponse(originalBuffer)
        }

        val numBytesRequired = bytesRead // Already in bytes?
        val outputBytes = ByteArray(numBytesRequired)
        ringBuffer.readBlocking(outputBytes, numBytesRequired)
        val outputBuffer = ByteBuffer.wrap(outputBytes)
        return BufferResponse(outputBuffer)
    }

    fun onPlayStarted(state: Boolean){
        isMusicPlaying = state;
    }

    fun startMusicPlayback(byteArray: ByteArray) {
        if (!isMusicPlaying){
            return
        }
        if (byteArray.size % 2 != 0) {
            Log.e(TAG, "ByteArray size not even: ${byteArray.size}")
            return
        }
        val monoData = convertStereoToMono(byteArray)
        ringBuffer.write(monoData)
        isMusicPlaying = true
    }

    private fun convertStereoToMono(stereoArray: ByteArray): ByteArray {
        val monoArray = ByteArray(stereoArray.size / 2) // Mono is half the size of stereo
        val stereoBuffer = ByteBuffer.wrap(stereoArray).order(ByteOrder.nativeOrder())
        val monoBuffer = ByteBuffer.wrap(monoArray).order(ByteOrder.nativeOrder())

        while (stereoBuffer.hasRemaining()) {
            val leftChannel = stereoBuffer.short.toInt()
            val rightChannel = stereoBuffer.short.toInt()
            val monoSample = ((leftChannel + rightChannel) * musicVolume / 2).toInt().toShort() // Average the two channels
            monoBuffer.putShort(monoSample)
        }
        return monoArray
    }

    fun stopMusicPlayback() {
        isMusicPlaying = false
        ringBuffer.clear()
    }

    fun setMusicVolume(volume: Float) {
        musicVolume = volume
    }

    fun setOriginalVolume(volume: Float) {
        originalVolume = volume
    }
}
