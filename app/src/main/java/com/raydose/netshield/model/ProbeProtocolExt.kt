package com.raydose.netshield.model

import com.raydose.netshield.net.buildWriteMultiRegsFrame
import com.raydose.netshield.net.buildWriteSingleRegFrame
import com.raydose.netshield.net.u32ToLe
import kotlin.math.roundToInt

/** 组播「协议地址」→ Modbus 从机地址（默认 1） */
fun SavedProbe.modbusDeviceAddr(): Byte {
    val n = protoAddr.trim().toIntOrNull() ?: 1
    return n.coerceIn(1, 247).toByte()
}

/** Neiji reg123 bit15：外置 CAN 报警在线（0=未连接，暂只读显示） */
fun isExternalAlarmConnectedFromCtrl2(value: Long): Boolean = (value shr 15) and 1L != 0L

/** 转接板 alarm_bit bit26/30（非 Neiji 探头） */
fun isExternalAlarmConnected(alarmBit: Long): Boolean {
    val soundOffline = (alarmBit shr 26) and 1L != 0L
    val lightOffline = (alarmBit shr 30) and 1L != 0L
    return !soundOffline || !lightOffline
}

/** alarm_bit bit0=辐射上限报警，bit1=辐射下限报警（与 0x52 使能位同序；1=正在报警） */
private const val RADIATION_UPPER_ALARM_BIT = 0
private const val RADIATION_LOWER_ALARM_BIT = 1

/** 仅辐射剂量上/下阈值超限视为报警；温湿压、CO2、PM2.5、声光离线等 bit 忽略。 */
fun isRadiationDoseAlarmActive(alarmBit: Long): Boolean {
    val upper = (alarmBit shr RADIATION_UPPER_ALARM_BIT) and 1L != 0L
    val lower = (alarmBit shr RADIATION_LOWER_ALARM_BIT) and 1L != 0L
    return upper || lower
}

/** Neiji 0x23 alarm_bit：环境传感器离线位（与内机 fsy_regmap UpdateEnv 一致） */
enum class NeijiEnvSensorKind(val label: String, val offlineBits: Long) {
    TEMP_HUMIDITY("温湿度", (1L shl 6) or (1L shl 14)),
    PRESSURE("气压", 1L shl 10),
    CO2("CO2", 1L shl 18),
    PM25("PM2.5", 1L shl 22),
    ;

    fun isOffline(alarmBit: Long): Boolean = (alarmBit and offlineBits) != 0L
}

private val NEIJI_ENV_SENSOR_OFFLINE_MASK =
    NeijiEnvSensorKind.entries.fold(0L) { acc, kind ->
        acc or kind.offlineBits
    }

/** 解析当前离线环境传感器；无离线返回空集 */
fun neijiEnvSensorsOffline(alarmBit: Long): Set<NeijiEnvSensorKind> =
    NeijiEnvSensorKind.entries.filter { it.isOffline(alarmBit) }.toSet()

fun hasNeijiEnvSensorOffline(alarmBit: Long): Boolean =
    (alarmBit and NEIJI_ENV_SENSOR_OFFLINE_MASK) != 0L

/** Neiji reg 82：bit0/bit1=1 表示启用上/下限剂量报警 */
fun isRadiationUpperAlarmEnabled(alarmEnable: Long): Boolean = (alarmEnable and 1L) != 0L

fun isRadiationLowerAlarmEnabled(alarmEnable: Long): Boolean = (alarmEnable and 2L) != 0L

fun alarmEnableRegisterValue(upperOn: Boolean, lowerOn: Boolean): Long {
    var v = 0L
    if (upperOn) v = v or 1L
    if (lowerOn) v = v or 2L
    return v
}

fun doseX100ToUsvText(x100: Long): String = "%.2f".format(x100 / 100.0)

fun usvTextToX100(text: String): Long? =
    text.trim().replace(',', '.').toDoubleOrNull()?.let { (it * 100.0).roundToInt().toLong() }

