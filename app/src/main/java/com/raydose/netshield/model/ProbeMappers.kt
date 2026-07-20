package com.raydose.netshield.model

import com.raydose.netshield.net.ParsedFsyFrame

fun SavedProbe.toSlaveProbeUi(telemetry: LiveProbeTelemetry?): SlaveProbeUi {
    val live = telemetry ?: LiveProbeTelemetry()
    return SlaveProbeUi(
        id = id,
        name = displayName,
        ip = ip,
        location = location,
        isOnline = live.isOnline,
        doseRateText = if (live.isOnline) live.doseRateText else "---",
        doseUnit = live.doseUnit,
        temperature = live.temperature,
        pressure = live.pressure,
        humidity = live.humidity,
        co2 = live.co2,
        pm25 = live.pm25,
        hasAlarm = live.hasAlarm,
    )
}

fun LiveProbeTelemetry.applyRealtimeUpload(values: List<Long>): LiveProbeTelemetry {
    if (values.size < 8) return this
    if (!isPlausibleRealtimeDoseX100(values[0])) return this
    val dose = values[0] / 100.0
    val temp = values[1] / 10.0
    val pressurePa = values[2]
    val humidityPct = values[3]
    val co2ppm = values[4]
    val pm25 = values[5] / 10.0
    val alarmBit = values[6]
    val statusBit = values[7]
    val doorOpen = (statusBit and 1L) != 0L
    val (_, lightOn, screenOn) = controlEnablesFromBit(statusBit)
    return copy(
        isOnline = true,
        doseRateText = "%.2f".format(dose),
        temperature = "%.1f°C".format(temp),
        pressure = formatProbePressureKpa(pressurePa),
        humidity = "${humidityPct}%",
        co2 = "${co2ppm} ppm",
        pm25 = "%.1f μg/m³".format(pm25),
        hasAlarm = isRadiationDoseAlarmActive(alarmBit),
        alarmBit = alarmBit,
        doorOpen = doorOpen,
        controlBit2Value = statusBit,
        slaveScreenOn = screenOn,
        alarmLightOn = lightOn,
        externalAlarmConnected = isExternalAlarmConnectedFromCtrl2(statusBit),
    )
}

fun LiveProbeTelemetry.applyParsedFrame(frame: ParsedFsyFrame): LiveProbeTelemetry {
    var next = this
    frame.uploadValues?.takeIf { it.size >= 8 }?.let { next = next.applyRealtimeUpload(it) }
    frame.thresholdValues?.let { thr ->
        if (thr.isNotEmpty()) {
            next = next.copy(doseUpperUsv = doseX100ToUsvText(thr[0]))
        }
        if (thr.size >= 2) {
            next = next.copy(doseLowerUsv = doseX100ToUsvText(thr[1]))
        }
    }
    frame.doseHiX100?.let { hi ->
        next = next.copy(isOnline = true, doseUpperUsv = doseX100ToUsvText(hi))
    }
    frame.doseLoX100?.let { lo ->
        next = next.copy(isOnline = true, doseLowerUsv = doseX100ToUsvText(lo))
    }
    frame.alarmEnableValue?.let { enable ->
        next = next.copy(
            isOnline = true,
            radiationUpperAlarmOn = isRadiationUpperAlarmEnabled(enable),
            radiationLowerAlarmOn = isRadiationLowerAlarmEnabled(enable),
        )
    }
    frame.controlBit1Volume?.let { vol ->
        next = next.copy(isOnline = true, volume = volumeRegToSlider(vol))
    }
    frame.controlBit2Value?.let { ctrl ->
        val (_, light, screen) = controlEnablesFromBit(ctrl)
        next = next.copy(
            isOnline = true,
            controlBit2Value = ctrl,
            alarmLightOn = light,
            slaveScreenOn = screen,
            externalAlarmConnected = isExternalAlarmConnectedFromCtrl2(ctrl),
        )
    }
    frame.deviceVersion?.trim()?.takeIf { it.isNotEmpty() }?.let { ver ->
        next = next.copy(isOnline = true, softwareVersion = ver)
    }
    frame.statusBitValue?.let { status ->
        val doorOpen = (status and 1L) != 0L
        next = next.copy(
            isOnline = true,
            doorOpen = doorOpen,
        )
    }
    if (frame.uploadValues == null && frame.thresholdValues == null &&
        frame.doseHiX100 == null && frame.doseLoX100 == null &&
        frame.alarmEnableValue == null && frame.controlBit1Volume == null &&
        frame.controlBit2Value == null && frame.statusBitValue == null &&
        frame.deviceVersion == null
    ) {
        // 写应答 0x16/0x20 等不表示实时在线，避免离线后又被“拉活”
        if (frame.func != 0x16 && frame.func != 0x20) {
            next = next.copy(isOnline = true)
        }
    }
    return next
}

