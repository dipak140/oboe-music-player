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

    fun startMusicPlayback(byteArray: ByteArray) {
        isMusicPlaying = true

        val stereoBuffer: ByteBuffer = convertToByteBuffer(byteArray, true)
        // Convert ByteArray to ShortArray
        val shortArray = ByteArrayToShortArray(byteArray)

        // Downmix stereo to mono
        val monoSamples = ShortArray(shortArray.size / 2)
        for (i in monoSamples.indices) {
            val left = shortArray[i * 2].toInt()
            val right = shortArray[i * 2 + 1].toInt()
            monoSamples[i] = ((left + right) / 2).toShort()
        }

        Thread {
            try {
                synchronized(musicQueue) {
                    musicQueue.clear()
                    musicQueue.put(monoSamples)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }.start()
    }

    fun convertToByteBuffer(pcmData: ByteArray, isMono: Boolean): ByteBuffer {
        // Determine output size
        val outputSize = if (isMono) pcmData.size * 2 else pcmData.size
        val byteBuffer = ByteBuffer.allocateDirect(outputSize)
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN) // PCM data is usually little-endian

        if (isMono) {
            // Convert mono to stereo
            var i = 0
            while (i < pcmData.size) {
                // 16-bit samples = 2 bytes
                byteBuffer.put(pcmData[i]) // Left channel (copy original sample)
                byteBuffer.put(pcmData[i + 1]) // Left channel (copy original sample)
                byteBuffer.put(pcmData[i]) // Right channel (duplicate)
                byteBuffer.put(pcmData[i + 1]) // Right channel (duplicate)
                i += 2
            }
        } else {
            // Already stereo, directly add to the ByteBuffer
            byteBuffer.put(pcmData)
        }

        // Reset buffer position to the beginning
        byteBuffer.flip()
        return byteBuffer
    }

    // Helper function to convert ByteArray to ShortArray
//    private fun ByteArrayToShortArray(byteArray: ByteArray): ShortArray {
//        val shortArray = ShortArray(byteArray.size / 2)
//        for (i in shortArray.indices) {
//            shortArray[i] = ((byteArray[i * 2].toInt() and 0xFF) or
//                    (byteArray[i * 2 + 1].toInt() shl 8)).toShort()
//        }
//        return shortArray
//    }

    private fun ByteArrayToShortArray(byteArray: ByteArray): ShortArray {
        val shortBuffer = ByteBuffer.wrap(byteArray)
            .order(ByteOrder.LITTLE_ENDIAN) // Ensure little-endian interpretation
            .asShortBuffer()
        val shortArray = ShortArray(shortBuffer.remaining())
        shortBuffer.get(shortArray)
        return shortArray
    }

    fun stopMusicPlayback() {
        isMusicPlaying = false
        musicQueue.clear()
        currentMusicFrame = null
        currentMusicFramePos = 0
    }
}
//
//package com.plausiblesoftware.drumthumper
//
//import android.media.AudioFormat
//import io.livekit.android.audio.MixerAudioBufferCallback
//import java.nio.ByteBuffer
//import java.util.concurrent.LinkedBlockingQueue
//
//class CustomAudioEffectCallback() : MixerAudioBufferCallback() {
//    private val musicQueue: LinkedBlockingQueue<ByteArray> = LinkedBlockingQueue()
//    private var isMusicPlaying = false
//
//    // Variables for iterating through music samples:
//    private var currentMusicFrame: ByteArray? = null
//    private var currentMusicFramePos = 0
//
//    override fun onBufferRequest(
//        originalBuffer: ByteBuffer,
//        audioFormat: Int,
//        channelCount: Int,
//        sampleRate: Int,
//        bytesRead: Int,
//        captureTimeNs: Long
//    ): BufferResponse? {
//        // Process PCM 16-bit audio directly using ByteArray
//        if (audioFormat == AudioFormat.ENCODING_PCM_16BIT) {
//            val processedBuffer = ByteArray(originalBuffer.remaining())
//
//            // Read original audio data
//            originalBuffer.get(processedBuffer)
//
//            // Apply effects and mix with music
//            for (i in processedBuffer.indices step 2) {
//                val originalSample = getSampleFromBytes(processedBuffer, i)
//                val musicSample = getNextMusicSample()
//
//                // Mix the two samples
//                val mixedSample = ((originalSample + musicSample) / 2)
//                    .coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt())
//                    .toShort()
//
//                // Write back the mixed sample
//                putSampleIntoBytes(processedBuffer, i, mixedSample)
//            }
//
//            // Wrap the processed data in a ByteBuffer
//            val byteBuffer = ByteBuffer.wrap(processedBuffer)
//            return BufferResponse(byteBuffer)
//        }
//
//        // For unsupported formats, return the original buffer
//        return BufferResponse(originalBuffer)
//    }
//
//    private fun getSampleFromBytes(byteArray: ByteArray, index: Int): Short {
//        return ((byteArray[index + 1].toInt() shl 8) or (byteArray[index].toInt() and 0xFF)).toShort()
//    }
//
//    private fun putSampleIntoBytes(byteArray: ByteArray, index: Int, sample: Short) {
//        byteArray[index] = (sample.toInt() and 0xFF).toByte()
//        byteArray[index + 1] = (sample.toInt() shr 8 and 0xFF).toByte()
//    }
//
//    private fun getNextMusicSample(): Short {
//        if (!isMusicPlaying) return 0
//
//        // If no current frame or end of current frame is reached, fetch the next frame
//        if (currentMusicFrame == null || currentMusicFramePos >= currentMusicFrame!!.size) {
//            currentMusicFrame = musicQueue.poll()
//            currentMusicFramePos = 0
//            if (currentMusicFrame == null) return 0
//        }
//
//        val sample = getSampleFromBytes(currentMusicFrame!!, currentMusicFramePos)
//        currentMusicFramePos += 2
//        return sample
//    }
//
//    fun startMusicPlayback(byteArray: ByteArray) {
//        isMusicPlaying = true
//        Thread {
//            try {
//                synchronized(musicQueue) {
//                    musicQueue.clear()
//                    musicQueue.put(byteArray)
//                }
//            } catch (e: Exception) {
//                e.printStackTrace()
//            }
//        }.start()
//    }
//
//    fun stopMusicPlayback() {
//        isMusicPlaying = false
//        synchronized(musicQueue) {
//            musicQueue.clear()
//        }
//        currentMusicFrame = null
//        currentMusicFramePos = 0
//    }
//}
