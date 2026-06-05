package com.raydose.netshield.data

import android.content.Context
import com.raydose.netshield.model.AppLanguage
import com.raydose.netshield.model.DisplaySoundSettings
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.NetworkWifiDefaults
import com.raydose.netshield.model.ProbeCardDisplayMode
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.model.TimeSettings
import org.json.JSONObject

class HostSettingsRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun loadDisplaySound(): DisplaySoundSettings {
        val o = prefs.getString(KEY_DISPLAY, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return DisplaySoundSettings()
        return DisplaySoundSettings(
            language = AppLanguage.entries.getOrElse(o.optInt("language", 0)) { AppLanguage.Zh },
            probeCardMode = ProbeCardDisplayMode.entries.getOrElse(o.optInt("probeCardMode", 0)) {
                ProbeCardDisplayMode.Fixed
            },
            standbyMinutes = o.optInt("standbyMinutes", 5),
            brightness = o.optDouble("brightness", 0.7).toFloat(),
            systemVolume = o.optDouble("systemVolume", 0.7).toFloat(),
            hostAlarmVolume = o.optDouble("hostAlarmVolume", 0.7).toFloat(),
            promptVolume = o.optDouble("promptVolume", 0.7).toFloat(),
            visibleProbeCards = o.optInt("visibleProbeCards", 1).coerceIn(1, 4),
            mute = o.optBoolean("mute", false),
            pauseAlarmFiveMinutes = o.optBoolean("pauseAlarmFiveMinutes", false),
        )
    }

    fun saveDisplaySound(settings: DisplaySoundSettings) {
        val o = JSONObject().apply {
            put("language", settings.language.ordinal)
            put("probeCardMode", settings.probeCardMode.ordinal)
            put("standbyMinutes", settings.standbyMinutes)
            put("brightness", settings.brightness.toDouble())
            put("systemVolume", settings.systemVolume.toDouble())
            put("hostAlarmVolume", settings.hostAlarmVolume.toDouble())
            put("promptVolume", settings.promptVolume.toDouble())
            put("visibleProbeCards", settings.visibleProbeCards)
            put("mute", settings.mute)
            put("pauseAlarmFiveMinutes", settings.pauseAlarmFiveMinutes)
        }
        prefs.edit().putString(KEY_DISPLAY, o.toString()).apply()
    }

    fun loadHostNetwork(): HostNetworkSettings {
        val o = prefs.getString(KEY_HOST_NET, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return HostNetworkSettings()
        return HostNetworkSettings(
            hostDeviceId = o.optInt("hostDeviceId", 0x20),
            hostDisplayName = o.optString("hostDisplayName", "NetShield 主机"),
            ipAddress = o.optString("ipAddress", ""),
            wifiName = o.optString("wifiName").ifBlank { NetworkWifiDefaults.HOST_WIFI_NAME },
            wifiPassword = o.optString("wifiPassword").ifBlank { NetworkWifiDefaults.HOST_WIFI_PASSWORD },
            bluetoothName = o.optString("bluetoothName").ifBlank { NetworkWifiDefaults.HOST_BLUETOOTH_NAME },
        )
    }

    fun saveHostNetwork(settings: HostNetworkSettings) {
        val o = JSONObject().apply {
            put("hostDeviceId", settings.hostDeviceId)
            put("hostDisplayName", settings.hostDisplayName)
            put("ipAddress", settings.ipAddress)
            put("wifiName", settings.wifiName)
            put("wifiPassword", settings.wifiPassword)
            put("bluetoothName", settings.bluetoothName)
        }
        prefs.edit().putString(KEY_HOST_NET, o.toString()).apply()
    }

    fun loadSlaveNetworkOverrides(): Map<String, JSONObject> {
        val raw = prefs.getString(KEY_SLAVE_NET, null) ?: return emptyMap()
        return try {
            val root = JSONObject(raw)
            buildMap {
                val keys = root.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    put(key, root.getJSONObject(key))
                }
            }
        } catch (_: Exception) {
            emptyMap()
        }
    }

    fun saveSlaveNetworkCards(cards: List<SlaveNetworkCard>) {
        val root = JSONObject()
        cards.forEach { card ->
            root.put(
                card.probeId,
                JSONObject().apply {
                    put("wifiName", card.wifiName)
                    put("wifiPassword", card.wifiPassword)
                },
            )
        }
        prefs.edit().putString(KEY_SLAVE_NET, root.toString()).apply()
    }

    fun loadTimeSettings(): TimeSettings {
        val o = prefs.getString(KEY_TIME, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return TimeSettings()
        return TimeSettings(
            use24Hour = o.optBoolean("use24Hour", true),
            showLunar = o.optBoolean("showLunar", true),
            showGregorian = o.optBoolean("showGregorian", true),
            showHoliday = o.optBoolean("showHoliday", false),
        )
    }

    fun saveTimeSettings(settings: TimeSettings) {
        val o = JSONObject().apply {
            put("use24Hour", settings.use24Hour)
            put("showLunar", settings.showLunar)
            put("showGregorian", settings.showGregorian)
            put("showHoliday", settings.showHoliday)
        }
        prefs.edit().putString(KEY_TIME, o.toString()).apply()
    }

    fun buildSlaveNetworkCards(probes: List<SavedProbe>): List<SlaveNetworkCard> {
        val overrides = loadSlaveNetworkOverrides()
        return probes.map { probe ->
            val o = overrides[probe.id]
            SlaveNetworkCard(
                probeId = probe.id,
                displayName = probe.displayName,
                protoAddr = probe.protoAddr,
                ip = probe.ip,
                wifiName = o?.optString("wifiName").orEmpty().ifBlank { NetworkWifiDefaults.SLAVE_WIFI_NAME },
                wifiPassword = o?.optString("wifiPassword").orEmpty()
                    .ifBlank { NetworkWifiDefaults.SLAVE_WIFI_PASSWORD },
            )
        }
    }

    companion object {
        private const val PREFS_NAME = "netshield_host_settings"
        private const val KEY_DISPLAY = "display_sound"
        private const val KEY_HOST_NET = "host_network"
        private const val KEY_SLAVE_NET = "slave_network"
        private const val KEY_TIME = "time_settings"
    }
}
