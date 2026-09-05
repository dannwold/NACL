package com.your.app.ui



import androidx.compose.foundation.Canvas

import androidx.compose.foundation.layout.fillMaxSize

import androidx.compose.foundation.layout.fillMaxWidth

import androidx.compose.foundation.layout.height

import androidx.compose.runtime.*

import androidx.compose.ui.Modifier

import androidx.compose.ui.geometry.Offset

import androidx.compose.ui.geometry.Size

import androidx.compose.ui.graphics.Color

import androidx.compose.ui.unit.dp

import com.your.app.nacl.NaclAudioBridge

import kotlinx.coroutines.flow.collect



@Composable

fun AudioWaveformWidget(

    modifier: Modifier = Modifier,

    activeColor: Color = Color(0xFF00E676),

    inactiveColor: Color = Color(0x3300E676)

) {

    // Keep a local list of historically pushed sound heights

    var currentEnvelope by remember { mutableStateOf(FloatArray(32) { 0.05f }) }



    LaunchedEffect(Unit) {

        NaclAudioBridge.waveformStream.collect { frame ->

            currentEnvelope = frame.envelope

        }

    }



    Canvas(

        modifier = modifier

            .fillMaxWidth()

            .height(180.dp)

    ) {

        val width = size.width

        val height = size.height

        val centerY = height / 2f

        val barCount = currentEnvelope.size

        val barSpacing = 4f

        val totalSpacing = barSpacing * (barCount - 1)

        val barWidth = (width - totalSpacing) / barCount



        for (i in 0 until barCount) {

            // Envelope amplitude values are normalized from 0.0f to 1.0f

            val amplitude = currentEnvelope[i].coerceIn(0.01f, 1.0f)

            val barHeight = amplitude * height * 0.9f // scale slightly to fit

            val x = i * (barWidth + barSpacing)



            // Draw symmetric waveform bars radiating out from the center line

            drawRect(

                color = activeColor,

                topLeft = Offset(x, centerY - (barHeight / 2f)),

                size = Size(barWidth, barHeight)

            )

        }

    }

}