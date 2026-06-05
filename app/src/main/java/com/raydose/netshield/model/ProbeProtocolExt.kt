package com.raydose.netshield.model

import com.raydose.netshield.net.buildWriteControlBit2Frame
import com.raydose.netshield.net.buildWriteMultiRegsFrame
import com.raydose.netshield.net.buildWriteSingleRegFrame
import com.raydose.netshield.net.u32ToLe
import kotlin.math.roundToInt

/** 组播「协议地址」→ Modbus 从机地址（默认 1） */
fun SavedProbe.modbusDeviceAddr(): Byte {
    val n = protoAddr.trim().toIntOrNull() ?: 1
    return n.coerceIn(1, 247).toByte()
}

/** alarm_bit：声(bit26)或光(bit30)任一路非离线即视为外置声光已连接 */
fun isExternalAlarmConnected(alarmBit: Long): Boolean {
    val soundOffline = (alarmBit shr 26) and 1L != 0L
    val lightOffline = (alarmBit shr 30) and 1L != 0L
    return !soundOffline || !lightOffline
}

/** 0x52 bit0=禁止辐射上限报警，bit1=禁止下限；UI 开=未禁止 */
fun isRadiationUpperAlarmEnabled(alarmEnable: Long): Boolean = (alarmEnable and 1L) == 0L

fun isRadiationLowerAlarmEnabled(alarmEnable: Long): Boolean = (alarmEnable and 2L) == 0L

fun alarmEnableRegisterValue(upperOn: Boolean, lowerOn: Boolean): Long {
    var v = 0L
    if (!upperOn) v = v or 1L
    if (!lowerOn) v = v or 2L
    return v
}

fun doseX100ToUsvText(x100: Long): String = "%.2f".format(x100 / 100.0)

fun usvTextToX100(text: String): Long? =
    text.trim().replace(',', '.').toDoubleOrNull()?.let { (it * 100.0).roundToInt().toLong() }

/** control_bit2 / status_bit：bit12 声、bit13 光、bit14 屏（1=启用） */
fun controlEnablesFromBit(value: Long): Triple<Boolean, Boolean, Boolean> {
    val sound = (value shr 12) and 1L != 0L
    val light = (value shr 13) and 1L != 0L
    val screen = (value shr 14) and 1L != 0L
    return Triple(sound, light, screen)
}

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

/** 仅写 control_bit2 屏/光使能（bit13 光、bit14 屏）→ 寄存器 0x7B */
fun ProbeManageDraft.buildControlBit2WriteFrame(currentControlBit2: Long?): ByteArray {
    val base = currentControlBit2 ?: 0L
    val ctrl = mergeControlBit2Enables(base, slaveScreenOn, alarmLightOn)
    return buildWriteControlBit2Frame(ctrl, savedProbe.modbusDeviceAddr())
}

/** 写 0x52 报警使能（上下限独立 bit） */
fun ProbeManageDraft.buildAlarmEnableWriteFrame(): ByteArray =
    buildWriteMultiRegsFrame(
        0x0052,
        u32ToLe(alarmEnableRegisterValue(radiationUpperAlarmOn, radiationLowerAlarmOn).toInt()),
        savedProbe.modbusDeviceAddr(),
    )

/** 上限行报警 checkbox：0x52 + 0x32 */
fun ProbeManageDraft.buildUpperAlarmCheckboxWriteFrames(): List<ByteArray> {
    val frames = mutableListOf(buildAlarmEnableWriteFrame())
    buildDoseUpperWriteFrame()?.let { frames += it }
    return frames
}

/** 下限行报警 checkbox：0x52 + 0x34 */
fun ProbeManageDraft.buildLowerAlarmCheckboxWriteFrames(): List<ByteArray> {
    val frames = mutableListOf(buildAlarmEnableWriteFrame())
    buildDoseLowerWriteFrame()?.let { frames += it }
    return frames
}

/** 仅写音量 → 寄存器 0x7A */
fun ProbeManageDraft.buildVolumeWriteFrame(): ByteArray =
    buildWriteSingleRegFrame(0x007A, volumeSliderToReg(volume), savedProbe.modbusDeviceAddr())

/** 辐射报警上限 → 0x32；无效输入返回 null（不写） */
fun ProbeManageDraft.buildDoseUpperWriteFrame(): ByteArray? =
    usvTextToX100(doseUpperUsv)?.let { upper ->
        buildWriteMultiRegsFrame(0x0032, u32ToLe(upper.toInt()), savedProbe.modbusDeviceAddr())
    }

/** 辐射报警下限 → 0x34 */
fun ProbeManageDraft.buildDoseLowerWriteFrame(): ByteArray? =
    usvTextToX100(doseLowerUsv)?.let { lower ->
        buildWriteMultiRegsFrame(0x0034, u32ToLe(lower.toInt()), savedProbe.modbusDeviceAddr())
    }
