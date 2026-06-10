package com.raydose.netshield.net

/**
 * 串口字节流拆帧：支持 0x23 / 0x13 等变长 Modbus 风格帧。
 */
class FsySerialFrameCollector {
    private val buffer = ArrayList<Byte>(256)

    @Synchronized
    fun feed(chunk: ByteArray): List<ByteArray> {
        if (chunk.isEmpty()) return emptyList()
        buffer.addAll(chunk.toList())
        val frames = mutableListOf<ByteArray>()
        while (true) {
            val expectedLength = expectedFrameLength() ?: break
            if (buffer.size < expectedLength) break
            val frame = ByteArray(expectedLength) { idx -> buffer[idx] }
            repeat(expectedLength) { buffer.removeAt(0) }
            frames.add(frame)
        }
        return frames
    }

    @Synchronized
    fun reset() {
        buffer.clear()
    }

    private fun expectedFrameLength(): Int? {
        if (buffer.size < 2) return null
        return when (buffer[1].toInt() and 0xFF) {
            0x13, 0x23 -> {
                if (buffer.size < 5) null else (buffer[2].toInt() and 0xFF) + 7
            }
            0x15, 0x16, 0x20, 0x25, 0x83, 0x85, 0x86, 0x90 -> 8
            else -> {
                buffer.removeAt(0)
                null
            }
        }
    }
}
