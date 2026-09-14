package com.your.app.nacl



import android.app.Service

import android.content.Intent

import android.graphics.PixelFormat

import android.os.IBinder

import android.view.Gravity

import android.view.SurfaceHolder

import android.view.SurfaceView

import android.view.WindowManager

import kotlinx.coroutines.CoroutineScope

import kotlinx.coroutines.Dispatchers

import kotlinx.coroutines.cancel

import kotlinx.coroutines.launch



class NaclOverlayService : Service() {

    private lateinit var windowManager: WindowManager

    private lateinit var overlayView: SurfaceView

    private val serviceScope = CoroutineScope(Dispatchers.Main)

    private var displayContextHandle: Long = 0



    override fun onCreate() {

        super.onCreate()

        windowManager = getSystemService(WINDOW_SERVICE) as WindowManager

        overlayView = SurfaceView(this)



        // Configure Window params to force rendering above all applications

        val params = WindowManager.LayoutParams(

            WindowManager.LayoutParams.MATCH_PARENT,

            400, // Fixed height for visualizer wave bar

            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,

            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,

            PixelFormat.TRANSLUCENT

        ).apply {

            gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL

            x = 0

            y = 100

        }



        // Add to active display view hierarchy

        windowManager.addView(overlayView, params)



        overlayView.holder.addCallback(object : SurfaceHolder.Callback {

            override fun surfaceCreated(holder: SurfaceHolder) {

                serviceScope.launch(Dispatchers.Default) {

                    // Pass the raw Java surface object to JNI

                    displayContextHandle = nativeInitializeDisplay(holder.surface)

                    startWaveformPollingLoop()

                }

            }



            override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {}

            override fun surfaceDestroyed(holder: SurfaceHolder) {

                nativeReleaseDisplay(displayContextHandle)

                displayContextHandle = 0

            }

        })

    }



    private fun startWaveformPollingLoop() {

        serviceScope.launch(Dispatchers.Default) {

            while (displayContextHandle != 0L) {

                // Read processed envelope coefficients from libaudio.so

                val envelopeData = NaclBridge.getLatestAudioEnvelope()



                // Pipe directly to our GPU rendering pipeline

                nativeUpdateDisplayWaveform(displayContextHandle, envelopeData)

                nativeRenderDisplayFrame(displayContextHandle)



                // Throttle to 60 FPS (approx. 16.6ms)

                Thread.sleep(16)

            }

        }

    }



    override fun onBind(intent: Intent?): IBinder? = null



    override fun onDestroy() {

        super.onDestroy()

        serviceScope.cancel()

        windowManager.removeView(overlayView)

    }



    // JNI Native Gateway Methods mapping directly to libdisplay.so

    private external fun nativeInitializeDisplay(surface: Any): Long

    private external fun nativeUpdateDisplayWaveform(context: Long, data: FloatArray)

    private external fun nativeRenderDisplayFrame(context: Long)

    private external fun nativeReleaseDisplay(context: Long)

}