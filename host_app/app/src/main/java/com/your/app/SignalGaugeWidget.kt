package com.your.app.ui.components



import androidx.compose.animation.animateColorAsState

import androidx.compose.animation.core.animateFloatAsState

import androidx.compose.animation.core.tween

import androidx.compose.foundation.Canvas

import androidx.compose.foundation.background

import androidx.compose.foundation.layout.*

import androidx.compose.foundation.shape.RoundedCornerShape

import androidx.compose.material3.Text

import androidx.compose.runtime.Composable

import androidx.compose.runtime.collectAsState

import androidx.compose.runtime.getValue

import androidx.compose.ui.Alignment

import androidx.compose.ui.Modifier

import androidx.compose.ui.graphics.Color

import androidx.compose.ui.graphics.StrokeCap

import androidx.compose.ui.graphics.drawscope.Stroke

import androidx.compose.ui.text.font.FontWeight

import androidx.compose.ui.unit.dp

import androidx.compose.ui.unit.sp

import com.your.app.nacl.telephony.DecodedCellTowerMetric

import com.your.app.nacl.telephony.SignalQuality

import kotlinx.coroutines.flow.StateFlow



@Composable

fun TelephonySignalGaugeWidget(

    metricFlow: StateFlow<DecodedCellTowerMetric?>

) {

    val metric by metricFlow.collectAsState()



    // Default configuration when telemetry is absent

    val currentMetric = metric ?: DecodedCellTowerMetric(

        0, 0, -120, -120, -25, -5, 0, 0, 0, 0, 0, 0, 0

    )



    // Maps RSRP decibel ranges to a safe 0.0f - 1.0f progress float

    // Standard mapping: -120dBm (0%) to -50dBm (100%)

    val progress = ((currentMetric.rsrp + 120f) / 70f).coerceIn(0f, 1f)

    val animatedProgress by animateFloatAsState(

        targetValue = progress,

        animationSpec = tween(durationMillis = 500),

        label = "RSRPProgress"

    )



    val gaugeColor = when (currentMetric.quality) {

        SignalQuality.EXCELLENT -> Color(0xFF2ECC71) // Vivid Green

        SignalQuality.GOOD -> Color(0xFF3498DB)      // Deep Blue

        SignalQuality.FAIR -> Color(0xFFF1C40F)      // Caution Yellow

        SignalQuality.POOR -> Color(0xFFE74C3C)      // Hazard Red

        SignalQuality.DEAD -> Color(0xFF95A5A6)      // Muted Grey

    }



    val animatedColor by animateColorAsState(

        targetValue = gaugeColor,

        animationSpec = tween(durationMillis = 300),

        label = "GaugeColor"

    )



    Box(

        modifier = Modifier

            .fillMaxWidth()

            .padding(16.dp)

            .background(Color(0xFF1E272C), shape = RoundedCornerShape(16.dp))

            .padding(24.dp),

        contentAlignment = Alignment.Center

    ) {

        Column(

            horizontalAlignment = Alignment.CenterHorizontally,

            verticalArrangement = Arrangement.Center

        ) {

            Text(

                text = "CELLULAR MODEM DIAGNOSTIC",

                color = Color.White.copy(alpha = 0.5f),

                fontSize = 11.sp,

                fontWeight = FontWeight.Bold,

                letterSpacing = 1.sp

            )



            Spacer(modifier = Modifier.height(16.dp))



            Box(

                modifier = Modifier.size(160.dp),

                contentAlignment = Alignment.Center

            ) {

                // Vector Canvas drawing our Arc Gauge

                Canvas(modifier = Modifier.fillMaxSize()) {

                    // Backing Muted Track Arc

                    drawArc(

                        color = Color.White.copy(alpha = 0.1f),

                        startAngle = 135f,

                        sweepAngle = 270f,

                        useCenter = false,

                        style = Stroke(width = 12.dp.toPx(), cap = StrokeCap.Round)

                    )



                    // Active Signal Metric Arc

                    drawArc(

                        color = animatedColor,

                        startAngle = 135f,

                        sweepAngle = animatedProgress * 270f,

                        useCenter = false,

                        style = Stroke(width = 12.dp.toPx(), cap = StrokeCap.Round)

                    )

                }



                Column(horizontalAlignment = Alignment.CenterHorizontally) {

                    Text(

                        text = "${currentMetric.rsrp} dBm",

                        color = Color.White,

                        fontSize = 28.sp,

                        fontWeight = FontWeight.Bold

                    )

                    Text(

                        text = currentMetric.techString,

                        color = animatedColor,

                        fontSize = 14.sp,

                        fontWeight = FontWeight.SemiBold

                    )

                }

            }



            Spacer(modifier = Modifier.height(16.dp))



            // Lower Detailed Diagnostics Box

            Row(

                modifier = Modifier.fillMaxWidth(),

                horizontalArrangement = Arrangement.SpaceEvenly

            ) {

                DiagnosticItem(label = "RSRQ", value = "${currentMetric.rsrq} dB", color = animatedColor)

                DiagnosticItem(label = "SNR", value = "${currentMetric.rssnr} dB", color = animatedColor)

                DiagnosticItem(label = "PCI", value = "${currentMetric.pci}", color = Color.White)

            }

        }

    }

}



@Composable

private fun DiagnosticItem(label: String, value: String, color: Color) {

    Column(horizontalAlignment = Alignment.CenterHorizontally) {

        Text(text = label, color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp)

        Text(text = value, color = color, fontSize = 15.sp, fontWeight = FontWeight.Bold)

    }

}