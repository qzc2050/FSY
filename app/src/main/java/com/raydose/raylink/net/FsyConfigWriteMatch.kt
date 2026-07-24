package com.raydose.raylink.net

/** 写请求帧对应的应答期望（0x06→0x16，0x10→0x20） */
data class WriteAckExpectation(
    val deviceAddr: Int,
    val ackFunc: Int,
    val reg: Int,
    val count: Int? = null,
)

fun parseWriteAckExpectation(frame: ByteArray): WriteAckExpectation? {
    if (frame.size < 6) return null
    val addr = frame[0].toUByte().toInt()
    val func = frame[1].toUByte().toInt()
    val reg = frame[2].toUByte().toInt() or (frame[3].toUByte().toInt() shl 8)
    return when (func) {
        0x06 -> WriteAckExpectation(deviceAddr = addr, ackFunc = 0x16, reg = reg)
        0x10 -> {
            val count = frame[4].toUByte().toInt() or (frame[5].toUByte().toInt() shl 8)
            WriteAckExpectation(deviceAddr = addr, ackFunc = 0x20, reg = reg, count = count)
        }
        else -> null
    }
}

fun ParsedFsyFrame.matchesWriteAck(expect: WriteAckExpectation): Boolean {
    if (!crcOk || addr != expect.deviceAddr) return false
    return when (expect.ackFunc) {
        0x16 -> func == 0x16 && writeAckReg == expect.reg
        0x20 -> {
            func == 0x20 &&
                writeMultiStartReg == expect.reg &&
                (expect.count == null || writeMultiCount == expect.count)
        }
        else -> false
    }
}
