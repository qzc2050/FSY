package com.raydose.raylink.net

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager
import java.io.File

data class HostConnectivityStatus(
    /** 转接板 USB 蓝牙音频模块 QCC3084 是否在线 */
    val bluetoothOnline: Boolean = false,
    /** 本机有线网卡（eth*）是否已有 IPv4 */
    val ethernetOnline: Boolean = false,
)

fun detectHostConnectivity(appContext: Context): HostConnectivityStatus {
    val ctx = appContext.applicationContext
    return HostConnectivityStatus(
        bluetoothOnline = isQcc3084UsbAudioPresent(ctx),
        ethernetOnline = hasEthernetIpv4(ctx),
    )
}

/**
 * 主页蓝牙图标：转接板挂载的 Qualcomm QCC3084 USB 音频设备。
 * 优先 AudioManager；若权限/机型差异导致拿不到，再扫 /proc/asound 下各 card 的 id。
 */
fun isQcc3084UsbAudioPresent(appContext: Context): Boolean {
    if (isQcc3084ViaAudioManager(appContext)) return true
    return isQcc3084ViaAlsaProc()
}

private fun isQcc3084ViaAudioManager(appContext: Context): Boolean {
    val am = appContext.getSystemService(Context.AUDIO_SERVICE) as? AudioManager ?: return false
    return am.getDevices(AudioManager.GET_DEVICES_OUTPUTS).any { device ->
        val usbType = device.type == AudioDeviceInfo.TYPE_USB_HEADSET ||
            device.type == AudioDeviceInfo.TYPE_USB_DEVICE
        if (!usbType) return@any false
        val haystack = buildString {
            device.productName?.let { append(it).append(' ') }
            append(device.address.orEmpty())
        }
        haystack.contains("QCC3084", ignoreCase = true)
    }
}

private fun isQcc3084ViaAlsaProc(): Boolean {
    val asound = File("/proc/asound")
    if (!asound.isDirectory) return false
    return asound.listFiles()
        ?.asSequence()
        ?.filter { it.isDirectory && it.name.startsWith("card") }
        ?.any { cardDir ->
            runCatching {
                File(cardDir, "id").readText().trim().equals("QCC3084", ignoreCase = true)
            }.getOrDefault(false)
        } == true
}

fun hasEthernetIpv4(appContext: Context): Boolean {
    return listFsyNetworkOptions(appContext).any { opt ->
        opt.interfaceName.startsWith("eth", ignoreCase = true) &&
            opt.localIpv4.isNotBlank()
    }
}
