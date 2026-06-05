package com.raydose.netshield.net

/**
 * 组播发现串：型号,序列号,ip,控制端口,数据流端口,协议地址,协议类型,预留
 */
data class FsyBroadcast(
    val model: String,
    val serial: String,
    val ip: String,
    val controlPort: Int,
    val dataPort: Int,
    val protoAddr: String,
    val protoType: String,
    val extra: String,
    val raw: String,
)

fun parseFsyBroadcast(raw: String): FsyBroadcast? {
    val parts = raw.split(",").map { it.trim() }
    if (parts.size < 8) return null
    return try {
        FsyBroadcast(
            model = parts[0],
            serial = parts[1],
            ip = parts[2],
            controlPort = parts[3].toInt(),
            dataPort = parts[4].toInt(),
            protoAddr = parts[5],
            protoType = parts[6],
            extra = parts[7],
            raw = raw,
        )
    } catch (_: Exception) {
        null
    }
}