/** 0x23 实时包：剂量、外置声光、status_bit 屏/光使能（不写阈值/音量/0x52） */
fun ProbeManageDraft.mergeRealtimeFromTelemetry(telemetry: LiveProbeTelemetry?): ProbeManageDraft {
    val t = telemetry ?: return this
    return copy(
        isTcpOnline = t.isOnline,
        doseRateSummary = if (t.isOnline) t.doseRateText else "---",
        externalAlarmConnected = t.externalAlarmConnected,
        slaveScreenOn = t.slaveScreenOn ?: slaveScreenOn,
        alarmLightOn = t.alarmLightOn ?: alarmLightOn,
    )
}

/** 0x13 读应答或 0x23 阈值主动上传：合并配置项到草稿 */
fun ProbeManageDraft.mergeConfigFromTelemetry(telemetry: LiveProbeTelemetry?): ProbeManageDraft {
    val t = telemetry ?: return this
    return mergeRealtimeFromTelemetry(t).copy(
        doseUpperUsv = t.doseUpperUsv ?: doseUpperUsv,
        doseLowerUsv = t.doseLowerUsv ?: doseLowerUsv,
        radiationUpperAlarmOn = t.radiationUpperAlarmOn ?: radiationUpperAlarmOn,
        radiationLowerAlarmOn = t.radiationLowerAlarmOn ?: radiationLowerAlarmOn,
        volume = t.volume ?: volume,
        slaveScreenOn = t.slaveScreenOn ?: slaveScreenOn,
        alarmLightOn = t.alarmLightOn ?: alarmLightOn,
        externalAlarmConnected = if (t.controlBit2Value != null) {
            isExternalAlarmConnectedFromCtrl2(t.controlBit2Value)
        } else {
            externalAlarmConnected
        },
        controlBit2Raw = t.controlBit2Value ?: controlBit2Raw,
        softwareVersion = t.softwareVersion?.takeIf { it.isNotBlank() } ?: softwareVersion,
    )
}

fun ProbeManageDraft.mergeFromTelemetry(telemetry: LiveProbeTelemetry?): ProbeManageDraft =
    mergeConfigFromTelemetry(telemetry)

fun deriveDoorState(telemetryMap: Map<String, LiveProbeTelemetry>): DoorState {
    val doors = telemetryMap.values.mapNotNull { it.doorOpen }
    return when {
        doors.isEmpty() -> DoorState.Unknown
        doors.any { it } -> DoorState.Open
        else -> DoorState.Closed
    }
}

fun matchesSaved(probe: SavedProbe, device: DiscoveredDevice): Boolean {
    val probeSerial = probe.serial.trim()
    val deviceSerial = device.serial.trim()
    if (probeSerial.isNotEmpty() && deviceSerial.isNotEmpty()) {
        if (probeSerial.equals(deviceSerial, ignoreCase = true)) return true
    }
    if (probe.id == device.stableId) return true
    if (probe.protoAddr.isNotBlank() && probe.protoAddr == device.protoAddr) {
        // 协议地址相同：IP 变化仍视为同一设备（序列号为空时兜底）
        if (probeSerial.isEmpty() || deviceSerial.isEmpty()) return true
        if (probeSerial.equals(deviceSerial, ignoreCase = true)) return true
    }
    return probe.ip == device.ip && probe.protoAddr == device.protoAddr
}

/** 组播发现同一设备（优先序列号）时，同步 IP/端口等网络信息。无变化返回 null。 */
fun SavedProbe.mergeFromDiscovery(device: DiscoveredDevice): SavedProbe? {
    if (!matchesSaved(this, device)) return null
    val normalizedId = device.stableId.takeIf { device.serial.trim().isNotEmpty() } ?: id
    val merged = copy(
        id = normalizedId,
        serial = device.serial.trim().ifEmpty { serial },
        // 串口/CAN 发现无 IP：保留已保存的网口信息，避免「网口→CAN」时 IP 被清空
        ip = device.ip.ifBlank { ip },
        controlPort = device.controlPort.takeIf { it > 0 } ?: controlPort,
        dataPort = device.dataPort.takeIf { it > 0 } ?: dataPort,
        model = device.model.ifBlank { model },
        protoAddr = device.protoAddr.ifBlank { protoAddr },
    )
    return merged.takeIf { it != this }
}

fun findSavedProbeForDevice(probes: List<SavedProbe>, device: DiscoveredDevice): SavedProbe? =
    probes.firstOrNull { matchesSaved(it, device) }
