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
        doseRateText = live.doseRateText,
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
        externalAlarmConnected = isExternalAlarmConnected(alarmBit),
        doorOpen = doorOpen,
        controlBit2Value = statusBit,
        slaveScreenOn = screenOn,
        alarmLightOn = lightOn,
    )
}

fun LiveProbeTelemetry.applyParsedFrame(frame: ParsedFsyFrame): LiveProbeTelemetry {
    var next = this
    frame.uploadValues?.takeIf { it.size >= 8 }?.let { next = next.applyRealtimeUpload(it) }
    frame.thresholdValues?.takeIf { it.size >= 2 }?.let { thr ->
        next = next.copy(
            isOnline = true,
            doseUpperUsv = doseX100ToUsvText(thr[0]),
            doseLowerUsv = doseX100ToUsvText(thr[1]),
        )
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
        )
    }
    frame.statusBitValue?.let { status ->
        val doorOpen = (status and 1L) != 0L
        val (_, light, screen) = controlEnablesFromBit(status)
        next = next.copy(
            isOnline = true,
            doorOpen = doorOpen,
            controlBit2Value = status,
            alarmLightOn = light,
            slaveScreenOn = screen,
        )
    }
    if (frame.uploadValues == null && frame.thresholdValues == null &&
        frame.alarmEnableValue == null && frame.controlBit1Volume == null &&
        frame.controlBit2Value == null && frame.statusBitValue == null
    ) {
        next = next.copy(isOnline = true)
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
        ip = device.ip,
        controlPort = device.controlPort,
        dataPort = device.dataPort,
        model = device.model.ifBlank { model },
        protoAddr = device.protoAddr.ifBlank { protoAddr },
    )
    return merged.takeIf { it != this }
}

fun findSavedProbeForDevice(probes: List<SavedProbe>, device: DiscoveredDevice): SavedProbe? =
    probes.firstOrNull { matchesSaved(it, device) }
