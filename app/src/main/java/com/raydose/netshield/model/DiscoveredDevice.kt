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
