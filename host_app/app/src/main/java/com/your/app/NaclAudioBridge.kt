package com.your.app.nacl



import kotlinx.coroutines.flow.MutableSharedFlow

import kotlinx.coroutines.flow.asSharedFlow

import java.nio.ByteBuffer

import java.nio.ByteOrder



object NaclAudioBridge {

    private val _waveformStream = MutableSharedFlow<WaveformUIFrame>(extraBufferCapacity = 60)

    val waveformStream = _waveformStream.asSharedFlow()



    data class WaveformUIFrame(

        val rms: Float,

        val peak: Float,

        val db: Float,

        val envelope: FloatArray

    )



    /**

     * Entry point triggered directly by the native libaudio worker thread.

     * Extracts byte data via an allocated Direct ByteBuffer.

     */

    @JvmStatic

    fun onNativeAudioFrame(buffer: ByteBuffer) {

        buffer.order(ByteOrder.nativeOrder())



        // Match the packed struct fields: window_size(4B), rms(4B), peak(4B), db(4B)

        val windowSize = buffer.int

        val rms = buffer.float

        val peak = buffer.float

        val db = buffer.float



        // Read the downsampled envelope values (32 floats = 128 bytes)

        val envelope = FloatArray(32)

        for (i in 0 until 32) {

            envelope[i] = buffer.float

        }



        val uiFrame = WaveformUIFrame(rms, peak, db, envelope)

        _waveformStream.tryEmit(uiFrame)

    }

}