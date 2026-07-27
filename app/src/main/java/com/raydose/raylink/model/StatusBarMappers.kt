package com.raydose.raylink.model

/** 无 IP 时连接方式展示（中英文相同）；带地址时为 CAN/LoRa(3) */
const val LINK_CAN_LORA_LABEL = "CAN/LoRa"

fun linkCanLoraLabel(protoAddr: String): String {
    val addr = protoAddr.trim()
    return if (addr.isNotEmpty()) "$LINK_CAN_LORA_LABEL($addr)" else LINK_CAN_LORA_LABEL
}

/**
 * 下拉「在线设备」= 已入库在线探头 + 组播/CAN/LoRa 发现但未入库的在线设备。
 *
 * - 已添加：第一行设备名，第二行 IP 或 CAN/LoRa(地址)
 * - 未添加：第一行「未添加 · 型号」，第二行 CAN/LoRa(地址) —— 多台靠地址区分
 */
fun buildStatusBarConnectedDevices(
    saved: List<SavedProbe>,
    live: Map<String, LiveProbeTelemetry>,
    discovered: List<DiscoveredDevice>,
    notAddedLabel: String = "未添加",
    deviceFallback: String = "设备",
    probeFallback: String = "探头",
): List<ConnectedDeviceUi> {
    val fromSaved = saved.mapNotNull { probe ->
        if (live[probe.id]?.isOnline != true) return@mapNotNull null
        ConnectedDeviceUi(
            name = probe.displayName.ifBlank {
                probe.model.ifBlank { probeFallback }
            },
            ip = probe.ip.ifBlank { linkCanLoraLabel(probe.protoAddr) },
            isOnline = true,
        )
    }
    val fromDiscovered = discovered
        .filter { device -> saved.none { matchesSaved(it, device) } }
        .sortedForAddProbeDialog()
        .map { device ->
            ConnectedDeviceUi(
                name = statusBarDiscoveredName(device, notAddedLabel, deviceFallback),
                ip = device.ip.ifBlank { linkCanLoraLabel(device.protoAddr) },
                isOnline = true,
            )
        }
    return fromSaved + fromDiscovered
}

private fun statusBarDiscoveredName(
    device: DiscoveredDevice,
    notAddedLabel: String,
    deviceFallback: String,
): String {
    val model = device.model.trim().ifBlank { deviceFallback }
    return "$notAddedLabel · $model"
}
