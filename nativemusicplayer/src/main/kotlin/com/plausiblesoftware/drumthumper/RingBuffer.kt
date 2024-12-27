package com.plausiblesoftware.drumthumper

class RingBuffer(capacity: Int) {
    private val buffer = ByteArray(capacity)
    private var writePos = 0
    private var readPos = 0
    private var size = 0 // how many valid bytes currently in the buffer

    // Write to ring buffer
    fun write(data: ByteArray) {
        for (byte in data) {
            buffer[writePos] = byte
            writePos = (writePos + 1) % buffer.size
            if (size < buffer.size) {
                size++
            } else {
                // Overwrite if full, i.e., move readPos forward
                readPos = (readPos + 1) % buffer.size
            }
        }
    }

    // Read from ring buffer
    fun read(output: ByteArray): Int {
        val bytesToRead = minOf(output.size, size)
        for (i in 0 until bytesToRead) {
            output[i] = buffer[readPos]
            readPos = (readPos + 1) % buffer.size
        }
        size -= bytesToRead
        return bytesToRead
    }

    fun clear() {
        writePos = 0
        readPos = 0
        size = 0
    }
}
