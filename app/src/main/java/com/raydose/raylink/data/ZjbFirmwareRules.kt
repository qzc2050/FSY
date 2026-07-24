package com.raydose.raylink.data

import java.util.Locale

object ZjbFirmwareRules {
    /** 与 ZJB App 区大小一致（0xE000 = 57344 字节） */
    const val MAX_BYTES = 0xE000

    private const val NAME_MARKER = "zjb_V"

    const val REJECT_MESSAGE = "选的文件不对"

    fun isValidSelection(fileName: String, sizeBytes: Long): Boolean {
        if (fileName.isBlank()) return false
        if (!fileName.contains(NAME_MARKER)) return false
        if (!fileName.lowercase(Locale.US).endsWith(".bin")) return false
        if (sizeBytes <= 0L || sizeBytes > MAX_BYTES) return false
        if (sizeBytes % 2L != 0L) return false
        return true
    }
}
