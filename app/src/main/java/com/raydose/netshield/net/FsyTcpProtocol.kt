package com.raydose.netshield.net

import java.util.Calendar
import kotlin.math.min

data class FiveMinUpload(
    val year: Int,
    val month: Int,
    val day: Int,
    val hour: Int,
    val minute: Int,
    val second: Int,
    val doseRateX100: Long,
) {
    val doseRateUsvH: Double get() = doseRateX100 / 100.0
    val timeString: String get() = "20%02d-%02d-%02d %02d:%02d:%02d".format(year, month, day, hour, minute, second)
    fun displayLine(): String = "$timeString  ${"%.2f".format(doseRateUsvH)} uSv/h"
}

data class DeviceTimeInfo(
    val year: Int,
    val month: Int,
    val day: Int,
    val hour: Int,
    val minute: Int,
    val second: Int,
) {
    val timeString: String
        get() = "20%02d-%02d-%02d %02d:%02d:%02d".format(year, month, day, hour, minute, second)
}

data class ParsedFsyFrame(
    val addr: Int,
    val func: Int,
    val crcOk: Boolean,
    val summary: String,
    val uploadValues: List<Long>? = null,
    val fiveMinUpload: FiveMinUpload? = null,
    val deviceVersion: String? = null,
    val deviceSerial: String? = null,
    val deviceTime: DeviceTimeInfo? = null,
    val thresholdValues: List<Long>? = null,  // 12项，顺序：辐射上/下、温度上/下、气压上/下、湿度上/下、CO2上/下、PM2.5上/下
    val statusBitValue: Long? = null,
    val controlBit2Value: Long? = null,
    val alarmEnableValue: Long? = null,
    val controlBit1Volume: Int? = null,
    val writeAckReg: Int? = null,
    val writeAckValue: Int? = null,
    val writeMultiStartReg: Int? = null,
    val writeMultiCount: Int? = null,
    val otaState: Long? = null,
    val otaWrittenBytes: Long? = null,
)

fun parseFsyTcpFrame(frame: ByteArray): ParsedFsyFrame? {
    if (frame.size < 4) return null
    val addr = frame[0].toUByte().toInt()
    val func = frame[1].toUByte().toInt()
    val crcOk = crcOk(frame)

    return when (func) {
        0x23 -> parseUpload23(frame, addr, crcOk)
        0x13 -> parseReadResp13(frame, addr, crcOk)
        0x16 -> parseWriteAck16(frame, addr, crcOk)
        0x20 -> parseWriteMultiAck20(frame, addr, crcOk)
        else -> ParsedFsyFrame(
            addr = addr,
            func = func,
            crcOk = crcOk,
            summary = "未知功能码 0x${func.toString(16).uppercase()} len=${frame.size}",
            uploadValues = null,
        )
    }
}

fun appendModbusCrc(dataNoCrc: ByteArray): ByteArray {
    val crc = crc16Modbus(dataNoCrc, 0, dataNoCrc.size)
    return dataNoCrc + byteArrayOf((crc and 0xFF).toByte(), ((crc shr 8) and 0xFF).toByte())
}

/**
 * 时间同步帧 (0x10 → reg=0x0020, count=4, byte_count=8)
 * 协议字段按小端序发送。
 * 数据格式: [year%100, month, day, hour, minute, second, 0, 0]
 */
fun buildWriteTimeFrame(
    year2d: Int, month: Int, day: Int,
    hour: Int, minute: Int, second: Int,
    deviceAddr: Byte = 0x01,
): ByteArray = appendModbusCrc(
    byteArrayOf(
        deviceAddr, 0x10, 0x20, 0x00, 0x04, 0x00, 0x08,
        year2d.toByte(), month.toByte(), day.toByte(),
        hour.toByte(), minute.toByte(), second.toByte(), 0x00, 0x00,
    )
)

/**
 * 报警阈值写入帧 (0x10 → reg=0x0040, count=24, byte_count=48)
 * thresholds 12 项 u32 小端序：辐射上/下(×100 uSv/h)、温度上/下(×10 ℃)、气压上/下(Pa)、
 * 湿度上/下(%)、CO2上/下(ppm)、PM2.5上/下(×10 ug/m3)
 */