/** reg123 / status：bit13 光报警，bit14 背光（1=开，用内机 bright_sz），bit15 外置报警在线 */
fun controlEnablesFromBit(value: Long): Triple<Boolean, Boolean, Boolean> {
    val sound = (value shr 12) and 1L != 0L
    val light = (value shr 13) and 1L != 0L
    val screen = (value shr 14) and 1L != 0L
    return Triple(sound, light, screen)
}

/** reg123 默认：bit13+bit14=1（光报警开、背光开） */
const val NEIJI_CTRL2_DEFAULT: Long = (1L shl 13) or (1L shl 14)

fun mergeControlBit2Enables(current: Long, screenOn: Boolean, lightOn: Boolean): Long {
    var v = current
    v = setBit(v, 13, lightOn)
    v = setBit(v, 14, screenOn)
    return v
}

private fun setBit(value: Long, bit: Int, on: Boolean): Long =
    if (on) value or (1L shl bit) else value and (1L shl bit).inv()

/** 音量寄存器 0~100 → UI 滑条 0~1 */
fun volumeRegToSlider(reg: Int): Float = (reg.coerceIn(0, 100) / 100f).coerceIn(0f, 1f)

fun volumeSliderToReg(volume: Float): Int = (volume.coerceIn(0f, 1f) * 100f).roundToInt()

/** 写 reg 82 报警使能（bit0 上限、bit1 下限，1=启用） */
fun ProbeManageDraft.buildAlarmEnableWriteFrame(): ByteArray =
    buildWriteMultiRegsFrame(
        NeijiProbeRegs.ALARM_ENABLE,
        u32ToLe(alarmEnableRegisterValue(radiationUpperAlarmOn, radiationLowerAlarmOn).toInt()),
        savedProbe.modbusDeviceAddr(),
    )

/** 上限行报警 checkbox：reg 82 + reg 50 */
fun ProbeManageDraft.buildUpperAlarmCheckboxWriteFrames(): List<ByteArray> {
    val frames = mutableListOf(buildAlarmEnableWriteFrame())
    buildDoseUpperWriteFrame()?.let { frames += it }
    return frames
}

/** 下限行报警 checkbox：reg 82 + reg 52 */
fun ProbeManageDraft.buildLowerAlarmCheckboxWriteFrames(): List<ByteArray> {
    val frames = mutableListOf(buildAlarmEnableWriteFrame())
    buildDoseLowerWriteFrame()?.let { frames += it }
    return frames
}

/** 写 reg 122 音量 0~100 */
fun ProbeManageDraft.buildVolumeWriteFrame(): ByteArray =
    buildWriteSingleRegFrame(
        NeijiProbeRegs.ALARM_VOLUME,
        volumeSliderToReg(volume),
        savedProbe.modbusDeviceAddr(),
    )

/** 写 reg123：仅更新 bit13 光报警、bit14 背光；保留其它位（含 bit15 只读） */
fun ProbeManageDraft.buildControlBit2WriteFrame(): ByteArray {
    val merged = mergeControlBit2Enables(controlBit2Raw, slaveScreenOn, alarmLightOn)
    return buildWriteMultiRegsFrame(
        NeijiProbeRegs.CONTROL_BIT2,
        u32ToLe(merged.toInt()),
        savedProbe.modbusDeviceAddr(),
    )
}

/** 辐射报警上限 → reg 50 */
fun ProbeManageDraft.buildDoseUpperWriteFrame(): ByteArray? =
    usvTextToX100(doseUpperUsv)?.let { upper ->
        buildWriteMultiRegsFrame(
            NeijiProbeRegs.DOSE_HI_TH,
            u32ToLe(upper.toInt()),
            savedProbe.modbusDeviceAddr(),
        )
    }

/** 辐射报警下限 → reg 52 */
fun ProbeManageDraft.buildDoseLowerWriteFrame(): ByteArray? =
    usvTextToX100(doseLowerUsv)?.let { lower ->
        buildWriteMultiRegsFrame(
            NeijiProbeRegs.DOSE_LO_TH,
            u32ToLe(lower.toInt()),
            savedProbe.modbusDeviceAddr(),
        )
    }
