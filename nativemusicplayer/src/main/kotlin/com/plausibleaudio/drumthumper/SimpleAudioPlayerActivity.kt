package com.plausiblesoftware.drumthumper

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioManager
import android.media.MediaPlayer
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.Manifest
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.arthenica.ffmpegkit.FFmpegKit
import com.arthenica.ffmpegkit.ReturnCode
import `in`.reconv.oboenativemodule.DuplexStreamForegroundService
import `in`.reconv.oboenativemodule.LiveEffectEngine
import `in`.reconv.oboemusicplayerandrecorder.NativeLib
import kotlinx.coroutines.*
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream

class SimpleAudioPlayerActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {

    private val TAG = "SimpleAudioPlayerActivity"
    private var mAudioMgr: AudioManager? = null
    private var mMusicPlayer = MusicPlayer()

    private lateinit var gainSeekBar: SeekBar
    private lateinit var fileSpinner: Spinner
    private lateinit var wavFileList: ArrayList<String> // List of available WAV files in assets
    private var selectedWavFile: String? = null
    private var isRecording = false
    private var mergedFilePath: String? = null

    private var mediaPlayer: MediaPlayer? = null
    private val PERMISSION_REQUEST_CODE = 1001
    private var nativeLib: NativeLib? = null

    init {
        // Load the native library that contains the JNI functions
        System.loadLibrary("drumthumper")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        nativeLib = NativeLib()
        println("Message from the native land: " + nativeLib?.stringFromJNI())
        println("Message from the native land: " + nativeLib?.onRecordingStarted("test"))
        setContentView(R.layout.activity_simple_audio_player)

        // Check and request permissions
        if (!hasPermissions()) {
            requestPermissions()
        }
        
        // Setup oboe recording and start foreground service
        LiveEffectEngine.setCallbackObject(this)
        LiveEffectEngine.setDefaultStreamValues(this)
        volumeControlStream = AudioManager.STREAM_MUSIC

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val serviceIntent = Intent(DuplexStreamForegroundService.ACTION_START, null, this, DuplexStreamForegroundService::class.java)
            startForegroundService(serviceIntent)
        }
        LiveEffectEngine.create()
        LiveEffectEngine.isAAudioRecommended()
        LiveEffectEngine.setAPI(0)

        // Initialize the AudioManager
        mAudioMgr = getSystemService(Context.AUDIO_SERVICE) as AudioManager

        // Initialize the Spinner (Dropdown) and load WAV files from assets
        fileSpinner = findViewById(R.id.fileSpinner)
        loadWavFilesFromAssets()

        // Setup play and stop buttons
        val playButton = findViewById<Button>(R.id.playButton)
        playButton.setOnClickListener {
            if (!isRecording) {
                CoroutineScope(Dispatchers.IO).launch {
                    // Start recording and play audio simultaneously
                    startRecordingAndPlaySample()
                }
            }
        }

        val stopButton = findViewById<Button>(R.id.stopButton)
        stopButton.setOnClickListener { stopAudioSample() }

        // New button to play the merged recording
        val playMergedButton = findViewById<Button>(R.id.playMergedButton)
        playMergedButton.setOnClickListener { playMergedAudio() }

        // Initialize the gain SeekBar
        gainSeekBar = findViewById(R.id.gainSeekBar)
        gainSeekBar.setOnSeekBarChangeListener(this)

