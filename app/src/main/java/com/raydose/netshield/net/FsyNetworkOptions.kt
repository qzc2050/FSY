package com.raydose.netshield.net

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import java.net.Inet4Address
import java.net.NetworkInterface
import java.util.Collections

data class FsyNetworkOption(
    val key: String,
    val label: String,
    val network: Network?,
    val interfaceName: String,
    val localIpv4: String,
    val fromConnectivity: Boolean,
    val isDefault: Boolean,
)

fun listFsyNetworkOptions(appContext: Context): List<FsyNetworkOption> {
    val cm = appContext.applicationContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    val active = cm.activeNetwork
    val result = mutableListOf<FsyNetworkOption>()
    val knownInterfaces = mutableSetOf<String>()

    cm.networksSnapshot().forEach { network ->
        val lp = cm.getLinkProperties(network) ?: return@forEach
        val caps = cm.getNetworkCapabilities(network)
        val ipv4 = lp.linkAddresses
            .map { it.address }
            .filterIsInstance<Inet4Address>()
            .firstOrNull { !it.isLoopbackAddress }
            ?.hostAddress
            ?: return@forEach
        val iface = lp.interfaceName ?: "unknown"
        knownInterfaces += iface
        val type = caps?.toShortTypeName()
        val isDefault = network == active
        result += FsyNetworkOption(
            key = network.toString(),
            label = buildLabel(iface = iface, type = type, ipv4 = ipv4, isDefault = isDefault, source = null),
            network = network,
            interfaceName = iface,
            localIpv4 = ipv4,
            fromConnectivity = true,
            isDefault = isDefault,
        )
    }

    Collections.list(NetworkInterface.getNetworkInterfaces()).forEach { iface ->
        val name = iface.name ?: return@forEach
        if (name in knownInterfaces) return@forEach
        if (!iface.isUp || iface.isLoopback) return@forEach
        val ipv4 = iface.inetAddresses.asSequence()
            .filterIsInstance<Inet4Address>()
            .firstOrNull { !it.isLoopbackAddress }
            ?.hostAddress
            ?: return@forEach
        val type = inferTypeFromInterfaceName(name)
        result += FsyNetworkOption(
            key = "iface:$name",
            label = buildLabel(iface = name, type = type, ipv4 = ipv4, isDefault = false, source = "接口"),
            network = null,
            interfaceName = name,
            localIpv4 = ipv4,
            fromConnectivity = false,
            isDefault = false,
        )
    }

    return result.sortedWith(
        compareByDescending<FsyNetworkOption> { it.isDefault }
            .thenBy { it.interfacePriority() }
            .thenByDescending { it.fromConnectivity }
            .thenBy { it.label },
    )
}

/**
 * 组播/TCP 发现用网卡：工控机常同时有 Wi‑Fi 与以太网，优先有线（与 [findRoutesToHost] 一致），
 * 避免 activeNetwork 落在 Wi‑Fi 而探头在同网段以太网上导致收不到 236.2.3.6:2468。
 */
fun pickFsyNetworkForMulticast(options: List<FsyNetworkOption>): FsyNetworkOption? {
    if (options.isEmpty()) return null
    return options.firstOrNull { it.interfaceName.startsWith("eth", ignoreCase = true) }
        ?: options.firstOrNull { it.isDefault }
        ?: options.firstOrNull()
}

/**
 * 当前已连接 [Network] 的快照。官方弃用 [ConnectivityManager.getAllNetworks] 后未提供同步替代 API，
 * 一次性刷新列表仍需使用该方法；持续监听场景才适合 [ConnectivityManager.registerNetworkCallback]。
 */
@Suppress("DEPRECATION")
private fun ConnectivityManager.networksSnapshot(): Array<Network> = allNetworks

private fun FsyNetworkOption.interfacePriority(): Int {
    val iface = interfaceName.lowercase()
    return when {
        iface.startsWith("wlan") -> 0
        iface.startsWith("eth") -> 1
        iface.startsWith("usb") -> 2
        iface.startsWith("rmnet") || iface.startsWith("ccmni") -> 3
        else -> 9
    }
}

private fun NetworkCapabilities.toShortTypeName(): String {
    return when {
        hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "Wi-Fi"
        hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> "Ethernet"
        hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "Cellular"
        hasTransport(NetworkCapabilities.TRANSPORT_USB) -> "USB"
        else -> "Other"
    }
}

private fun inferTypeFromInterfaceName(name: String): String {
    val iface = name.lowercase()
    return when {
        iface.startsWith("wlan") -> "Wi-Fi"
        iface.startsWith("eth") -> "Ethernet"
        iface.startsWith("usb") -> "USB"
        iface.startsWith("rmnet") || iface.startsWith("ccmni") -> "Cellular"
        else -> "Interface"
    }
}

private fun buildLabel(
    iface: String,
    type: String?,
    ipv4: String,
    isDefault: Boolean,
    source: String?,
): String = buildString {
    append(iface)
    if (!type.isNullOrBlank()) append(" / ").append(type)
    append(" / ").append(ipv4)
    if (!source.isNullOrBlank()) append(" / ").append(source)
    if (isDefault) append(" / 当前默认")
}