fun buildWriteThresholdsFrame(thresholds: List<Long>, deviceAddr: Byte = 0x01): ByteArray {
    require(thresholds.size == 12) { "需要 12 项阈值" }
    val d = ByteArray(48)
    thresholds.forEachIndexed { i, v ->
        val vi = v.toInt()
        d[i * 4 + 0] = (vi and 0xFF).toByte()
        d[i * 4 + 1] = ((vi ushr 8) and 0xFF).toByte()
        d[i * 4 + 2] = ((vi ushr 16) and 0xFF).toByte()
        d[i * 4 + 3] = ((vi ushr 24) and 0xFF).toByte()
    }
    return appendModbusCrc(byteArrayOf(deviceAddr, 0x10, 0x40, 0x00, 0x18, 0x00, 0x30) + d)
}

/**
 * controlbit2 写入帧 (0x10 -> reg=0x007B, count=2, byte_count=4)
 * 数据为 uint32 小端序；bit0 门状态保留，通常不建议主动改。
 */
fun buildWriteControlBit2Frame(value: Long, deviceAddr: Byte = 0x01): ByteArray {
    val v = value.toInt()
    val data = byteArrayOf(
        (v and 0xFF).toByte(),
        ((v ushr 8) and 0xFF).toByte(),
        ((v ushr 16) and 0xFF).toByte(),
        ((v ushr 24) and 0xFF).toByte(),
    )
    return appendModbusCrc(byteArrayOf(deviceAddr, 0x10, 0x7B, 0x00, 0x02, 0x00, 0x04) + data)
}

/**
 * 序列号写入帧 (0x10 -> reg=0x0056, count=8, byte_count=16)
 * ASCII 编码，不足 16 字节补 0x00。
 */
fun buildWriteSerialFrame(serial: String, deviceAddr: Byte = 0x01): ByteArray {
    val data = serial.toByteArray(Charsets.US_ASCII).copyOf(16)
    return appendModbusCrc(byteArrayOf(deviceAddr, 0x10, 0x56, 0x00, 0x08, 0x00, 0x10) + data)
}

fun buildReadRegsFrame(startReg: Int, count: Int, deviceAddr: Byte = 0x01): ByteArray =
    appendModbusCrc(
        byteArrayOf(
            deviceAddr,
            0x03,
            (startReg and 0xFF).toByte(),
            ((startReg ushr 8) and 0xFF).toByte(),
            (count and 0xFF).toByte(),
            ((count ushr 8) and 0xFF).toByte(),
        )
    )

/**
 * 单寄存器写入帧 (0x06)，reg/value 均按小端序发送
 * 控制寄存器参考：声报警=0x0058，光报警=0x0059，风扇=0x005A；value: 0=关 1=开
 */
fun buildWriteSingleRegFrame(reg: Int, value: Int, deviceAddr: Byte = 0x01): ByteArray =
    appendModbusCrc(
        byteArrayOf(
            deviceAddr, 0x06,
            (reg and 0xFF).toByte(), ((reg ushr 8) and 0xFF).toByte(),
            (value and 0xFF).toByte(), ((value ushr 8) and 0xFF).toByte(),
        )
    )

fun buildWriteMultiRegsFrame(startReg: Int, data: ByteArray, deviceAddr: Byte = 0x01): ByteArray {
    require(data.isNotEmpty()) { "data 不能为空" }
    require(data.size % 2 == 0) { "data 长度必须为偶数" }
    require(data.size <= 128) { "data 单帧不能超过 128 字节" }
    val regCount = data.size / 2
    return appendModbusCrc(
        byteArrayOf(
            deviceAddr, 0x10,
            (startReg and 0xFF).toByte(), ((startReg ushr 8) and 0xFF).toByte(),
            (regCount and 0xFF).toByte(), ((regCount ushr 8) and 0xFF).toByte(),
            data.size.toByte(),
        ) + data
    )
}

fun buildOtaStartFrame(totalSize: Int, deviceAddr: Byte = 0x01): ByteArray =
    buildWriteMultiRegsFrame(0x00C8, u32ToLe(totalSize), deviceAddr)

fun buildOtaDoneFrame(crc32: Long, deviceAddr: Byte = 0x01): ByteArray =
    buildWriteMultiRegsFrame(0x00CA, u32ToLe(crc32.toInt()), deviceAddr)

