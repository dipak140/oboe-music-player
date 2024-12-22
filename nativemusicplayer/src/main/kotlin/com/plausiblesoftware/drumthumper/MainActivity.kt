package com.plausiblesoftware.drumthumper

import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Environment
import android.widget.Button
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import `in`.reconv.oboemusicplayer.NativeMusicPlayer
import io.livekit.android.AudioOptions
import io.livekit.android.AudioType
import io.livekit.android.LiveKit
import io.livekit.android.LiveKitOverrides
import io.livekit.android.events.RoomEvent
import io.livekit.android.events.collect
import io.livekit.android.renderer.SurfaceViewRenderer
import io.livekit.android.room.Room
import io.livekit.android.room.participant.LocalParticipant
import io.livekit.android.room.track.LocalAudioTrack
import io.livekit.android.room.track.LocalAudioTrackOptions
import io.livekit.android.room.track.VideoTrack
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

class MainActivity: AppCompatActivity() {

    private var playButton: Button? = null
    private var stopButton: Button? = null
    private val nativePlayer = NativeMusicPlayer()
    lateinit var room: Room
    lateinit var localParticipant: LocalParticipant


    private var isAudioMuted = false
    private var isVideoMuted = false
    private var isRoomConnected = false
    private var audioEffectCallback = NewCustomAudioCallback(this)
    private var fileOutputStream: FileOutputStream? = null

