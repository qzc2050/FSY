package com.raydose.raylink.data

import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.TimeUnit

/** 工控机本地串口（stty 配置 + 原始读写）。 */
class SerialPortClient(
    private val devicePath: String,
    private val onReceive: (ByteArray) -> Unit,
    private val onError: (String) -> Unit,
) {
    @Volatile
    private var running = false
    private var input: FileInputStream? = null
    private var output: FileOutputStream? = null
    private var readerThread: Thread? = null

    fun open(baudRate: Int = 115200): Boolean {
        if (running) return true
        return try {
            if (!configurePort(baudRate)) return false
            val devFile = File(devicePath)
            input = FileInputStream(devFile)
            output = FileOutputStream(devFile)
            running = true
            readerThread = Thread {
                val buffer = ByteArray(1024)
                while (running) {
                    try {
                        val count = input?.read(buffer) ?: -1
                        when {
                            count > 0 -> onReceive(buffer.copyOf(count))
                            count < 0 -> {
                                onError("读串口结束")
                                break
                            }
                        }
                    } catch (e: IOException) {
                        if (running) onError("读失败: ${e.message ?: "unknown"}")
                        break
                    }
                }
            }.apply {
                isDaemon = true
                name = "raylink-uart-reader"
                start()
            }
            true
        } catch (e: Exception) {
            onError("打开失败: ${e.message ?: "unknown"}")
            close()
            false
        }
    }

    private fun configurePort(baudRate: Int): Boolean {
        return try {
            val command = "stty -F $devicePath $baudRate cs8 -cstopb -parenb -ixon -ixoff -crtscts raw"
            val process = ProcessBuilder("/system/bin/sh", "-c", command)
                .redirectErrorStream(true)
                .start()
            val outputText = process.inputStream.bufferedReader().use { it.readText() }.trim()
            val exited = process.waitFor(2, TimeUnit.SECONDS)
            if (!exited || process.exitValue() != 0) {
                onError(
                    "配置串口失败: ${outputText.ifEmpty { "stty exit=${if (exited) process.exitValue() else "timeout"}" }}",
                )
                false
            } else {
                true
            }
        } catch (e: Exception) {
            onError("配置串口异常: ${e.message ?: "unknown"}")
            false
        }
    }

    fun send(data: ByteArray) {
        try {
            output?.write(data)
            output?.flush()
        } catch (e: Exception) {
            onError("发送失败: ${e.message ?: "unknown"}")
        }
    }

    fun close() {
        running = false
        try {
            input?.close()
        } catch (_: Exception) {
        }
        try {
            output?.close()
        } catch (_: Exception) {
        }
        input = null
        output = null
        readerThread = null
    }
}