fun buildOtaDataFrame(chunk: ByteArray, deviceAddr: Byte = 0x01): ByteArray =
    buildWriteMultiRegsFrame(0x00D0, chunk, deviceAddr)

fun buildReadOtaStatusFrame(deviceAddr: Byte = 0x01): ByteArray =
    buildReadRegsFrame(startReg = 0x00CC, count = 0x0004, deviceAddr = deviceAddr)

fun deviceTimeFromCalendar(cal: Calendar): DeviceTimeInfo = DeviceTimeInfo(
    year = cal.get(Calendar.YEAR) % 100,
    month = cal.get(Calendar.MONTH) + 1,
    day = cal.get(Calendar.DAY_OF_MONTH),
    hour = cal.get(Calendar.HOUR_OF_DAY),
    minute = cal.get(Calendar.MINUTE),
    second = cal.get(Calendar.SECOND),
)

fun encodeDeviceDataTime(time: DeviceTimeInfo): ByteArray = byteArrayOf(
    time.year.toByte(), time.month.toByte(), time.day.toByte(),
    time.hour.toByte(), time.minute.toByte(), time.second.toByte(),
    0x00, 0x00,
)

/**
 * 请求五分钟历史数据 (0x10 → reg=0x006C, count=8, byte_count=16)
 * 数据：data_time_start[8] + data_time_end[8]
 */
fun buildWriteFiveMinHistoryRequestFrame(
    start: DeviceTimeInfo,
    end: DeviceTimeInfo,
    deviceAddr: Byte = 0x01,
): ByteArray = appendModbusCrc(
    byteArrayOf(
        deviceAddr, 0x10, 0x6C, 0x00, 0x08, 0x00, 0x10,
    ) + encodeDeviceDataTime(start) + encodeDeviceDataTime(end),
)

/** 协议要求单次查询不超过 1 小时；按总时长拆成多个 1 小时窗口（从旧到新） */
fun buildFiveMinHistoryHourChunks(totalHours: Int): List<Pair<DeviceTimeInfo, DeviceTimeInfo>> {
    require(totalHours in 1..24) { "totalHours 须在 1..24" }
    val now = Calendar.getInstance()
    val rangeStart = now.clone() as Calendar
    rangeStart.add(Calendar.HOUR_OF_DAY, -totalHours)

    val chunks = mutableListOf<Pair<DeviceTimeInfo, DeviceTimeInfo>>()
    var chunkStart = rangeStart.clone() as Calendar
    while (chunkStart.before(now)) {
        val chunkEnd = chunkStart.clone() as Calendar
        chunkEnd.add(Calendar.HOUR_OF_DAY, 1)
        val actualEnd = if (chunkEnd.after(now)) now.clone() as Calendar else chunkEnd
        chunks.add(deviceTimeFromCalendar(chunkStart) to deviceTimeFromCalendar(actualEnd))
        chunkStart = chunkEnd
    }
    return chunks
}

/** 从机 TCP/串口主动上传：实时环境数据（8 项，至 status_bit） */
private const val REG_REALTIME_UPLOAD = 0x0001

/** 从机 TCP 主动上传阈值时的起始寄存器（协议表地址 50） */
private const val REG_THRESHOLD_UPLOAD = 0x0032

/** 主机读阈值应答 / 写阈值使用的起始寄存器 */
private const val REG_THRESHOLD_READ = 0x0040

