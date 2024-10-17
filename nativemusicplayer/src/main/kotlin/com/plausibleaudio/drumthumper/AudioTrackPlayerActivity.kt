package com.plausiblesoftware.drumthumper

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.widget.Button
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.plausiblesoftware.drumthumper.R
import `in`.reconv.oboenativemodule.LiveEffectEngine
import `in`.reconv.oboenativemodule.DuplexStreamForegroundService
import kotlinx.coroutines.*
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

class AudioTrackPlayerActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {

    private val TAG = "AudioTrackPlayerActivity"
    private lateinit var audioTrack: AudioTrack
    private var audioData: ByteArray? = null
    private var isPaused = false
    private var playbackPosition = 0
    private lateinit var gainSeekBar: SeekBar
    private var sampleRate = 44100 // Default sample rate
    private var channelCount = 2    // Stereo (2 channels)
    private val PERMISSION_REQUEST_CODE = 1001

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_audio_track_player)

        LiveEffectEngine.setCallbackObject(this)
        LiveEffectEngine.setDefaultStreamValues(this)
        volumeControlStream = AudioManager.STREAM_MUSIC

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val serviceIntent =
                Intent(DuplexStreamForegroundService.ACTION_START, null, this, DuplexStreamForegroundService::class.java)
            startForegroundService(serviceIntent)
        }
        LiveEffectEngine.create()
        LiveEffectEngine.isAAudioRecommended()
        LiveEffectEngine.setAPI(0)

        // Check for permissions on startup
        if (!hasPermissions()) {
            requestPermissions()
        }

        // Load audio data from assets (WAV file)
        audioData = loadWavFile("output.wav")

        // Setup buttons for play, pause, and stop
        val playButton = findViewById<Button>(R.id.playButton)
        playButton.setOnClickListener {
            if (isPaused) {
                resumeAudio()
            } else {
                startRecording()
            }
        }

        val pauseButton = findViewById<Button>(R.id.pauseButton)
        pauseButton.setOnClickListener {
            pauseAudio()
        }

        val stopButton = findViewById<Button>(R.id.stopButton)
        stopButton.setOnClickListener {
            stopAudio()
        }

        // Gain/Volume control using SeekBar
        gainSeekBar = findViewById(R.id.gainSeekBar)
        gainSeekBar.setOnSeekBarChangeListener(this)
    }

    // Check necessary permissions
    private fun hasPermissions(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
                && ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
                && ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
    }

    // Request required permissions
    private fun requestPermissions() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf(
                Manifest.permission.RECORD_AUDIO,
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE
            ),
            PERMISSION_REQUEST_CODE
        )
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST_CODE && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            Log.d(TAG, "All permissions granted.")
        } else {
            Log.e(TAG, "Permissions not granted.")
            finish()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        audioTrack.release()
        LiveEffectEngine.delete()
    }

    // Load WAV file from assets and extract audio properties
    private fun loadWavFile(fileName: String): ByteArray? {
        return try {
            val inputStream: InputStream = assets.open(fileName)
            val header = ByteArray(44)
            inputStream.read(header)

            sampleRate = ByteBuffer.wrap(header, 24, 4).order(ByteOrder.LITTLE_ENDIAN).int
            Log.d(TAG, "Sample Rate: $sampleRate Hz")

            channelCount = ByteBuffer.wrap(header, 22, 2).order(ByteOrder.LITTLE_ENDIAN).short.toInt()
            Log.d(TAG, "Channel Count: $channelCount channels")

            inputStream.readBytes()
        } catch (e: IOException) {
            Log.e(TAG, "Error loading WAV file: ${e.message}")
            null
        }
    }

    // Get file path for recording
    fun getRecordingFilePath(fileName: String): String? {
        val externalDirectory: File? = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        val recordingFile = File(externalDirectory, "$fileName.wav")

        return try {
            if (!recordingFile.exists()) {
                recordingFile.createNewFile()
            }
            recordingFile.absolutePath
        } catch (e: IOException) {
            e.printStackTrace()
            null
        }
    }

    // Start recording and play audio when recording starts
    private fun startRecording() {
        val path = getRecordingFilePath("recording")
        LiveEffectEngine.startRecording(path, 8, System.currentTimeMillis())
    }

    // DO NOT Remove, called from oboenative
    fun onRecordingStarted(eventInfo: String) {
        runOnUiThread {
            playAudio()
        }
        Log.d(TAG, "Recording started: $eventInfo")
    }

    // Play audio function with threading
    private fun playAudio() {
        if (audioData == null) return

        val channelMask = if (channelCount == 1) {
            AudioFormat.CHANNEL_OUT_MONO
        } else {
            AudioFormat.CHANNEL_OUT_STEREO
        }

        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelMask)
                    .build()
            )
            .setBufferSizeInBytes(AudioTrack.getMinBufferSize(sampleRate, channelMask, AudioFormat.ENCODING_PCM_16BIT))
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()

        audioTrack.play()

        CoroutineScope(Dispatchers.IO).launch {
            streamAudioData()
        }

        isPaused = false
        Toast.makeText(this, "Playing audio...", Toast.LENGTH_SHORT).show()
    }

    // Streaming audio data in a coroutine
    private suspend fun streamAudioData() {
        withContext(Dispatchers.IO) {
            var offset = 0
            val bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelCount, AudioFormat.ENCODING_PCM_16BIT)
            while (offset < audioData!!.size) {
                val chunkSize = minOf(bufferSize, audioData!!.size - offset)
                audioTrack.write(audioData!!, offset, chunkSize)
                offset += chunkSize
            }
        }
    }

    // Pause audio playback
    private fun pauseAudio() {
        if (::audioTrack.isInitialized && audioTrack.playState == AudioTrack.PLAYSTATE_PLAYING) {
            playbackPosition = audioTrack.playbackHeadPosition
            audioTrack.pause()
            isPaused = true
            Toast.makeText(this, "Audio paused", Toast.LENGTH_SHORT).show()
        }
    }

    // Resume audio playback
    private fun resumeAudio() {
        if (isPaused) {
            audioTrack.setPlaybackHeadPosition(playbackPosition)
            audioTrack.play()
            isPaused = false
            Toast.makeText(this, "Resuming audio...", Toast.LENGTH_SHORT).show()
        }
    }

    // Stop audio playback
    private fun stopAudio() {
        if (::audioTrack.isInitialized && audioTrack.playState != AudioTrack.PLAYSTATE_STOPPED) {
            audioTrack.stop()
            isPaused = false
            playbackPosition = 0
            Toast.makeText(this, "Audio stopped", Toast.LENGTH_SHORT).show()
        }
    }

    // Handle gain (volume) changes
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (::audioTrack.isInitialized) {
            val volume = progress / 100.0f
            audioTrack.setVolume(volume)
            Log.d(TAG, "Volume set to $volume")
        }
    }

    override fun onStartTrackingTouch(seekBar: SeekBar?) {}
    override fun onStopTrackingTouch(seekBar: SeekBar?) {}
}
