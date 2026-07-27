package com.raydose.raylink.net

import android.util.Base64
import android.util.Log
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.Charset
import java.util.concurrent.TimeUnit

/**
 * 从 HLK-7688 管理页读取 AP 的 WiFi 名称 / 密码。
 *
 * 约定（同网段 a.b.c.*）：
 * - 主机 7688：末字节 `.1`
 * - 从机设备 ID = n → 7688 末字节 `.(n+1)`（ID1→.2，ID2→.3）
 * 管理页为 HTTP Basic（默认 admin/admin），SSID 在 basic.shtml，PSK 在 security.shtml。
 */
object Hlk7688WifiClient {
    private const val TAG = "Hlk7688Wifi"
    private const val DEFAULT_USER = "admin"
    private const val DEFAULT_PASSWORD = "admin"
    private val CONNECT_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(5).toInt()
    private val READ_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(8).toInt()

    private val SSID_FROM_BASIC = Regex(
        """mssid_0\.value\s*=\s*"([^"]*)"""",
        RegexOption.IGNORE_CASE,
    )
    private val SSID_FROM_SECURITY = Regex(
        """if\s*\(\(str\s*=\s*"([^"]*)"\)\s*!=\s*""\)[\s\S]{0,80}?SSID\[0\]\s*=\s*str""",
        setOf(RegexOption.IGNORE_CASE, RegexOption.MULTILINE),
    )
    private val PSK_FROM_SECURITY = Regex(
        """WPAPSK\[0\]\s*=\s*"([^"]*)"""",
        RegexOption.IGNORE_CASE,
    )

    data class WifiCredentials(
        val ssid: String,
        val password: String,
        val gatewayIp: String,
    )

    sealed class FetchResult {
        data class Ok(val credentials: WifiCredentials) : FetchResult()
        data class Err(val error: WifiFetchError) : FetchResult()
    }

    sealed class WifiFetchError {
        data class HostIpInvalid(val hostIp: String) : WifiFetchError()
        data class SlaveIdInvalid(val deviceId: Int) : WifiFetchError()
        data class SubnetInvalid(val subnetIp: String) : WifiFetchError()
        data class NoSsidParsed(val gatewayIp: String) : WifiFetchError()
        data class EmptySsid(val gatewayIp: String) : WifiFetchError()
        data class ConnectFailed(val gatewayIp: String) : WifiFetchError()
        data class ReadFailed(val detail: String) : WifiFetchError()
    }

    /** `a.b.c.x` → `a.b.c.<lastOctet>` */
    fun gatewayIpWithLastOctet(anyIpInSubnet: String, lastOctet: Int): String? {
        if (lastOctet !in 1..254) return null
        val parts = anyIpInSubnet.trim().split('.')
        if (parts.size != 4) return null
        if (parts.any { it.toIntOrNull()?.let { n -> n !in 0..255 } != false }) return null
        return "${parts[0]}.${parts[1]}.${parts[2]}.$lastOctet"
    }

    /** 主机网关：`a.b.c.x` → `a.b.c.1` */
    fun gatewayIpFromHostIp(hostIp: String): String? = gatewayIpWithLastOctet(hostIp, 1)

    /** 从机网关：设备 ID = n → `a.b.c.(n+1)` */
    fun gatewayIpForSlave(subnetIp: String, deviceId: Int): String? {
        if (deviceId !in 1..253) return null
        return gatewayIpWithLastOctet(subnetIp, deviceId + 1)
    }

    fun fetchHostWifi(
        hostIp: String,
        username: String = DEFAULT_USER,
        password: String = DEFAULT_PASSWORD,
    ): FetchResult {
        val gateway = gatewayIpFromHostIp(hostIp)
            ?: return FetchResult.Err(WifiFetchError.HostIpInvalid(hostIp))
        return fetchFromGateway(gateway, username, password)
    }

    fun fetchSlaveWifi(
        subnetIp: String,
        deviceId: Int,
        username: String = DEFAULT_USER,
        password: String = DEFAULT_PASSWORD,
    ): FetchResult {
        if (deviceId !in 1..253) {
            return FetchResult.Err(WifiFetchError.SlaveIdInvalid(deviceId))
        }
        val gateway = gatewayIpForSlave(subnetIp, deviceId)
            ?: return FetchResult.Err(WifiFetchError.SubnetInvalid(subnetIp))
        return fetchFromGateway(gateway, username, password)
    }

    fun fetchFromGateway(
        gatewayIp: String,
        username: String = DEFAULT_USER,
        password: String = DEFAULT_PASSWORD,
    ): FetchResult {
        return try {
            val basicHtml = httpGet("http://$gatewayIp/wireless/basic.shtml", username, password)
            val securityHtml = httpGet("http://$gatewayIp/wireless/security.shtml", username, password)
            val ssid = parseSsid(basicHtml, securityHtml)
                ?: return FetchResult.Err(WifiFetchError.NoSsidParsed(gatewayIp))
            val psk = parsePassword(securityHtml).orEmpty()
            if (ssid.isBlank()) {
                return FetchResult.Err(WifiFetchError.EmptySsid(gatewayIp))
            }
            Log.i(TAG, "fetched wifi from $gatewayIp ssid=$ssid")
            FetchResult.Ok(WifiCredentials(ssid = ssid, password = psk, gatewayIp = gatewayIp))
        } catch (e: IOException) {
            Log.w(TAG, "fetch failed gateway=$gatewayIp: ${e.message}")
            FetchResult.Err(WifiFetchError.ConnectFailed(gatewayIp))
        } catch (e: Exception) {
            Log.w(TAG, "fetch error gateway=$gatewayIp", e)
            FetchResult.Err(WifiFetchError.ReadFailed(e.message ?: e.javaClass.simpleName))
        }
    }

    private fun parseSsid(basicHtml: String, securityHtml: String): String? {
        SSID_FROM_BASIC.find(basicHtml)?.groupValues?.getOrNull(1)?.takeIf { it.isNotBlank() }
            ?.let { return it }
        return SSID_FROM_SECURITY.find(securityHtml)?.groupValues?.getOrNull(1)?.takeIf { it.isNotBlank() }
    }

    private fun parsePassword(securityHtml: String): String? =
        PSK_FROM_SECURITY.find(securityHtml)?.groupValues?.getOrNull(1)

    private fun httpGet(url: String, username: String, password: String): String {
        val conn = (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = CONNECT_TIMEOUT_MS
            readTimeout = READ_TIMEOUT_MS
            requestMethod = "GET"
            instanceFollowRedirects = true
            setRequestProperty("Authorization", basicAuthHeader(username, password))
            setRequestProperty("Accept", "text/html,*/*")
        }
        try {
            val code = conn.responseCode
            val stream = if (code in 200..299) {
                conn.inputStream
            } else {
                conn.errorStream ?: conn.inputStream
            }
            val body = stream?.bufferedReader(Charset.forName("UTF-8"))?.use { it.readText() }.orEmpty()
            when (code) {
                HttpURLConnection.HTTP_UNAUTHORIZED ->
                    throw IOException("认证失败（用户名/密码错误）")
                in 200..299 -> return body
                else -> throw IOException("HTTP $code")
            }
        } finally {
            conn.disconnect()
        }
    }

    private fun basicAuthHeader(username: String, password: String): String {
        val token = Base64.encodeToString(
            "$username:$password".toByteArray(Charsets.UTF_8),
            Base64.NO_WRAP,
        )
        return "Basic $token"
    }
}
