package com.plausiblesoftware.drumthumper

import android.content.Context
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

object PCMFileWriter {

    const val TAG = "PCMFileWriter"
    private var fileOutputStream: FileOutputStream? = null

    /**
     * Save PCM data to a file
     * @param context The application context
     * @param fileName The name of the file (e.g., "sample_audio.pcm")
     * @param data The PCM data to save
     */
    fun savePCMData(context: Context, fileName: String, data: ByteArray) {
        try {
            // Get the application-specific storage directory for audio
            if (fileOutputStream == null) {
                val appSpecificDir = context.getExternalFilesDir(Environment.DIRECTORY_MUSIC)
                val pcmFile = File(appSpecificDir, fileName)

                // Ensure the directory exists
                if (!appSpecificDir!!.exists()) {
                    appSpecificDir.mkdirs()
                }

                fileOutputStream = FileOutputStream(pcmFile)
            }
            fileOutputStream?.write(data)
        } catch (e: IOException) {
            Log.e(TAG, "Error saving PCM data", e)
        }
    }
}
