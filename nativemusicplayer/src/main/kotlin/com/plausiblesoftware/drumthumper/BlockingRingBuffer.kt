package com.plausiblesoftware.drumthumper

import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class BlockingRingBuffer(private val capacity: Int) {
    private val buffer = ByteArray(capacity)
    private var writePos = 0
    private var readPos = 0
    private var size = 0

    private val lock = ReentrantLock()
    private val dataAvailable = lock.newCondition()

    /**
     * Write [data] to the ring buffer.
     * If [data] is larger than free space, the oldest data is overwritten.
     * Notifies readers that new data is available.
     */
    fun write(data: ByteArray) {
        lock.withLock {
            for (byte in data) {
                buffer[writePos] = byte
                writePos = (writePos + 1) % capacity

                if (size < capacity) {
                    size++
                } else {
                    // Overwrite scenario: advance read pointer
                    readPos = (readPos + 1) % capacity
                }
            }
            // Signal that data is available
            dataAvailable.signalAll()
        }
    }

    /**
     * Read exactly [length] bytes in a *blocking* fashion.
     * If there's not enough data, this method waits until data arrives.
     */
    fun readBlocking(output: ByteArray, length: Int) {
        require(length <= output.size) {
            "Output array must be at least $length bytes"
        }
        var readSoFar = 0

        lock.withLock {
            while (readSoFar < length) {
                // Wait until enough data is available
                while (size == 0) {
                    dataAvailable.await()
                }
                // Read one byte at a time
                output[readSoFar] = buffer[readPos]
                readPos = (readPos + 1) % capacity
                size--
                readSoFar++
            }
        }
    }

    fun clear() {
        lock.withLock {
            writePos = 0
            readPos = 0
            size = 0
        }
    }
}