        // Handle file selection from the spinner
        fileSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>, view: android.view.View, position: Int, id: Long) {
                selectedWavFile = wavFileList[position]  // Get selected WAV file from assets
                Log.d(TAG, "Selected file: $selectedWavFile")
            }

            override fun onNothingSelected(parent: AdapterView<*>) {
                // No file selected
            }
        }
    }

    private fun hasPermissions(): Boolean {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
    }

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
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                Log.d(TAG, "All permissions granted.")
            } else {
                Toast.makeText(this, "Permissions denied.", Toast.LENGTH_SHORT).show()
                finish()
            }
        }
    }

    fun onRecordingStarted(random : String) {
        println("rand: $random")
    }

    override fun onDestroy() {
        super.onDestroy()
        LiveEffectEngine.delete()
        mediaPlayer?.release()
        mediaPlayer = null
    }

    override fun onStart() {
        super.onStart()
        // Setup audio stream when the activity starts
        mMusicPlayer.setupAudioStream() // Setup audio stream, basically call open stream in simple multi player
        mMusicPlayer.loadWavAssets(assets) // Load WAV files from assets before the call to start even begins
        mMusicPlayer.startAudioStream() // Start the audio stream, this is a check function which makes 
                                        // sure the open stream is working, openstream has already been called in setupAudioStream
        setGainFromSeekBar(gainSeekBar.progress)
    }

    override fun onStop() {
        super.onStop()
        // Unload assets and teardown the audio stream when the activity stops
        mMusicPlayer.teardownAudioStream()
        mMusicPlayer.unloadWavAssets()
    }

    // Load the list of WAV files from assets and populate the spinner
    private fun loadWavFilesFromAssets() {
        wavFileList = ArrayList()
        try {
            // Get the list of files in the assets directory
            val files = assets.list("") // List all files in the assets directory
            if (files != null) {
                for (file in files) {
                    if (file.endsWith(".wav")) {
                        wavFileList.add(file)  // Add only WAV files to the list
                    }
                }
            }
        } catch (e: IOException) {
            Log.e(TAG, "Error loading WAV files from assets: ${e.message}")
        }

        // If no files are found, add a placeholder
        if (wavFileList.isEmpty()) {
            wavFileList.add("No WAV files found")
        }

        // Set up the adapter to display the list in the spinner
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, wavFileList)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        fileSpinner.adapter = adapter
    }

    // Function to start recording and play selected WAV file at the same time
    private suspend fun startRecordingAndPlaySample() = withContext(Dispatchers.IO) {
        val recordedFilePath = getRecordingFilePath("recording.wav")
        if (recordedFilePath != null) {
            isRecording = true

            // Start recording in a separate coroutine
            val recordJob = CoroutineScope(Dispatchers.IO).launch {
                LiveEffectEngine.startRecording(recordedFilePath, 8, System.currentTimeMillis())
                Log.d(TAG, "Recording started")
            }

            // Play audio sample in a separate coroutine simultaneously
            val playJob = CoroutineScope(Dispatchers.IO).launch {
                if (selectedWavFile != null) {
                    playAudioSample(selectedWavFile!!)
                } else {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(this@SimpleAudioPlayerActivity, "No valid WAV file selected", Toast.LENGTH_SHORT).show()
                    }
                }
            }

            // Wait for both jobs to finish (if needed, you can await them if further actions depend on their completion)
            playJob.join()
            recordJob.join()
        } else {
            withContext(Dispatchers.Main) {
                Toast.makeText(this@SimpleAudioPlayerActivity, "Unable to get recording file path", Toast.LENGTH_SHORT).show()
            }
        }
    }


    // Function to play audio sample using the selected WAV file from assets
    private suspend fun playAudioSample(fileName: String) = withContext(Dispatchers.IO) {
        try {
            val inputStream = assets.open(fileName) // Open the selected WAV file from assets
            inputStream.close()

            Log.d(TAG, "Playing file: $fileName")
            println("music started at ${System.currentTimeMillis()}")
            when (fileName) {
                "Karoke_aaj_se_teri.wav" -> mMusicPlayer.trigger(0)
                "Karoke_baaton_ko_teri.wav" -> mMusicPlayer.trigger(1)
                "Karoke_chahun_mei_ya_naa.wav" -> mMusicPlayer.trigger(2)
                "Karoke_tum_he_ho.wav" -> mMusicPlayer.trigger(3)
            }

            withContext(Dispatchers.Main) {
                Toast.makeText(this@SimpleAudioPlayerActivity, "Playing sound: $fileName", Toast.LENGTH_SHORT).show()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error playing sound: ${e.message}")
            withContext(Dispatchers.Main) {
                Toast.makeText(this@SimpleAudioPlayerActivity, "Failed to play sound", Toast.LENGTH_SHORT).show()
            }
        }
    }

    // Function to play merged audio using MediaPlayer
    private fun playMergedAudio() {
        if (mergedFilePath == null) {
            Toast.makeText(this, "No merged audio file to play", Toast.LENGTH_SHORT).show()
            return
        }

        try {
            mediaPlayer?.release()
            mediaPlayer = MediaPlayer().apply {
                setDataSource(mergedFilePath)
                prepare()
                start()
            }

            Toast.makeText(this, "Playing merged audio", Toast.LENGTH_SHORT).show()

        } catch (e: IOException) {
            Log.e(TAG, "Error playing merged audio: ${e.message}")
            Toast.makeText(this, "Failed to play merged audio", Toast.LENGTH_SHORT).show()
        }
    }

    private fun copyAssetToStorage(assetFileName: String): String? {
        val externalDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        val outputFile = File(externalDirectory, assetFileName)
        return try {
            val inputStream: InputStream = assets.open(assetFileName)
            val outputStream = FileOutputStream(outputFile)

            val buffer = ByteArray(1024)
            var length: Int

            while (inputStream.read(buffer).also { length = it } > 0) {
                outputStream.write(buffer, 0, length)
            }

            inputStream.close()
            outputStream.close()

            outputFile.absolutePath
        } catch (e: IOException) {
            e.printStackTrace()
            null
        }
    }
    // Function to stop audio sample and merge the two audio files
    private fun stopAudioSample() {
        CoroutineScope(Dispatchers.IO).launch {
            try {
                when (selectedWavFile) {
                    "Karoke_aaj_se_teri.wav" -> mMusicPlayer.stopTrigger(0)
                    "Karoke_baaton_ko_teri.wav" -> mMusicPlayer.stopTrigger(1)
                    "Karoke_chahun_mei_ya_naa.wav" -> mMusicPlayer.stopTrigger(2)
                    "Karoke_tum_he_ho.wav" -> mMusicPlayer.stopTrigger(3)
                }

                // Ensure LiveEffectEngine is properly stopped
                LiveEffectEngine.stopRecording()
                isRecording = false

                // Merge the recorded file and selected WAV file
                val recordedFilePath = getRecordingFilePath("recording.wav")
                if (recordedFilePath != null && selectedWavFile != null) {
                    val selectedFilePath = copyAssetToStorage(selectedWavFile!!) // Copy asset to accessible location
                    mergedFilePath = getMergedFilePath("merged_${System.currentTimeMillis()}")

                    if (mergedFilePath != null && selectedFilePath != null) {
                        mergeAudioFiles(recordedFilePath, selectedFilePath, mergedFilePath!!)

                        withContext(Dispatchers.Main) {
                            Toast.makeText(this@SimpleAudioPlayerActivity, "Audio files merged successfully!", Toast.LENGTH_SHORT).show()
                        }
                    }
                } else {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(this@SimpleAudioPlayerActivity, "Failed to get file paths", Toast.LENGTH_SHORT).show()
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping sound: ${e.message}")
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@SimpleAudioPlayerActivity, "Failed to stop sound", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    // Helper function to get the merged file path
    private fun getMergedFilePath(fileName: String): String? {
        val externalDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        val mergedFile = File(externalDirectory, "$fileName.wav")
        return mergedFile.absolutePath
    }

    // FFmpeg command to merge the two audio files
    private fun mergeAudioFiles(recordedFilePath: String, selectedFilePath: String, outputFilePath: String) {
        val ffmpegCommand = "-i $recordedFilePath -i $selectedFilePath -filter_complex \"[1]volume=0.5[a];[0][a]amix=inputs=2:duration=first:dropout_transition=2\" $outputFilePath"
        val session = FFmpegKit.execute(ffmpegCommand)

        if (ReturnCode.isSuccess(session.returnCode)) {
            Log.d(TAG, "Merge successful, output at: $outputFilePath")
        } else {
            Log.e(TAG, "Failed to merge audio files: ${session.failStackTrace}")
        }
    }

    // SeekBar.OnSeekBarChangeListener implementation
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (seekBar?.id == R.id.gainSeekBar) {
            setGainFromSeekBar(progress)
        }
    }

    override fun onStartTrackingTouch(seekBar: SeekBar?) {}
    override fun onStopTrackingTouch(seekBar: SeekBar?) {}

    // Function to set gain from the SeekBar progress
    private fun setGainFromSeekBar(progress: Int) {
        val gainValue = progress.toFloat() / 100f
        when (selectedWavFile) {
            "Karoke_aaj_se_teri.wav" -> mMusicPlayer.setGain(0, gainValue)
            "Karoke_baaton_ko_teri.wav" -> mMusicPlayer.setGain(1, gainValue)
            "Karoke_chahun_mei_ya_naa.wav" -> mMusicPlayer.setGain(2, gainValue)
            "Karoke_tum_he_ho.wav" -> mMusicPlayer.setGain(3, gainValue)
        }
        Log.d(TAG, "Gain set to $gainValue")
    }

    // Helper function to get the file path for recording
    private fun getRecordingFilePath(fileName: String): String? {
        val externalDirectory = getExternalFilesDir(Environment.DIRECTORY_MUSIC)
        val recordingFile = File(externalDirectory, "$fileName")
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
}
