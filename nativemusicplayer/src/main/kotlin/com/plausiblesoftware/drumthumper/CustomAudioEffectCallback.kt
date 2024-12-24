package com.plausiblesoftware.drumthumper
import android.media.AudioFormat
import io.livekit.android.audio.MixerAudioBufferCallback
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.LinkedBlockingQueue

class CustomAudioEffectCallback() : MixerAudioBufferCallback() {
    private val musicQueue: LinkedBlockingQueue<ShortArray> = LinkedBlockingQueue()
    private var isMusicPlaying = false

    // Variables for iterating through music samples:
    private var currentMusicFrame: ShortArray? = null
    private var currentMusicFramePos = 0

    override fun onBufferRequest(
        originalBuffer: ByteBuffer,
        audioFormat: Int,
        channelCount: Int,
        sampleRate: Int,
        bytesRead: Int,
        captureTimeNs: Long
    ): BufferResponse? {
        // Process PCM 16-bit audio
        if (audioFormat == AudioFormat.ENCODING_PCM_16BIT) {
            val shortBuffer = originalBuffer.asShortBuffer()
            val processedBuffer = ShortArray(shortBuffer.limit())

            // Read original audio data
            shortBuffer.get(processedBuffer)

            // Apply echo effect and mix with music
            for (i in processedBuffer.indices) {
                val originalSample = processedBuffer[i]
                var musicSample = getNextMusicSample()

                // Mix original sample with delayed echo and music sample
                processedBuffer[i] =
                    ((originalSample.toInt() * 0.5) + (musicSample.toInt() * 0.5)).toInt().toShort()
            }

            // Write processed data to a new ByteBuffer
            val byteBuffer = ByteBuffer.allocateDirect(processedBuffer.size * 2)
                .order(ByteOrder.nativeOrder())
            byteBuffer.asShortBuffer().put(processedBuffer)

            return BufferResponse(byteBuffer)
        }

        // For unsupported formats, return the original buffer
        return BufferResponse(originalBuffer)
    }

    private fun getNextMusicSample(): Short {
        if (!isMusicPlaying) return 0
        // If we have no current frame or we've reached the end of it, fetch the next one
        if (currentMusicFrame == null || currentMusicFramePos >= currentMusicFrame!!.size) {
            currentMusicFrame = musicQueue.poll() ?: return 0
            currentMusicFramePos = 0
        }

        val sample = currentMusicFrame!![currentMusicFramePos]
        currentMusicFramePos++
        return sample
    }

    fun startMusicPlayback(shortArray: ShortArray) {
        isMusicPlaying = true
        val monoSamples = ShortArray(shortArray.size / 2)
        for (i in monoSamples.indices) {
            val left = shortArray[i * 2]
            val right = shortArray[i * 2 + 1]
            monoSamples[i] = ((left + right) / 2).toShort()
        }
        Thread {
            try {
                musicQueue.clear()
                musicQueue.put(monoSamples)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }.start()
    }


    fun stopMusicPlayback() {
        isMusicPlaying = false
        musicQueue.clear()
        currentMusicFrame = null
        currentMusicFramePos = 0
    }
}