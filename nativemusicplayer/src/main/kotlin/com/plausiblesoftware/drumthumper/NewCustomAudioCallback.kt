package com.plausiblesoftware.drumthumper
import android.content.Context
import android.media.AudioFormat
import android.util.Log
import io.livekit.android.audio.MixerAudioBufferCallback
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.LinkedBlockingQueue
import kotlin.math.max

class NewCustomAudioCallback(context: Context) : MixerAudioBufferCallback()  {
    private val TAG = "AudioCallback"
    private val musicQueue: LinkedBlockingQueue<ByteArray> = LinkedBlockingQueue(200000) // Max buffer size: 100 frames
    private var isMusicPlaying = false
    private var currentMusicFrame: ByteArray? = null
    private var currentMusicFramePos = 0
    private var musicVolume: Float = 0.5f
    private var originalVolume: Float = 0.5f

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
        // Get original audio data (mono vocal)
        var musicData1 = musicQueue.poll()?.let { ByteBuffer.wrap(it) }
        var musicData2 = musicQueue.poll()?.let { ByteBuffer.wrap(it) }

        if (musicData1 == null){
            musicData1 = musicQueue.poll()?.let { ByteBuffer.wrap(it) }
        }
        if (musicData2 == null){
            musicData2 = musicQueue.poll()?.let { ByteBuffer.wrap(it) }
        }
        if (musicData2 == null || musicData1 == null){
            return BufferResponse(null)
        }
        val combinedBuffer = ByteBuffer.allocate(musicData1.remaining() + musicData2.remaining())
        combinedBuffer.put(musicData1.slice()) // Slice ensures position/limit are respected

        // Copy the contents of buffer2 into the combined buffer
        combinedBuffer.put(musicData2.slice())

        // Flip the buffer to prepare it for reading
        combinedBuffer.flip()
        mixByteBuffers(originalBuffer, combinedBuffer)
        return BufferResponse(combinedBuffer)
    }

    fun onPlayStarted(state: Boolean){
        isMusicPlaying = state;
    }


    private fun mixByteBuffers(
        original: ByteBuffer,
        addBuffer: ByteBuffer,
    ) {
        println("Cap1 "+ original.capacity())
        println("Cap2 "+ addBuffer.capacity())
        val size = max(original.capacity(), addBuffer.capacity())
        if (size <= 0) return
        for (i in 0 until size) {
            val sum = ((original[i]*originalVolume).toInt() + (addBuffer[i]*musicVolume).toInt())
                .coerceIn(Byte.MIN_VALUE.toInt(), Byte.MAX_VALUE.toInt())
            original.put(i, sum.toByte())
        }
    }

    fun startMusicPlayback(byteArray: ByteArray) {
        if (!isMusicPlaying){
            return
        }

        if (byteArray.size % 2 != 0) {
            Log.e(TAG, "ByteArray size not even: ${byteArray.size}")
            return
        }

        currentMusicFrame = byteArray.copyOf()
        currentMusicFramePos = 0
        isMusicPlaying = true
        synchronized(musicQueue) {
            musicQueue.put(convertStereoToMono(byteArray))
        }

    }

    private fun convertStereoToMono(stereoArray: ByteArray): ByteArray {
        val monoArray = ByteArray(stereoArray.size / 2) // Mono is half the size of stereo
        val stereoBuffer = ByteBuffer.wrap(stereoArray).order(ByteOrder.nativeOrder())
        val monoBuffer = ByteBuffer.wrap(monoArray).order(ByteOrder.nativeOrder())

        while (stereoBuffer.hasRemaining()) {
            val leftChannel = stereoBuffer.short.toInt()
            val rightChannel = stereoBuffer.short.toInt()
            val monoSample = ((leftChannel + rightChannel) / 2).toShort() // Average the two channels
            monoBuffer.putShort(monoSample)
        }

        return monoArray
    }

    fun stopMusicPlayback() {
        isMusicPlaying = false
        synchronized(musicQueue) {
            musicQueue.clear()
        }
        currentMusicFrame = null
        currentMusicFramePos = 0
    }

    fun setMusicVolume(volume: Float) {
        musicVolume = volume
    }

    fun setOriginalVolume(volume: Float) {
        originalVolume = volume
    }
}
