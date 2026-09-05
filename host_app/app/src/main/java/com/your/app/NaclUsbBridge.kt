package com.your.app.ui



import androidx.compose.foundation.background

import androidx.compose.foundation.layout.*

import androidx.compose.foundation.lazy.LazyColumn

import androidx.compose.foundation.lazy.items

import androidx.compose.material3.Text

import androidx.compose.runtime.*

import androidx.compose.ui.Modifier

import androidx.compose.ui.graphics.Color

import androidx.compose.ui.text.font.FontFamily

import androidx.compose.ui.unit.dp

import androidx.compose.ui.unit.sp

import com.your.app.nacl.NaclUsbBridge

import kotlinx.coroutines.flow.map



@Composable

fun UsbDiagnosticTerminal(modifier: Modifier = Modifier) {

    val incomingDataList = remember { mutableStateListOf<String>() }



    // Collect the low-latency raw flow and append hex output to console list

    LaunchedEffect(Unit) {

        NaclUsbBridge.usbEvents

            .map { bytes -> bytes.joinToString(" ") { String.format("%02X", it) } }

            .collect { hexString ->

                if (incomingDataList.size > 100) incomingDataList.removeAt(0)

                incomingDataList.add("[RX] $hexString")

            }

    }



    Column(modifier = modifier.fillMaxSize().padding(16.dp)) {

        Text(

            text = "USB RAW BULK CAPTURE LOGGER",

            color = Color.Green,

            fontSize = 14.sp,

            modifier = Modifier.padding(bottom = 8.dp)

        )

        LazyColumn(

            modifier = Modifier

                .fillMaxSize()

                .background(Color.Black)

                .padding(8.dp)

        ) {

            items(incomingDataList) { logLine ->

                Text(

                    text = logLine,

                    color = Color.Green,

                    fontFamily = FontFamily.Monospace,

                    fontSize = 12.sp

                )

            }

        }

    }

}