private fun parseUpload23(frame: ByteArray, addr: Int, crcOk: Boolean): ParsedFsyFrame {
    if (frame.size < 7) {
        return ParsedFsyFrame(addr, 0x23, crcOk, "0x23 长度不足 len=${frame.size}", null)
    }
    val byteCount = frame[2].toUByte().toInt()
    val startReg = u16le(frame, 3)

    tryParseFiveMinUpload(frame, startReg, byteCount, addr, crcOk)?.let { return it }

    val values = readU32Payload(frame)
    val count = values.size

    // 设备 TCP 主动上传阈值：01 23 30 32 00 ...（start=0x0032 或 0x0040，至少 2×u32=辐射上下限，最多 12×u32）
    if ((startReg == REG_THRESHOLD_UPLOAD || startReg == REG_THRESHOLD_READ) && count >= 2) {
        val thr = values.take(minOf(12, count))
        val summary = "0x23 阈值主动上传: start=0x${startReg.toString(16).uppercase()} n=${thr.size} " +
            "辐射上=${"%.2f".format(thr[0] / 100.0)} uSv/h" +
            if (thr.size > 1) " 下=${"%.2f".format(thr[1] / 100.0)}" else ""
        return ParsedFsyFrame(addr, 0x23, crcOk, summary, thresholdValues = thr)
    }

    // 设备 TCP 主动上传实时：01 23 20 01 00 ...（start=0x0001，常见 8×u32）
    if (startReg == REG_REALTIME_UPLOAD && count >= 8) {
        val summary = buildString {
            append("0x23 实时上传(${count}项): ")
            append("剂量=${"%.2f".format(values[0] / 100.0)} uSv/h ")
            append("温度=${"%.1f".format(values[1] / 10.0)} ℃ ")
            append("气压=${values[2]} Pa ")
            append("湿度=${values[3]}% ")
            append("CO2=${values[4]} ppm ")
            append("PM2.5=${"%.1f".format(values[5] / 10.0)} ug/m3 ")
            append("alarm=0x${values[6].toString(16).uppercase()} ")
            append("status=0x${values[7].toString(16).uppercase()}")
        }
        return ParsedFsyFrame(addr, 0x23, crcOk, summary, uploadValues = values)
    }

    val preview = values.take(4).joinToString(", ")
    val summary = "0x23 上传: start=0x${startReg.toString(16).uppercase()} byteCount=$byteCount u32=$count [$preview]"
    return ParsedFsyFrame(addr, 0x23, crcOk, summary, values)
}

/** 五分钟值：主动上传与历史应答均为 reg=0x001E(表地址30)；payload=8 字节时间 + u32 辐射×100 */
private fun tryParseFiveMinUpload(
    frame: ByteArray,
    startReg: Int,
    byteCount: Int,
    addr: Int,
    crcOk: Boolean,
): ParsedFsyFrame? {
    if (byteCount != 0x0C || frame.size < 19) return null
    if (startReg != 0x001E) return null
    val year = frame[5].toUByte().toInt()
    val month = frame[6].toUByte().toInt()
    val day = frame[7].toUByte().toInt()
    val hour = frame[8].toUByte().toInt()
    val minute = frame[9].toUByte().toInt()
    val second = frame[10].toUByte().toInt()
    val doseX100 = u32le(frame, 13)
    val fiveMin = FiveMinUpload(year, month, day, hour, minute, second, doseX100)
    val summary = "0x23 五分钟值: 时间=${fiveMin.timeString} 辐射量=${"%.2f".format(fiveMin.doseRateUsvH)} uSv/h"
    return ParsedFsyFrame(addr, 0x23, crcOk, summary, null, fiveMin)
}

private fun readU32Payload(frame: ByteArray): List<Long> {
    val payloadStart = 5
    val payloadEnd = frame.size - 2
    val payloadLen = (payloadEnd - payloadStart).coerceAtLeast(0)
    val count = payloadLen / 4
    val values = mutableListOf<Long>()
    for (i in 0 until count) {
        values.add(u32le(frame, payloadStart + i * 4))
    }
    return values
}

