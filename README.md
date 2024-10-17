Oboe Samples
==============
These samples demonstrate how to use the Oboe library:

This project implements a multi-sample audio player using the Oboe audio library for Android. 
It supports playing multiple audio samples simultaneously with low-latency performance, 
and now includes the capability to pause and resume both the entire audio stream and individual audio samples.

Features
-------------
**Multiple Sample Playback:** Play multiple audio samples at the same time.
**Low Latency:** Uses Oboe's PerformanceMode::LowLatency for optimal performance.
**Pause/Resume Functionality:**
**Global Pause/Resume:** Pause and resume the entire audio stream with all samples.
**Per-Sample Pause/Resume:** Pause and resume individual audio samples, maintaining their playback positions.
**Error Handling:** Automatically resets and restarts the stream in case of disconnections or errors.
**Pan and Gain Control:** Adjust the panning (left-right audio positioning) and gain (volume) for each sample.

For more information see [Full Guide to Oboe](FullGuide.md).
-------------
* Android device or emulator running API 16 (Jelly Bean) or above
* [Android SDK 26](https://developer.android.com/about/versions/oreo/android-8.0-migration.html#ptb)
* [NDK r17](https://developer.android.com/ndk/downloads/index.html) or above
* [Android Studio 2.3.0+](https://developer.android.com/studio/index.html)