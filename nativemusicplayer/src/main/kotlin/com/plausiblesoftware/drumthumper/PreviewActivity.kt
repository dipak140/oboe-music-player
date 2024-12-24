package com.plausiblesoftware.drumthumper

import android.os.Bundle
import android.os.Environment
import android.widget.Button
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File

class PreviewActivity : AppCompatActivity() {

    private val TAG = "PreviewActivity"
    private var player1: PreviewActivityNativePlayer? = null
    private var player2: PreviewActivityNativePlayer? = null
    private var playButton: Button? = null
    private var stopButton: Button? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_simple_audio_player)

        val externalDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        val music1 = File(externalDirectory, "music1.wav")
        val music2 = File(externalDirectory, "music2.wav")

        player1 = PreviewActivityNativePlayer()
        player2 = PreviewActivityNativePlayer()
        playButton = findViewById(R.id.playButton)
        stopButton = findViewById(R.id.stopButton)

        player1!!.initPlayer("1", music1.absolutePath, music2.absolutePath, false, 0)

        playButton!!.setOnClickListener {
            CoroutineScope(Dispatchers.IO).launch {
                player1!!.playMusic()
            }
        }

        stopButton!!.setOnClickListener {
            CoroutineScope(Dispatchers.IO).launch {
                player1!!.stopMusic()
            }
        }
    }
}