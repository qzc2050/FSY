package com.raydose.netshield.model

/** 探头管理页编辑态；设备参数即时写从机，底部保存仅写本机名称/位置/列表 */
data class ProbeManageDraft(
    val savedProbe: SavedProbe,
    val isTcpOnline: Boolean = false,
    val doseRateSummary: String = "---",
    val externalAlarmConnected: Boolean = false,
    val doseUpperUsv: String = "10.00",
    val doseLowerUsv: String = "0.10",
    /** reg 82 bit0=上阈值报警使能，bit1=下阈值报警使能（Neiji：1=启用） */
    val radiationUpperAlarmOn: Boolean = true,
    /** reg 82 bit1 */
    val radiationLowerAlarmOn: Boolean = true,
    val volume: Float = 0.5f,
    val slaveScreenOn: Boolean = true,
    val alarmLightOn: Boolean = true,
    /** 最近一次读到的 reg123 原值，写时保留 bit15 等未编辑位 */
    val controlBit2Raw: Long = NEIJI_CTRL2_DEFAULT,
    /** reg98 软件版本；离线或未读到时为空 */
    val softwareVersion: String = "",
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
