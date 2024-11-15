package com.plausiblesoftware.drumthumper

import android.content.Context
import androidx.lifecycle.MutableLiveData
import `in`.reconv.oboemusicplayer.NativeMusicPlayer

class KaraokePlayerWithOboe(private val context: Context) {
    companion object {
        const val IDLE = -1
        const val BUFFERING = 1
        const val PLAYING = 2
        const val PAUSED = 3
        const val ENDED = 4
        const val PROGRESS_UPDATE_DELAY = 50L
    }

    private val nativePlayer = NativeMusicPlayer()
    private val handler = android.os.Handler()
    private val stateLiveData = MutableLiveData<RingtonePlayerState>()
    private var playerState = IDLE
    private var currentPosition: Long = 0
    private var totalDuration: Long = 0

    data class RingtonePlayerState(
        var playerId: String = "",
        var playerState: Int = IDLE,
        var progress: Float = 0.0f
    )

    private var playerId = ""
    private var fileUri = ""

    fun setupKaraokePlayer() {
        nativePlayer.create()
        nativePlayer.setCallbackObject(context)
        nativePlayer.setDefaultStreamValues(context)
    }

    fun initPlayer(playerId: String, fileUri: String, loop: Boolean, context: Context) {
        this.playerId = playerId
        this.fileUri = fileUri
        nativePlayer.isAAudioRecommended()
        nativePlayer.setAPI(0)
        nativePlayer.setupAudioStream()
        nativePlayer.loadWavFile(fileUri, 0, 0f) // Load file for playback
        playerState = IDLE
    }

    fun playMusic() {
        nativePlayer.startAudioStream()
        nativePlayer.trigger(0)
        playerState = PLAYING
    }

    fun pauseMusic() {
        nativePlayer.pauseTrigger()
        playerState = PAUSED
    }

    fun stopMusic() {
        nativePlayer.stopTrigger(0)
        nativePlayer.teardownAudioStream()
        playerState = IDLE
    }

    fun playerSeekTo(milliSecond: Long) {
        nativePlayer.seekToPosition(milliSecond)
        currentPosition = milliSecond
    }

    fun setPlayerVolume(volume: Float) {
        nativePlayer.setGain(0,volume)
    }

    fun getCurrentPosition(): Long {
        return currentPosition
    }

    fun pauseRecording(){
        nativePlayer.pauseRecording()
    }

    fun resumeRecording() {
        nativePlayer.resumeRecording()
    }

    fun delete() {
        nativePlayer.delete()
    }

    fun teardown() {
        nativePlayer.teardownAudioStream()
        nativePlayer.unloadWavAssets()
    }

    fun startRecording(recordedFilePath: String, i: Int, currentTimeMillis: Long) {
        nativePlayer.startRecording(recordedFilePath, i, currentTimeMillis)
    }


    fun stopRecording() {
        nativePlayer.stopRecording()
    }
}
