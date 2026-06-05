package com.raydose.netshield.model

/** 探头管理页编辑态；设备参数即时写从机，底部保存仅写本机名称/位置/列表 */
data class ProbeManageDraft(
    val savedProbe: SavedProbe,
    val isTcpOnline: Boolean = false,
    val doseRateSummary: String = "---",
    val externalAlarmConnected: Boolean = false,
    val doseUpperUsv: String = "10.00",
    val doseLowerUsv: String = "0.10",
    /** 0x52 bit0：1=禁止上限报警；开=启用 */
    val radiationUpperAlarmOn: Boolean = true,
    /** 0x52 bit1：1=禁止下限报警；开=启用 */
    val radiationLowerAlarmOn: Boolean = true,
    val volume: Float = 0.5f,
    val slaveScreenOn: Boolean = true,
    val alarmLightOn: Boolean = true,
) {
    val id: String get() = savedProbe.id
    val displayName: String get() = savedProbe.displayName
    val location: String get() = savedProbe.location
    val protoAddr: String get() = savedProbe.protoAddr
    val ip: String get() = savedProbe.ip

    fun withSavedProbe(probe: SavedProbe): ProbeManageDraft = copy(savedProbe = probe)

    fun withDisplayName(name: String): ProbeManageDraft =
        withSavedProbe(savedProbe.copy(displayName = name))

    fun withLocation(location: String): ProbeManageDraft =
        withSavedProbe(savedProbe.copy(location = location))
}

fun SavedProbe.toManageDraft(
    isTcpOnline: Boolean = false,
    doseRateSummary: String = "---",
    externalAlarmConnected: Boolean = false,
): ProbeManageDraft = ProbeManageDraft(
    savedProbe = this,
    isTcpOnline = isTcpOnline,
    doseRateSummary = doseRateSummary,
    externalAlarmConnected = externalAlarmConnected,
)
