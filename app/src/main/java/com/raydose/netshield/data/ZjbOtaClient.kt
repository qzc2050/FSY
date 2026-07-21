package com.raydose.netshield.data

import com.raydose.netshield.net.ParsedFsyFrame
import com.raydose.netshield.net.buildOtaDataFrame
import com.raydose.netshield.net.buildOtaDoneFrame
import com.raydose.netshield.net.buildOtaStartFrame
import com.raydose.netshield.net.buildReadOtaStatusFrame
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.otaCrc32
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull

data class ZjbOtaProgress(
    val statusText: String,
    val progress: Float,
    val deviceWritten: Long? = null,
)

/**
 * 经主机串口对 ZJB（地址 0xEF）推送固件 OTA。
 */
class ZjbOtaClient(
    private val serialRepository: HostEnvSerialRepository,
) {
    private val frameChannel = Channel<ParsedFsyFrame>(Channel.UNLIMITED)
    private val deviceAddr = HostEnvSerialRepository.HOST_ADAPTER_ADDR.toByte()

    suspend fun readFirmwareVersion(): String? = runWithListener {
        serialRepository.sendRaw(
            buildReadRegsFrame(startReg = REG_FIRMWARE_VERSION, count = 0x000A, deviceAddr = deviceAddr),
        )
        awaitFrame(timeoutMs = 2000L) { it.func == 0x13 && it.deviceVersion != null }?.deviceVersion
    }

    suspend fun upgrade(
        fileBytes: ByteArray,
        onProgress: (ZjbOtaProgress) -> Unit,
    ): Result<Unit> = runCatching {
        if (fileBytes.isEmpty()) error(ZjbFirmwareRules.REJECT_MESSAGE)
        if (fileBytes.size > ZjbFirmwareRules.MAX_BYTES) {
            error(ZjbFirmwareRules.REJECT_MESSAGE)
        }
        if (fileBytes.size % 2 != 0) {
            error(ZjbFirmwareRules.REJECT_MESSAGE)
        }

        val crc32 = otaCrc32(fileBytes)
        val totalChunks = (fileBytes.size + CHUNK_SIZE - 1) / CHUNK_SIZE

        runWithListener {
            serialRepository.setPollPaused(true)
            try {
                drainFrames()
                onProgress(ZjbOtaProgress("发送 OTA_START", 0f))
                sendStart(fileBytes.size)

                var offset = 0
                var chunkIndex = 0
                while (offset < fileBytes.size) {
                    val end = minOf(offset + CHUNK_SIZE, fileBytes.size)
                    val chunk = fileBytes.copyOfRange(offset, end)
                    chunkIndex++
                    onProgress(
                        ZjbOtaProgress(
                            statusText = "发送固件 $chunkIndex/$totalChunks",
                            progress = offset.toFloat() / fileBytes.size.toFloat(),
                            deviceWritten = offset.toLong(),
                        ),
                    )
                    sendDataChunk(
                        chunk = chunk,
                        expectedStart = offset.toLong(),
                        expectedEnd = end.toLong(),
                        chunkIndex = chunkIndex,
                    )
                    offset = end
                    delay(DATA_GAP_MS)
                }

                onProgress(ZjbOtaProgress("发送 OTA_DONE", 1f, deviceWritten = fileBytes.size.toLong()))
                sendDone(crc32)

                onProgress(
                    ZjbOtaProgress(
                        statusText = "固件已发送，转接板将校验并重启",
                        progress = 1f,
                        deviceWritten = fileBytes.size.toLong(),
                    ),
                )
            } finally {
                serialRepository.setPollPaused(false)
            }
        }
    }

    private suspend fun sendStart(totalSize: Int) {
        repeat(START_ATTEMPTS) { attempt ->
            drainFrames()
            serialRepository.sendRaw(buildOtaStartFrame(totalSize, deviceAddr))
            val ack = awaitFrame(START_TIMEOUT_MS) {
                it.func == 0x20 && it.writeMultiStartReg == REG_OTA_START
            }
            if (ack != null) return

            val status = readOtaStatus()
            if (status?.otaState == OTA_STATE_STARTED && status.otaWrittenBytes == 0L) {
                return
            }
            if (status?.otaState == OTA_STATE_ERROR) {
                error("OTA_START 失败：设备进入 ERROR")
            }
            if (attempt + 1 < START_ATTEMPTS) {
                delay(RETRY_GAP_MS)
            }
        }
        error("OTA_START 无应答")
    }

    private suspend fun sendDataChunk(
        chunk: ByteArray,
        expectedStart: Long,
        expectedEnd: Long,
        chunkIndex: Int,
    ) {
        repeat(DATA_ATTEMPTS) { attempt ->
            drainFrames()
            serialRepository.sendRaw(buildOtaDataFrame(chunk, deviceAddr))
            val ack = awaitFrame(DATA_TIMEOUT_MS) {
                it.func == 0x20 && it.writeMultiStartReg == REG_OTA_DATA
            }
            if (ack != null) return

            /*
             * 不能盲目重发：ACK 可能只是丢失，而设备已经落盘。
             * 读取 204 的 written_bytes，只有仍停在本包起点才允许重发。
             */
            val status = readOtaStatus()
            if (status != null) {
                if (status.otaState == OTA_STATE_ERROR) {
                    error("OTA 数据包 $chunkIndex 写入失败：设备进入 ERROR")
                }
                when (status.otaWrittenBytes) {
                    expectedEnd -> return
                    expectedStart -> Unit
                    else -> error(
                        "OTA 数据包 $chunkIndex 偏移异常：" +
                            "设备=${status.otaWrittenBytes}，期望=$expectedStart 或 $expectedEnd",
                    )
                }
            }
            if (attempt + 1 < DATA_ATTEMPTS) {
                delay(RETRY_GAP_MS)
            }
        }
        error("OTA 数据包 $chunkIndex 无应答，且无法确认设备写入进度")
    }

    private suspend fun sendDone(crc32: Long) {
        repeat(DONE_ATTEMPTS) { attempt ->
            drainFrames()
            serialRepository.sendRaw(buildOtaDoneFrame(crc32, deviceAddr))
            val ack = awaitFrame(DONE_ACK_TIMEOUT_MS) {
                it.func == 0x20 && it.writeMultiStartReg == REG_OTA_DONE
            }
            if (ack != null) return

            /*
             * 固件将 DONE 做成幂等：已完成时重复 DONE 只会重发 ACK。
             * 先读状态；STARTED 表示上次 DONE 未收到，可安全重发。
             */
            val status = readOtaStatus()
            when (status?.otaState) {
                OTA_STATE_DONE -> return
                OTA_STATE_ERROR -> error("OTA_DONE 失败：CRC 或升级标记写入失败")
                OTA_STATE_STARTED, null -> Unit
                else -> error(
                    "OTA_DONE 状态异常：state=${status?.otaState} " +
                        "written=${status?.otaWrittenBytes}",
                )
            }
            if (attempt + 1 < DONE_ATTEMPTS) {
                delay(RETRY_GAP_MS)
            }
        }
        error("OTA_DONE 多次无应答，且无法确认设备已完成校验")
    }

    private suspend fun readOtaStatus(): ParsedFsyFrame? {
        repeat(STATUS_ATTEMPTS) { attempt ->
            drainFrames()
            serialRepository.sendRaw(buildReadOtaStatusFrame(deviceAddr))
            val status = awaitFrame(STATUS_TIMEOUT_MS) {
                it.func == 0x13 && it.otaState != null && it.otaWrittenBytes != null
            }
            if (status != null) return status
            if (attempt + 1 < STATUS_ATTEMPTS) {
                delay(RETRY_GAP_MS)
            }
        }
        return null
    }

    private suspend fun <T> runWithListener(block: suspend () -> T): T {
        serialRepository.setAdapterResponseListener { frame ->
            frameChannel.trySend(frame)
        }
        return try {
            block()
        } finally {
            serialRepository.setAdapterResponseListener(null)
            drainFrames()
        }
    }

    private suspend fun awaitFrame(
        timeoutMs: Long,
        predicate: (ParsedFsyFrame) -> Boolean,
    ): ParsedFsyFrame? = withTimeoutOrNull(timeoutMs) {
        while (true) {
            val frame = frameChannel.receive()
            if (predicate(frame)) return@withTimeoutOrNull frame
        }
        @Suppress("UNREACHABLE_CODE")
        null
    }

    private fun drainFrames() {
        while (frameChannel.tryReceive().isSuccess) {
            // discard stale frames
        }
    }

    companion object {
        private const val CHUNK_SIZE = 128
        private const val DATA_GAP_MS = 15L
        private const val RETRY_GAP_MS = 200L
        private const val START_TIMEOUT_MS = 15_000L
        private const val START_ATTEMPTS = 3
        private const val DATA_TIMEOUT_MS = 2_000L
        private const val DATA_ATTEMPTS = 3
        private const val DONE_ACK_TIMEOUT_MS = 3_000L
        private const val DONE_ATTEMPTS = 3
        private const val STATUS_TIMEOUT_MS = 2_000L
        private const val STATUS_ATTEMPTS = 3
        private const val OTA_STATE_STARTED = 1L
        private const val OTA_STATE_ERROR = 3L
        private const val OTA_STATE_DONE = 4L
        private const val REG_FIRMWARE_VERSION = 0x0062
        private const val REG_OTA_START = 0x00C8
        private const val REG_OTA_DONE = 0x00CA
        private const val REG_OTA_DATA = 0x00D0
    }
}
