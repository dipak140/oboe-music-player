package com.plausiblesoftware.drumthumper

import android.content.Context
import android.os.Handler
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import `in`.reconv.oboemusicplayer.NativeMusicPlayer

class NativeKaraokePlayer(private val context: Context) : DefaultLifecycleObserver {

    companion object {
        const val IDLE = -1
        const val BUFFERING = 1
        const val PLAYING = 2
        const val PAUSED = 3
        const val ENDED = 4
        const val PROGRESS_UPDATE_DELAY = 50L
    }

    private val nativePlayer = NativeMusicPlayer()
    private val handler = Handler()
    private val stateLiveData = MutableLiveData<RingtonePlayerState>()
    private var playerState = IDLE
    private var currentPosition: Long = 0
    private var totalDuration: Long = 0

    private var playerId = ""
    private var fileUri = ""
    private var loop = false

    data class RingtonePlayerState(
        var playerId: String = "",
        var playerState: Int = IDLE,
        var progress: Float = 0.0f
    )

    fun getState() = stateLiveData

    override fun onPause(owner: LifecycleOwner) {
        pauseMusic()
        handler.removeCallbacksAndMessages(null)
    }

    override fun onResume(owner: LifecycleOwner) {
        handler.postDelayed(progressUpdateRunnable, PROGRESS_UPDATE_DELAY)
    }

    override fun onDestroy(owner: LifecycleOwner) {
        teardown()
        handler.removeCallbacksAndMessages(null)
        nativePlayer.teardownAudioStream()
    }

    fun initPlayer(playerId: String, fileUri: String, loop: Boolean) {
        this.playerId = playerId
        this.fileUri = fileUri
        this.loop = loop
        nativePlayer.createPlayer()
        nativePlayer.setAPI(0)
        nativePlayer.setupAudioStream()
        nativePlayer.loadWavFile(fileUri, 0, 0f)
        nativePlayer.startAudioStream()
        playerState = IDLE
        updateState()
    }

    fun playMusic() {
        nativePlayer.trigger(0)
        nativePlayer.startAudioStream()
        playerState = PLAYING
        updateState()
    }

    fun pauseMusic() {
        nativePlayer.pauseTrigger()
        playerState = PAUSED
        updateState()
    }

    fun stopMusic() {
        println("stopped")
        println("MusicPlayerTimeStamp: " + nativePlayer.getMusicPlayerTimeStamp())
        println("MusicPlayerFramePosition: " + nativePlayer.getMusicPlayerFramePosition())
        println("RecorderTimeStamp: " + nativePlayer.getRecorderTimeStamp())
        println("RecorderFramePosition: " + nativePlayer.getRecorderFramePosition())
        println("Latency: " + calculateLatency(nativePlayer.getRecorderTimeStamp(), nativePlayer.getRecorderFramePosition(), nativePlayer.getMusicPlayerTimeStamp(), nativePlayer.getMusicPlayerFramePosition(), 44100))
        nativePlayer.stopTrigger(0)
        handler.removeCallbacks(progressUpdateRunnable) // Stop the progress updates
        playerState = IDLE
        updateState()
    }

    fun calculateLatency(
        recorderTimestamp: Long,
        recorderFramePosition: Long,
        musicTimestamp: Long,
        musicFramePosition: Long,
        sampleRate: Int
    ): Double {
        // Calculate timestamp difference in milliseconds
        val timestampDifferenceMs = (recorderTimestamp - musicTimestamp) / 1_000_000.0

        // Convert recorder and music frame positions to time in milliseconds
        val recorderFrameTimeMs = (recorderFramePosition.toDouble() / sampleRate) * 1000.0
        val musicFrameTimeMs = (musicFramePosition.toDouble() / sampleRate) * 1000.0

        // Calculate effective latency
        return timestampDifferenceMs - recorderFrameTimeMs + musicFrameTimeMs
    }

    fun playerSeekTo(milliSecond: Long) {
        nativePlayer.seekToPosition(milliSecond, 44100, 1)
        currentPosition = milliSecond
    }

    fun setPlayerVolume(volume: Float) {
        nativePlayer.setGain(0, volume)
    }

    fun getCurrentPosition(): Long = nativePlayer.getPosition(0, 44100, 1)

    fun getTotalDuration(): Long = totalDuration

    private fun updateState() {
        val progress = if (totalDuration > 0) currentPosition.toFloat() / totalDuration else 0.0f
        stateLiveData.postValue(RingtonePlayerState(playerId, playerState, progress))
    }

    private val progressUpdateRunnable = object : Runnable {
        override fun run() {
            currentPosition = getCurrentPosition()
            totalDuration = nativePlayer.getDuration(44100, 1) // Assuming this is available
            updateState()
            handler.postDelayed(this, PROGRESS_UPDATE_DELAY)
        }
    }

    fun teardown() {
        nativePlayer.stopTrigger(0)
        nativePlayer.teardownAudioStream()
        nativePlayer.unloadWavAssets()
    }

    fun startRecording(recordedFilePath: String, i: Int, currentTimeMillis: Long) {
        nativePlayer.startRecording(recordedFilePath, i, currentTimeMillis)

    }

    fun setEffectOn(Boolean: Boolean) {
        nativePlayer.setEffectOn(Boolean)
    }

    fun stopRecording() {
        nativePlayer.stopRecording()
    }

    fun pauseRecording(){
        nativePlayer.pauseRecording()
    }

    fun resumeRecording() {
        nativePlayer.resumeRecording()
    }

    fun resumeMusic(){
        nativePlayer.resumeTrigger()
        playerState = PLAYING
    }

    fun delete() {
        nativePlayer.delete()
    }

        fun setupKaraokePlayer() {
        nativePlayer.create()
        nativePlayer.setCallbackObject(context)
        nativePlayer.setDefaultStreamValues(context)
    }

    fun getRecorderAudioSessionId(): Int {
        return nativePlayer.getRecorderAudioSessionId()
    }

    fun getPlayerAudioSessionId():Int {
        return nativePlayer.getPlayerAudioSessionId()
    }
}