    val wsURL = "wss://roxstar-afhk8zna.livekit.cloud"
    val apiUrl = "https://api.dev.rockstarstudioz.com/v1/secret/generate-token-livekit"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_audio_livekit_player)
        requestPermissions()

        val externalMusicDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC);
        val music = File(externalMusicDirectory, "Karoke_aaj_se_teri.wav")

        room = LiveKit.create(applicationContext, overrides = LiveKitOverrides(
            audioOptions = AudioOptions(
                audioOutputType = AudioType.MediaAudioType()
            )
        )
        )
        room.initVideoRenderer(findViewById<SurfaceViewRenderer>(R.id.surfaceViewRenderer))
        room.initVideoRenderer(findViewById<SurfaceViewRenderer>(R.id.surfaceViewRendererRemote))

        // Connect to the room
        fetchTokenAndConnect()
        findViewById<Button>(R.id.btn_mute_audio).setOnClickListener { toggleAudio() }
        findViewById<Button>(R.id.btn_mute_video).setOnClickListener { toggleVideo() }
        findViewById<Button>(R.id.btn_end_call).setOnClickListener { endCall() }
        playButton = findViewById(R.id.playButton)
        stopButton = findViewById(R.id.stopButton)
        nativePlayer.createPlayer()
        nativePlayer.setupAudioStream()
        nativePlayer.loadWavFile(music.absolutePath, 0, 0f)
        nativePlayer.startAudioStream()
        nativePlayer.setCallbackObject(this)
        playButton!!.setOnClickListener{
            nativePlayer.trigger(0)
            audioEffectCallback.onPlayStarted(true)
        }

        stopButton!!.setOnClickListener{
            nativePlayer.stopTrigger(0)
        }
    }

    private fun requestPermissions() {
        val neededPermissions = listOf(
            android.Manifest.permission.RECORD_AUDIO,
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
            android.Manifest.permission.CAMERA
        ).filter {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_DENIED
        }.toTypedArray()

        if (neededPermissions.isNotEmpty()) {
            registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
                permissions.forEach { (permission, granted) ->
                    if (!granted) {
                        Toast.makeText(this, "Missing permission: $permission", Toast.LENGTH_SHORT).show()
                    }
                }
            }.launch(neededPermissions)
        }
    }

    private fun fetchTokenAndConnect() {
        lifecycleScope.launch {
            val token = fetchTokenFromServer()
            if (token != null) {
                println("Token fetched: $token")
                Toast.makeText(this@MainActivity, "Token fetched: $token", Toast.LENGTH_SHORT).show()
                connectToRoom(token)
            } else {
                Toast.makeText(this@MainActivity, "Failed to fetch token", Toast.LENGTH_SHORT).show()
            }
        }
    }


    private suspend fun fetchTokenFromServer(): String? = withContext(Dispatchers.IO) {
        val client = OkHttpClient()
        val json = JSONObject()
        json.put("room-name", "01e192c2-6596-4d67-a24b-962088c7856f")
        json.put("identity", "Dipak")

        val requestBody = RequestBody.create("application/json".toMediaType(), json.toString())
        val request = Request.Builder()
            .url(apiUrl)
            .header("Content-Type", "application/json")
            .post(requestBody)
            .build()

        try {
            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val responseBody = response.body?.string()
                val jsonResponse = JSONObject(responseBody ?: "")
                jsonResponse.getString("token")
            } else {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "Error fetching token: ${response.message}", Toast.LENGTH_SHORT).show()
                }
                null
            }
        } catch (e: Exception) {
            runOnUiThread {
                Toast.makeText(this@MainActivity, "Network error: ${e.message}", Toast.LENGTH_SHORT).show()
            }
            null
        }
    }

    private fun connectToRoom(token: String) {
        lifecycleScope.launch {

            launch {
                room.events.collect { event ->
                    when (event) {
                        is RoomEvent.TrackSubscribed -> onTrackSubscribed(event)
                        else -> {}
                    }
                }
            }

            room.connect(wsURL, token)

            isRoomConnected = true

            localParticipant = room.localParticipant

            // Enable microphone and camera
            localParticipant.setMicrophoneEnabled(true)
            localParticipant.setCameraEnabled(false)

            // Attach video track
            val videoTrack = localParticipant.getTrackPublication(io.livekit.android.room.track.Track.Source.CAMERA)?.track as? VideoTrack
            videoTrack?.let { attachVideo(it) }

            // Add custom audio effect
            val audioTrack = localParticipant.getTrackPublication(io.livekit.android.room.track.Track.Source.MICROPHONE)?.track as? LocalAudioTrack
            audioTrack?.setAudioBufferCallback(audioEffectCallback)
        }
    }

    private fun onTrackSubscribed(event: RoomEvent.TrackSubscribed) {
        val track = event.track
        if (track is VideoTrack) {
            attachRemoteVideo(track)
        }
    }

    // Do not delete
    fun onAudioDataAvailable(audioData: ByteArray) {
        try {
            // Use app-specific external files directory
            if (fileOutputStream == null) {
                val appSpecificDir = getExternalFilesDir(Environment.DIRECTORY_MUSIC) // App-specific storage
                println(appSpecificDir)
                val pcmFile = File(appSpecificDir, "output_audio.pcm")

                // Ensure the directory exists
                if (!appSpecificDir!!.exists()) {
                    appSpecificDir.mkdirs()
                }

                fileOutputStream = FileOutputStream(pcmFile)
            }

            // Write the byte array to the file
            fileOutputStream?.write(audioData)

            // If room is connected, start music playback
            if (isRoomConnected) {
                audioEffectCallback.startMusicPlayback(audioData)
            }

        } catch (e: IOException) {
            println("Error Error Error + " + e)
            e.printStackTrace()
        }
    }

    private fun attachRemoteVideo(videoTrack: VideoTrack){
        val surfaceViewRenderer = findViewById<SurfaceViewRenderer>(R.id.surfaceViewRendererRemote)
        videoTrack.addRenderer(surfaceViewRenderer)
    }

    private fun attachVideo(videoTrack: VideoTrack) {
        val surfaceViewRenderer = findViewById<SurfaceViewRenderer>(R.id.surfaceViewRenderer)
        videoTrack.addRenderer(surfaceViewRenderer)
    }

    private fun toggleAudio() {
        lifecycleScope.launch {
            isAudioMuted = !isAudioMuted
            localParticipant.setMicrophoneEnabled(!isAudioMuted)
            val button = findViewById<Button>(R.id.btn_mute_audio)
            button.text = if (isAudioMuted) "Unmute Audio" else "Mute Audio"
        }
    }

    private fun toggleVideo() {
        lifecycleScope.launch {
            isVideoMuted = !isVideoMuted
            localParticipant.setCameraEnabled(!isVideoMuted)
            val button = findViewById<Button>(R.id.btn_mute_video)
            button.text = if (isVideoMuted) "Unmute Video" else "Mute Video"
        }
    }

    private fun endCall() {
        room.disconnect()
        finish() // Close the activity
    }
}