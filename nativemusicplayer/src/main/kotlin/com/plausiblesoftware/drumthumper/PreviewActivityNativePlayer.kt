package com.plausiblesoftware.drumthumper
import `in`.reconv.oboemusicplayer.NativeMusicPlayer
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class PreviewActivityNativePlayer {

    companion object{
        const val TAG = "PreviewActivityNativePlayer"
        const val IDLE = -1
        const val PLAYING = 1
        const val PAUSED = 2
        const val ENDED = 3
    }

    private val nativePlayer = NativeMusicPlayer();
    private val nativePlayer1 = NativeMusicPlayer();
    private val nativePlayer2 = NativeMusicPlayer();
    private var playerId = 0;
    private var playerState = IDLE;
    private var player1State = IDLE;
    private var player2State = IDLE;
    private var player1fileUri = "";
    private var player2fileUri = "";

    fun getCurrentPosition(index: Int): Long = nativePlayer.getPosition(index, 44100, 1)

    fun getTotalDuration(playerNumber: Int): Long{
        var totalDuration = 0L;
        if (playerNumber == 1){
            totalDuration = nativePlayer1.getDuration(44100, 1)
        }
        if (playerNumber == 2){
            totalDuration = nativePlayer2.getDuration(44100, 1)
        }
        return totalDuration
    }


    fun initPlayer(playerId: String, player1fileUri: String, player2fileUri: String, loop: Boolean, index: Int) {
        this.playerId = playerId.toInt()

        this.player1fileUri = player1fileUri
        this.player2fileUri = player2fileUri

        // Create player
        nativePlayer1.createPlayer()
        nativePlayer2.createPlayer()

        // Set API
        nativePlayer1.setAPI(0)
        nativePlayer2.setAPI(0)

        // Setup audio stream
        nativePlayer1.setupAudioStream()
        nativePlayer2.setupAudioStream()

        // Load wav file
        nativePlayer1.loadWavFile(player1fileUri, 0, 0f)
        nativePlayer2.loadWavFile(player2fileUri, 1, 0f)

//        // Start Audio Stream
//        nativePlayer1.startAudioStream()
//        nativePlayer2.startAudioStream()
    }

    fun playMusic() {
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer1.trigger(0)
            nativePlayer1.startAudioStream()
        }
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer2.trigger(1)
            nativePlayer2.startAudioStream()
        }

        player1State = PLAYING
        player2State = PLAYING
    }

    fun pauseMusic() {
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer1.pauseTrigger()
        }
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer2.pauseTrigger()
        }

        player1State = PAUSED
        player2State = PAUSED
    }

    fun playerSeekTo(milliSecond: Long, playerNumber: Int) {
        if (playerNumber == 1){
            nativePlayer1.seekToPosition(milliSecond, 44100, 1)
        }
        if (playerNumber == 2){
            nativePlayer2.seekToPosition(milliSecond, 44100, 1)
        }
    }

    fun setPlayerVolume(volume: Float, playerNumber: Int) {
        if (playerNumber == 1){
            nativePlayer1.setGain(0, volume )
        }
        if (playerNumber == 2){
            nativePlayer2.setGain(0, volume)
        }
    }

    fun resumeMusic(){
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer1.resumeTrigger()
        }
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer2.resumeTrigger()
        }
        player1State = PLAYING
        player2State = PLAYING
    }

    fun delete() {
        nativePlayer1.delete()
        nativePlayer2.delete()
    }

    fun stopMusic() {
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer1.stopTrigger(0)
            nativePlayer1.teardownAudioStream()
            nativePlayer1.unloadWavAssets()
        }
        CoroutineScope(Dispatchers.IO).launch {
            nativePlayer2.stopTrigger(1)
            nativePlayer2.teardownAudioStream()
            nativePlayer2.unloadWavAssets()
        }

        player1State = ENDED
        player2State = ENDED
    }

}