private fun parseReadResp13(frame: ByteArray, addr: Int, crcOk: Boolean): ParsedFsyFrame {
    if (frame.size < 7) {
        return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 长度不足 len=${frame.size}", null)
    }
    val byteCount = frame[2].toUByte().toInt()
    val startReg = u16le(frame, 3)
    val payloadStart = 5
    val payloadEnd = frame.size - 2

    // 固件版本：startReg=0x0062，payload 为 ASCII 字符串
    if (startReg == 0x0062 && byteCount >= 10) {
        val end = minOf(payloadStart + byteCount, payloadEnd)
        val strBytes = frame.copyOfRange(payloadStart, end)
        val version = String(strBytes, Charsets.UTF_8).trimEnd('\u0000').trim()
        return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 固件版本: $version", deviceVersion = version)
    }

    // 序列号：startReg=0x0056，payload 为 16 字节 ASCII
    if (startReg == 0x0056 && byteCount >= 16) {
        val end = minOf(payloadStart + byteCount, payloadEnd)
        val strBytes = frame.copyOfRange(payloadStart, end)
        val serial = String(strBytes, Charsets.US_ASCII).trimEnd('\u0000').trim()
        return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 设备序列号: $serial", deviceSerial = serial)
    }

    // 设备时间：startReg=0x0020，payload 8 bytes [year%100,mon,day,hour,min,sec,0,0]
    if (startReg == 0x0020 && byteCount >= 8 && frame.size >= payloadStart + 8) {
        val year   = frame[payloadStart    ].toUByte().toInt()
        val month  = frame[payloadStart + 1].toUByte().toInt()
        val day    = frame[payloadStart + 2].toUByte().toInt()
        val hour   = frame[payloadStart + 3].toUByte().toInt()
        val minute = frame[payloadStart + 4].toUByte().toInt()
        val second = frame[payloadStart + 5].toUByte().toInt()
        val devTime = DeviceTimeInfo(year, month, day, hour, minute, second)
        return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 设备时间: ${devTime.timeString}", deviceTime = devTime)
    }

    // 实时环境：startReg=0x0001（转接板 0xEF 串口读/上传）
    if (startReg == REG_REALTIME_UPLOAD && byteCount >= 8 && frame.size >= payloadStart + 8) {
        val count = byteCount / 4
        val values = (0 until count).map { u32le(frame, payloadStart + it * 4) }
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 实时环境: ${values.size}项",
            uploadValues = values,
        )
    }

    // 报警阈值：startReg=0x0040（完整 12×u32 或仅辐射前 2×u32）
    if (startReg == REG_THRESHOLD_READ && byteCount >= 8 && frame.size >= payloadStart + 8) {
        val count = minOf(12, byteCount / 4)
        val thresholds = (0 until count).map { u32le(frame, payloadStart + it * 4) }
        val label = if (count >= 12) "12项" else "辐射${count}项"
        return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 报警阈值: $label", thresholdValues = thresholds)
    }

    if (startReg == 0x0052 && byteCount >= 4 && frame.size >= payloadStart + 4) {
        val enable = u32le(frame, payloadStart)
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 报警使能: 0x${enable.toString(16).uppercase()}",
            alarmEnableValue = enable,
        )
    }

    if (startReg == 0x007A && byteCount >= 2 && frame.size >= payloadStart + 2) {
        val volume = u16le(frame, payloadStart)
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 音量: $volume",
            controlBit1Volume = volume,
        )
    }

    if (startReg == 0x000F && byteCount >= 4 && frame.size >= payloadStart + 4) {
        val statusBit = u32le(frame, payloadStart)
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 状态寄存器15: 0x${statusBit.toString(16).uppercase()}",
            statusBitValue = statusBit,
        )
    }

    if (startReg == 0x007B && byteCount >= 4 && frame.size >= payloadStart + 4) {
        val controlBit2 = u32le(frame, payloadStart)
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 controlbit2寄存器123: 0x${controlBit2.toString(16).uppercase()}",
            controlBit2Value = controlBit2,
        )
    }

    if (startReg == 0x00CC && byteCount >= 8 && frame.size >= payloadStart + 8) {
        val otaState = u32le(frame, payloadStart)
        val otaWrittenBytes = u32le(frame, payloadStart + 4)
        return ParsedFsyFrame(
            addr = addr,
            func = 0x13,
            crcOk = crcOk,
            summary = "0x13 OTA状态: state=$otaState written=$otaWrittenBytes",
            otaState = otaState,
            otaWrittenBytes = otaWrittenBytes,
        )
    }

    // 通用
    val dataLen = (payloadEnd - payloadStart).coerceAtLeast(0)
    val show = min(16, dataLen)
    val preview = if (show > 0) frame.copyOfRange(payloadStart, payloadStart + show).joinToString(" ") { "%02X".format(it) } else "-"
    return ParsedFsyFrame(addr, 0x13, crcOk, "0x13 读应答: start=0x${startReg.toString(16).uppercase()} byteCount=$byteCount data[$preview]", null)
}

private fun parseWriteAck16(frame: ByteArray, addr: Int, crcOk: Boolean): ParsedFsyFrame {
    if (frame.size < 8) {
        return ParsedFsyFrame(addr, 0x16, crcOk, "0x16 长度不足 len=${frame.size}", null)
    }
    val reg = u16le(frame, 2)
    val value = u16le(frame, 4)
    val summary = "0x16 写单寄存器应答: reg=0x${reg.toString(16).uppercase()} value=0x${value.toString(16).uppercase()}"
    return ParsedFsyFrame(
        addr = addr,
        func = 0x16,
        crcOk = crcOk,
        summary = summary,
        writeAckReg = reg,
        writeAckValue = value,
    )
}

