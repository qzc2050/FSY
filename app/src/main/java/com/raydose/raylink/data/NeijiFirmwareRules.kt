package com.raydose.raylink.data

import java.util.Locale

/** NeiJi App/Download 区上限 896KB（7×128KB） */
object NeijiFirmwareRules {
    const val MAX_BYTES = 896 * 1024

    /** 与 ZJB 的 `zjb_V` 对应，例如 neiji_V1.1.1.20260720R.bin */
    private const val NAME_MARKER = "neiji_V"

    const val REJECT_MESSAGE = "选的文件不对"

    fun isValidSelection(fileName: String, sizeBytes: Long): Boolean {
        if (fileName.isBlank()) return false
        if (!fileName.contains(NAME_MARKER)) return false
        if (!fileName.lowercase(Locale.US).endsWith(".bin")) return false
        if (sizeBytes <= 0L || sizeBytes > MAX_BYTES.toLong()) return false
        if (sizeBytes % 2L != 0L) return false
        return true
    }

    /** 仅校验固件内容长度（读入后 / OTA 发送前） */
    fun isValidPayload(sizeBytes: Long): Boolean {
        if (sizeBytes <= 0L || sizeBytes > MAX_BYTES.toLong()) return false
        if (sizeBytes % 2L != 0L) return false
        return true
    }
}
