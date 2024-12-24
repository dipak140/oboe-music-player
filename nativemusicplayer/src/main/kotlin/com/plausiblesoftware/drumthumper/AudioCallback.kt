package com.plausiblesoftware.drumthumper
import android.util.Log

class AudioCallback {
    fun onAudioDataAvailable(audioData: ByteArray) {
        // Handle the audio data here
        Log.d("AudioDataCallback", "Audio data received: ${audioData.size} bytes")
        // Process or store the audio data as needed
    }
}