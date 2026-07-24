package com.raydose.raylink.model

import com.raydose.raylink.net.FsyBroadcast

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

        /**
         * 串口/CAN/LoRa 经 zjb 上报的 0x23 序列号广播（无 IP、无型号字段）。
         * 首次添加默认型号 [ProductModels.PROBE_RK100P]；产线/网口后续可用真实型号覆盖。
         */
        fun fromSerialSerialUpload(
            protoAddr: Int,
            serial: String,
            model: String = ProductModels.PROBE_RK100P,
            nowMillis: Long = System.currentTimeMillis(),
        ): DiscoveredDevice = DiscoveredDevice(
            model = model,
            serial = serial,
            ip = "",
            controlPort = 0,
            dataPort = 0,
            protoAddr = protoAddr.toString(),
            lastSeenMillis = nowMillis,
        )
    }
}

/** 发现列表排序：按 IP 升序固定排序，避免组播刷新时列表跳动。 */
fun Collection<DiscoveredDevice>.sortedForAddProbeDialog(): List<DiscoveredDevice> =
    sortedWith(
        compareBy(
            { ipv4SortKey(it.ip) },
            { it.protoAddr.trim() },
            { it.serial.trim() },
        ),
    )

/** 「添加探头」仅列出可入库的内机型号（RK100P/D/N 等）。 */
fun Collection<DiscoveredDevice>.forAddProbeDialog(): List<DiscoveredDevice> =
    filter { ProductModels.isManageableProbeModel(it.model) }
        .sortedForAddProbeDialog()

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
