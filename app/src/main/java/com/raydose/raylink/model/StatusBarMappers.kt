package com.raydose.raylink.model

/** 下拉「在线设备」= 已入库在线探头 + 组播/串口发现但未入库的在线设备 */
fun buildStatusBarConnectedDevices(
    saved: List<SavedProbe>,
    live: Map<String, LiveProbeTelemetry>,
    discovered: List<DiscoveredDevice>,
): List<ConnectedDeviceUi> {
    val fromSaved = saved.mapNotNull { probe ->
        if (live[probe.id]?.isOnline != true) return@mapNotNull null
        ConnectedDeviceUi(
            name = probe.displayName.ifBlank {
                probe.model.ifBlank { "探头" }
            },
            ip = probe.ip.ifBlank { "—" },
            isOnline = true,
        )
    }
    val fromDiscovered = discovered
        .filter { device -> saved.none { matchesSaved(it, device) } }
        .sortedForAddProbeDialog()
        .map { device ->
            ConnectedDeviceUi(
                name = statusBarDiscoveredName(device),
                ip = device.ip.ifBlank { "串口/CAN" },
                isOnline = true,
            )
        }
    return fromSaved + fromDiscovered
}

private fun statusBarDiscoveredName(device: DiscoveredDevice): String {
    val model = device.model.trim().ifBlank { "设备" }
    val addr = device.protoAddr.trim()
    return if (addr.isNotEmpty()) "$model ($addr)" else model
}

/** @deprecated 使用 [buildStatusBarConnectedDevices]；保留以免旧调用编译失败 */
fun HomeUiState.statusBarConnectedDevices(): List<ConnectedDeviceUi> = statusBarDevices
