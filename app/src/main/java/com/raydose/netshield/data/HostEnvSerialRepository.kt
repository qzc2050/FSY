package com.raydose.netshield.data

import android.util.Log
import com.raydose.netshield.model.HostAdapterSnapshot
import com.raydose.netshield.model.parseHostAdapterUpload
import com.raydose.netshield.net.FsySerialFrameCollector
import com.raydose.netshield.net.ParsedFsyFrame
import com.raydose.netshield.net.buildReadRegsFrame
import com.raydose.netshield.net.parseFsyTcpFrame
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * 本机环境：转接板串口地址 [HOST_ADAPTER_ADDR]（0xEF）。
 *
 * 优先解析固件每秒主动推送的 **0x23** 实时包；若长时间无数据则发 **0x03** 读 0x0001 兜底。
 */
class HostEnvSerialRepository(
    private val devicePath: String = DEFAULT_DEVICE_PATH,
    private val baudRate: Int = DEFAULT_BAUD_RATE,
    private val deviceAddr: Int = HOST_ADAPTER_ADDR,
    private val onProbeFrame: ((ParsedFsyFrame) -> Unit)? = null,
) {
    private val _snapshot = MutableStateFlow(HostAdapterSnapshot.empty())
    val snapshot: StateFlow<HostAdapterSnapshot> = _snapshot.asStateFlow()

    private val frameCollector = FsySerialFrameCollector()
    private var client: SerialPortClient? = null
    private var pollThread: Thread? = null
    @Volatile
    private var running = false
    /** OTA 期间禁 0x03 轮询，避免与固件包交织导致无 ACK */
    @Volatile
    private var pollPaused = false
    @Volatile
    private var adapterResponseListener: ((ParsedFsyFrame) -> Unit)? = null

    /** OTA / 读版本等：订阅转接板地址 0xEF 的非环境上传应答帧。 */
    fun setAdapterResponseListener(listener: ((ParsedFsyFrame) -> Unit)?) {
        adapterResponseListener = listener
    }

    fun setPollPaused(paused: Boolean) {
        pollPaused = paused
        if (paused) {
            Log.i(TAG, "HostEnv 轮询已暂停（OTA）")
        } else {
            Log.i(TAG, "HostEnv 轮询已恢复")
        }
    }

    fun start() {
        if (running) return
        running = true
        frameCollector.reset()
        client = SerialPortClient(
            devicePath = devicePath,
            onReceive = ::onSerialBytes,
            onError = ::onSerialError,
        )
        val opened = client?.open(baudRate) == true
        if (opened) {
            Log.i(TAG, "串口已打开 path=$devicePath baud=$baudRate addr=0x${deviceAddr.toString(16).uppercase()}")
            requestRealtimeRead()
            startPollLoop()
        } else {
            Log.w(TAG, "串口打开失败 path=$devicePath")
        }
    }

    fun stop() {
        running = false
        pollThread?.interrupt()
        pollThread = null
        client?.close()
        client = null
        frameCollector.reset()
    }

    /** 0x03 读实时环境 reg 0x0001，count=0x0016（11×uint32） */
    fun requestRealtimeRead() {
        if (client == null || pollPaused) return
        val frame = buildReadRegsFrame(
            startReg = REG_REALTIME_START,
            count = REG_REALTIME_COUNT,
            deviceAddr = deviceAddr.toByte(),
        )
        client?.send(frame)
    }

    /** 向 zjb 串口发送原始 Modbus 帧（非 0xEF 地址由转接板转发至 CAN/探头）。 */
    fun sendRaw(data: ByteArray) {
        /* OTA 期间只放行本机 0xEF，避免探头发现/轮询插帧撞 ACK */
        if (pollPaused && data.isNotEmpty() && (data[0].toInt() and 0xFF) != HOST_ADAPTER_ADDR) {
            return
        }
        client?.send(data)
    }

    private fun startPollLoop() {
        pollThread = Thread {
            while (running) {
                try {
                    Thread.sleep(POLL_INTERVAL_MS)
                } catch (_: InterruptedException) {
                    break
                }
                if (!running || pollPaused) continue
                val last = _snapshot.value.lastUpdateMillis
                if (last <= 0L || System.currentTimeMillis() - last >= STALE_MS) {
                    requestRealtimeRead()
                }
            }
        }.apply {
            isDaemon = true
            name = "host-env-serial-poll"
            start()
        }
    }

    private fun onSerialBytes(chunk: ByteArray) {
        val frames = frameCollector.feed(chunk)
        for (frame in frames) {
            val parsed = parseFsyTcpFrame(frame) ?: continue
            if (!parsed.crcOk) {
                Log.w(TAG, "CRC 错误 addr=0x${parsed.addr.toString(16)}: ${parsed.summary}")
                continue
            }
            if (parsed.addr == deviceAddr) {
                adapterResponseListener?.invoke(parsed)
                when (parsed.func) {
                    0x23 -> parsed.uploadValues?.let(::applyUploadValues)
                    0x13 -> parsed.uploadValues?.let(::applyUploadValues)
                }
                continue
            }
            if (parsed.addr in 0x01..0xFE) {
                onProbeFrame?.invoke(parsed)
            }
        }
    }

    private fun applyUploadValues(values: List<Long>) {
        val next = parseHostAdapterUpload(values) ?: return
        val prev = _snapshot.value
        if (prev.hasData && prev.envReadings == next.envReadings && prev.doorOpen == next.doorOpen) {
            return
        }
        _snapshot.value = next
    }

    private fun onSerialError(message: String) {
        Log.w(TAG, message)
    }

    companion object {
        private const val TAG = "NetShield"
        const val HOST_ADAPTER_ADDR = 0xEF
        const val DEFAULT_DEVICE_PATH = "/dev/ttyS9"
        const val DEFAULT_BAUD_RATE = 115200
        private const val REG_REALTIME_START = 0x0001
        /** 11 项 × 2 reg = 22 */
        private const val REG_REALTIME_COUNT = 0x0016
        private const val POLL_INTERVAL_MS = 3_000L
        private const val STALE_MS = 5_000L
    }
}
