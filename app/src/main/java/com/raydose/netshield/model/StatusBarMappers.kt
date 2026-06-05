package com.raydose.netshield.model

/** 从主页探头列表生成状态栏「已连接设备」列（协议发现后 [SlaveProbeUi.ip] 有值） */
fun HomeUiState.statusBarConnectedDevices(): List<ConnectedDeviceUi> =
    slaveProbes
        .filter { it.isOnline }
        .map { probe ->
            ConnectedDeviceUi(
                name = probe.name,
                ip = probe.ip.ifBlank { "—" },
                isOnline = true,
            )
        }
