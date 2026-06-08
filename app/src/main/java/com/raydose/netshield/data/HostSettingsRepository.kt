package com.raydose.netshield.data

import android.content.Context
import com.raydose.netshield.model.AlbumMessage
import com.raydose.netshield.model.AlbumSettings
import com.raydose.netshield.model.AppLanguage
import com.raydose.netshield.model.DisplaySoundSettings
import com.raydose.netshield.model.HostNetworkSettings
import com.raydose.netshield.model.NetworkWifiDefaults
import com.raydose.netshield.model.ProbeCardDisplayMode
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.SlaveNetworkCard
import com.raydose.netshield.model.TimeSettings
import org.json.JSONArray
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

    fun loadAlbumSettings(): AlbumSettings {
        val o = prefs.getString(KEY_ALBUM, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return AlbumSettings()
        return AlbumSettings(
            selectedImageUri = o.optString("selectedImageUri", ""),
            applyStandby = o.optBoolean("applyStandby", false),
            applyDesktop = o.optBoolean("applyDesktop", false),
            applyMessageDesktop = o.optBoolean("applyMessageDesktop", true),
        )
    }

    fun saveAlbumSettings(settings: AlbumSettings) {
        val o = JSONObject().apply {
            put("selectedImageUri", settings.selectedImageUri)
            put("applyStandby", settings.applyStandby)
            put("applyDesktop", settings.applyDesktop)
            put("applyMessageDesktop", settings.applyMessageDesktop)
        }
        prefs.edit().putString(KEY_ALBUM, o.toString()).apply()
    }

    fun loadAlbumMessages(): List<AlbumMessage> {
        val raw = prefs.getString(KEY_ALBUM_MESSAGES, null) ?: return defaultAlbumMessages()
        return runCatching {
            val array = JSONArray(raw)
            buildList {
                for (i in 0 until array.length()) {
                    val o = array.getJSONObject(i)
                    add(
                        AlbumMessage(
                            id = o.optLong("id"),
                            text = o.optString("text"),
                            createdAtMillis = o.optLong("createdAtMillis"),
                        ),
                    )
                }
            }.filter { it.id > 0L && it.text.isNotBlank() }
        }.getOrDefault(defaultAlbumMessages())
    }

    fun saveAlbumMessages(messages: List<AlbumMessage>) {
        val array = JSONArray()
        messages.forEach { message ->
            array.put(
                JSONObject().apply {
                    put("id", message.id)
                    put("text", message.text)
                    put("createdAtMillis", message.createdAtMillis)
                },
            )
        }
        prefs.edit().putString(KEY_ALBUM_MESSAGES, array.toString()).apply()
    }

    fun loadProbeDetailOrgName(): String =
        prefs.getString(KEY_PROBE_DETAIL_ORG_NAME, "NetShield")?.trim().orEmpty().ifBlank { "NetShield" }

    fun saveProbeDetailOrgName(name: String) {
        prefs.edit().putString(KEY_PROBE_DETAIL_ORG_NAME, name.trim().ifBlank { "NetShield" }).apply()
    }

    fun loadUsbTreeUri(): String = prefs.getString(KEY_USB_TREE_URI, "").orEmpty()

    fun saveUsbTreeUri(uri: String) {
        prefs.edit().putString(KEY_USB_TREE_URI, uri).apply()
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
        private const val KEY_ALBUM = "album_settings"
        private const val KEY_ALBUM_MESSAGES = "album_messages"
        private const val KEY_PROBE_DETAIL_ORG_NAME = "probe_detail_org_name"
        private const val KEY_USB_TREE_URI = "usb_tree_uri"

        private fun defaultAlbumMessages(): List<AlbumMessage> {
            val now = System.currentTimeMillis()
            return listOf(
                AlbumMessage(1, "十年离乱后，长大一相逢。", now),
                AlbumMessage(2, "在转瞬间消灭了踪影。你我相逢在黑夜的海上，你有你的，我有我的方向。", now),
                AlbumMessage(3, "有门，不用开开是我们的，就十分美好早晨，黑夜还要流浪", now),
            )
        }
    }
}