private fun parseWriteMultiAck20(frame: ByteArray, addr: Int, crcOk: Boolean): ParsedFsyFrame {
    if (frame.size < 8) {
        return ParsedFsyFrame(addr, 0x20, crcOk, "0x20 长度不足 len=${frame.size}", null)
    }
    val reg = u16le(frame, 2)
    val count = u16le(frame, 4)
    val summary = "0x20 写多寄存器应答: start=0x${reg.toString(16).uppercase()} count=$count"
    return ParsedFsyFrame(
        addr = addr,
        func = 0x20,
        crcOk = crcOk,
        summary = summary,
        writeMultiStartReg = reg,
        writeMultiCount = count,
    )
}

private fun crcOk(frame: ByteArray): Boolean {
    if (frame.size < 4) return false
    val got = (frame[frame.size - 2].toUByte().toInt()) or
        (frame[frame.size - 1].toUByte().toInt() shl 8)
    val expect = crc16Modbus(frame, 0, frame.size - 2)
    return got == expect
}

private fun u16le(data: ByteArray, offset: Int): Int {
    if (offset + 1 >= data.size) return 0
    return data[offset].toUByte().toInt() or (data[offset + 1].toUByte().toInt() shl 8)
}

fun u32ToLe(value: Int): ByteArray = byteArrayOf(
    (value and 0xFF).toByte(),
    ((value ushr 8) and 0xFF).toByte(),
    ((value ushr 16) and 0xFF).toByte(),
    ((value ushr 24) and 0xFF).toByte(),
)

private fun u32le(data: ByteArray, offset: Int): Long {
    if (offset + 3 >= data.size) return 0
    return (data[offset].toUByte().toLong()) or
        (data[offset + 1].toUByte().toLong() shl 8) or
        (data[offset + 2].toUByte().toLong() shl 16) or
        (data[offset + 3].toUByte().toLong() shl 24)
}

private fun crc16Modbus(data: ByteArray, offset: Int, len: Int): Int {
    var crc = 0xFFFF
    for (i in 0 until len) {
        crc = crc xor data[offset + i].toUByte().toInt()
        repeat(8) {
            crc = if ((crc and 0x0001) != 0) {
                (crc ushr 1) xor 0xA001
            } else {
                crc ushr 1
            }
        }
    }
    return crc and 0xFFFF
}

/**
 * 按功能码与 byte_count 从接收缓冲区切出完整协议帧（TCP 可能粘包/拆包）。
 */
class FsyProtocolFrameCollector {
    private val buffer = ArrayList<Byte>()

    @Synchronized
    fun feed(chunk: ByteArray): List<ByteArray> {
        buffer.addAll(chunk.toList())
        val frames = mutableListOf<ByteArray>()
        while (true) {
            val expectedLength = expectedFrameLength() ?: break
            if (buffer.size < expectedLength) break
            val frame = ByteArray(expectedLength) { idx -> buffer[idx] }
            repeat(expectedLength) { buffer.removeAt(0) }
            frames.add(frame)
        }
        return frames
    }

    private fun expectedFrameLength(): Int? {
        if (buffer.size < 2) return null
        return when (buffer[1].toInt() and 0xFF) {
            0x03 -> 8
            0x13, 0x23 -> {
                if (buffer.size < 3) null else (buffer[2].toInt() and 0xFF) + 7
            }
            0x15, 0x16, 0x20, 0x25, 0x83, 0x85, 0x86, 0x90 -> 8
            else -> {
                buffer.removeAt(0)
                null
            }
        }
    }
}

fun otaCrc32(data: ByteArray): Long {
    var crc = 0xFFFFFFFF.toInt()
    for (b in data) {
        crc = crc xor (b.toUByte().toInt() shl 24)
        repeat(8) {
            crc = if ((crc and 0x80000000.toInt()) != 0) {
                (crc shl 1) xor 0x04C11DB7
            } else {
                crc shl 1
            }
        }
    }
    return crc.toLong() and 0xFFFFFFFFL
}
