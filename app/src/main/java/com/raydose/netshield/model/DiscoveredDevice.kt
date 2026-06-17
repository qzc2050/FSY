package com.raydose.netshield.model

import com.raydose.netshield.net.FsyBroadcast

/** 组播发现的在线从机（尚未保存或已保存） */
data class DiscoveredDevice(
    val model: String,
    val serial: String,
    val ip: String,
    val controlPort: Int,
    val dataPort: Int,
    val protoAddr: String,
    val lastSeenMillis: Long,
) {
    val stableId: String
        get() = serial.trim().takeIf { it.isNotEmpty() }
            ?: "${ip.trim()}_${protoAddr.trim()}"

    fun toSavedProbe(displayName: String? = null): SavedProbe = SavedProbe(
        id = stableId,
        model = model,
        serial = serial,
        ip = ip,
        controlPort = controlPort,
        dataPort = dataPort,
        protoAddr = protoAddr,
        displayName = displayName?.trim().takeUnless { it.isNullOrEmpty() }
            ?: defaultDisplayName(),
        location = "",
    )

    private fun defaultDisplayName(): String {
        val addr = protoAddr.trim()
        return if (addr.isNotEmpty()) "Detector $addr" else model.ifBlank { "Detector" }
    }

    companion object {
        fun fromBroadcast(b: FsyBroadcast, nowMillis: Long = System.currentTimeMillis()): DiscoveredDevice =
            DiscoveredDevice(
                model = b.model,
                serial = b.serial,
                ip = b.ip,
                controlPort = b.controlPort,
                dataPort = b.dataPort,
                protoAddr = b.protoAddr,
                lastSeenMillis = nowMillis,
            )
    }
}

/** 添加探头弹窗：按 IP 升序固定排序，避免组播刷新时列表跳动。 */
fun Collection<DiscoveredDevice>.sortedForAddProbeDialog(): List<DiscoveredDevice> =
    sortedWith(
        compareBy(
            { ipv4SortKey(it.ip) },
            { it.protoAddr.trim() },
            { it.serial.trim() },
        ),
    )

private fun ipv4SortKey(ip: String): Long {
    val parts = ip.trim().split('.')
    if (parts.size != 4) return Long.MAX_VALUE
    var key = 0L
    for (part in parts) {
        val octet = part.toIntOrNull() ?: return Long.MAX_VALUE
        if (octet !in 0..255) return Long.MAX_VALUE
        key = (key shl 8) or octet.toLong()
    }
    return key
}
