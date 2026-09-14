package com.your.app.nacl.telephony



import java.nio.ByteBuffer

import java.nio.ByteOrder



enum class SignalQuality { EXCELLENT, GOOD, FAIR, POOR, DEAD }



data class DecodedCellTowerMetric(

    val techType: Int,

    val connectionStatus: Int,

    val dbm: Int,

    val rsrp: Int,

    val rsrq: Int,

    val rssnr: Int,

    val asu: Int,

    val mcc: Int,

    val mnc: Int,

    val tac: Int,

    val cellId: Int,

    val pci: Int,

    val arfcn: Int

) {

    val quality: SignalQuality

        get() = when {

            rsrp >= -80 -> SignalQuality.EXCELLENT

            rsrp >= -90 -> SignalQuality.GOOD

            rsrp >= -100 -> SignalQuality.FAIR

            rsrp >= -110 -> SignalQuality.POOR

            else -> SignalQuality.DEAD

        }



    val techString: String

        get() = when (techType) {

            13 -> "LTE"

            19 -> "5G NR"

            else -> "HSPA/UMTS"

        }

}



object CellTowerMetricDecoder {

    /**

     * Decodes a packed CellTowerMetric struct from raw binary payload.

     * Struct size: 1 byte + 1 byte + (11 * 4 bytes) = 46 bytes total.

     */

    fun decode(payload: ByteArray): DecodedCellTowerMetric {

        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.nativeOrder())



        val type = buffer.get().toInt() and 0xFF

        val status = buffer.get().toInt() and 0xFF

        val dbm = buffer.int

        val rsrp = buffer.int

        val rsrq = buffer.int

        val rssnr = buffer.int

        val asu = buffer.int

        val mcc = buffer.int

        val mnc = buffer.int

        val tac = buffer.int

        val cellId = buffer.int

        val pci = buffer.int

        val arfcn = buffer.int



        return DecodedCellTowerMetric(

            type, status, dbm, rsrp, rsrq, rssnr, asu, mcc, mnc, tac, cellId, pci, arfcn

        )

    }

}