package com.raydose.netshield.data

import com.raydose.netshield.net.ParsedFsyFrame
import com.raydose.netshield.net.buildOtaDataFrame
import com.raydose.netshield.net.buildOtaDoneFrame
import com.raydose.netshield.net.buildOtaStartFrame
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.otaCrc32
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withTimeoutOrNull

data class ZjbOtaProgress(
    val statusText: String,
    val progress: Float,
    val deviceWritten: Long? = null,
)

/**
 * 经主机串口对 ZJB（地址 0xEF）推送固件 OTA。
 * 流程与 testuart `UartTestPage.startUartOtaUpgrade` 一致。
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
            drainFrames()
            onProgress(ZjbOtaProgress("发送 OTA_START", 0f))

            serialRepository.sendRaw(buildOtaStartFrame(fileBytes.size, deviceAddr))
            awaitFrame(timeoutMs = 3000L) { it.func == 0x20 && it.writeMultiStartReg == REG_OTA_START }
                ?: error("OTA_START 无应答，请确认串口与转接板在线")

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
                serialRepository.sendRaw(buildOtaDataFrame(chunk, deviceAddr))
                awaitFrame(timeoutMs = 4000L) { it.func == 0x20 && it.writeMultiStartReg == REG_OTA_DATA }
                    ?: error("OTA 数据包 $chunkIndex 无应答")
                offset = end
            }

            onProgress(ZjbOtaProgress("发送 OTA_DONE", 1f, deviceWritten = fileBytes.size.toLong()))
            serialRepository.sendRaw(buildOtaDoneFrame(crc32, deviceAddr))
            awaitFrame(timeoutMs = 4000L) { it.func == 0x20 && it.writeMultiStartReg == REG_OTA_DONE }
                ?: error("OTA_DONE 无应答")

            onProgress(
                ZjbOtaProgress(
                    statusText = "固件已发送，转接板将校验并重启",
                    progress = 1f,
                    deviceWritten = fileBytes.size.toLong(),
                ),
            )
        }
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
        private const val REG_FIRMWARE_VERSION = 0x0062
        private const val REG_OTA_START = 0x00C8
        private const val REG_OTA_DONE = 0x00CA
        private const val REG_OTA_DATA = 0x00D0
    }
}
