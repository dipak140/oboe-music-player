package `in`.reconv.oboemusicplayer
import android.util.Log

class AudioDataCallback {
    fun onAudioDataAvailable(audioData: ByteArray) {
        // Handle the audio data
        Log.d("AudioDataCallback", "Audio data received: ${audioData.size} bytes")
    }
}