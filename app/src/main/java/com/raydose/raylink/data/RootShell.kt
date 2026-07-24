package com.raydose.raylink.data

import java.io.BufferedReader
import java.io.InputStreamReader

internal object RootShell {
    data class Result(
        val exitCode: Int,
        val stdout: String,
        val stderr: String,
    ) {
        val isSuccess: Boolean
            get() = exitCode == 0
    }

    fun isAvailable(): Boolean = run("id").isSuccess

    fun run(command: String): Result {
        val process = ProcessBuilder("su", "0", "sh", "-c", command)
            .redirectErrorStream(false)
            .start()
        val stdout = process.inputStream.bufferedReader().use(BufferedReader::readText)
        val stderr = process.errorStream.bufferedReader().use(BufferedReader::readText)
        val exitCode = process.waitFor()
        return Result(exitCode = exitCode, stdout = stdout.trim(), stderr = stderr.trim())
    }

    fun quote(path: String): String = buildString {
        append('"')
        path.forEach { ch ->
            when (ch) {
                '\\' -> append("\\\\")
                '"' -> append("\\\"")
                '$' -> append("\\$")
                '`' -> append("\\`")
                else -> append(ch)
            }
        }
        append('"')
    }
}