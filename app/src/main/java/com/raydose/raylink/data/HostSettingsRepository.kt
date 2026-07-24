package com.raydose.raylink.data

import android.content.Context
import com.raydose.raylink.model.AlbumMessage
import com.raydose.raylink.model.AlbumSettings
import com.raydose.raylink.model.AppLanguage
import com.raydose.raylink.model.DisplaySoundSettings
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.withExpiredPauseCleared
import com.raydose.raylink.model.HostNetworkSettings
import com.raydose.raylink.model.NetworkWifiDefaults
import com.raydose.raylink.model.ProbeCardDisplayMode
import com.raydose.raylink.model.ProbeTimeSyncState
import com.raydose.raylink.model.SavedProbe
import com.raydose.raylink.model.SlaveNetworkCard
import com.raydose.raylink.model.TimeSettings
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
            pauseAlarmUntilMillis = o.optLong("pauseAlarmUntilMillis", 0L),
        ).withExpiredPauseCleared()
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
            put("pauseAlarmUntilMillis", settings.pauseAlarmUntilMillis)
        }
        prefs.edit().putString(KEY_DISPLAY, o.toString()).apply()
    }

    fun loadHostNetwork(): HostNetworkSettings {
        val o = prefs.getString(KEY_HOST_NET, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return HostNetworkSettings()
        return HostNetworkSettings(
            hostDeviceId = o.optInt("hostDeviceId", 0x20),
            hostDisplayName = o.optString("hostDisplayName", "Raylink 主机"),
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
            autoSyncToProbe = o.optBoolean("autoSyncToProbe", true),
        )
    }

    fun saveTimeSettings(settings: TimeSettings) {
        val o = JSONObject().apply {
            put("use24Hour", settings.use24Hour)
            put("showLunar", settings.showLunar)
            put("showGregorian", settings.showGregorian)
            put("showHoliday", settings.showHoliday)
            put("autoSyncToProbe", settings.autoSyncToProbe)
        }
        prefs.edit().putString(KEY_TIME, o.toString()).apply()
    }

    fun loadProbeTimeSyncState(probeId: String): ProbeTimeSyncState {
        val root = loadProbeTimeSyncRoot()
        val o = root.optJSONObject(probeId) ?: return ProbeTimeSyncState()
        return ProbeTimeSyncState(
            lastAutoSyncMillis = o.optLong("lastAutoSyncMillis", 0L),
            lastOfflineMillis = o.optLong("lastOfflineMillis", 0L),
        )
    }

    fun saveProbeTimeSyncState(probeId: String, state: ProbeTimeSyncState) {
        val root = loadProbeTimeSyncRoot()
        root.put(
            probeId,
            JSONObject().apply {
                put("lastAutoSyncMillis", state.lastAutoSyncMillis)
                put("lastOfflineMillis", state.lastOfflineMillis)
            },
        )
        prefs.edit().putString(KEY_PROBE_TIME_SYNC, root.toString()).apply()
    }

    private fun loadProbeTimeSyncRoot(): JSONObject {
        val raw = prefs.getString(KEY_PROBE_TIME_SYNC, null) ?: return JSONObject()
        return runCatching { JSONObject(raw) }.getOrDefault(JSONObject())
    }

    fun loadAlbumSettings(): AlbumSettings {
        val o = prefs.getString(KEY_ALBUM, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return AlbumSettings()
        val legacyHomeMessages = when {
            o.has("showHomeMessages") -> o.optBoolean("showHomeMessages", true)
            else -> o.optBoolean("applyMessageDesktop", true)
        }
        val legacyStandbyMessages = when {
            o.has("showStandbyMessages") -> o.optBoolean("showStandbyMessages", true)
            else -> {
                val displayJson = prefs.getString(KEY_DISPLAY, null)
                    ?.let { runCatching { JSONObject(it) }.getOrNull() }
                displayJson?.optBoolean("showStandbyMessages", true) ?: true
            }
        }
        return AlbumSettings(
            selectedImageUri = o.optString("selectedImageUri", ""),
            lastPickerStorage = parseAlbumPickerStorage(o.optString("lastPickerStorage", "")),
            lastPickerDirectory = o.optString("lastPickerDirectory", ""),
            lastSelectedSourcePath = o.optString("lastSelectedSourcePath", ""),
            applyStandby = o.optBoolean("applyStandby", false),
            showHomeMessages = legacyHomeMessages,
            showStandbyMessages = legacyStandbyMessages,
        )
    }

    fun saveAlbumSettings(settings: AlbumSettings) {
        val o = JSONObject().apply {
            put("selectedImageUri", settings.selectedImageUri)
            put("lastPickerStorage", settings.lastPickerStorage.name)
            put("lastPickerDirectory", settings.lastPickerDirectory)
            put("lastSelectedSourcePath", settings.lastSelectedSourcePath)
            put("applyStandby", settings.applyStandby)
            put("showHomeMessages", settings.showHomeMessages)
            put("showStandbyMessages", settings.showStandbyMessages)
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
        prefs.getString(KEY_PROBE_DETAIL_ORG_NAME, "Raylink")?.trim().orEmpty().ifBlank { "Raylink" }

    fun saveProbeDetailOrgName(name: String) {
        prefs.edit().putString(KEY_PROBE_DETAIL_ORG_NAME, name.trim().ifBlank { "Raylink" }).apply()
    }

    fun loadUsbTreeUri(): String = prefs.getString(KEY_USB_TREE_URI, "").orEmpty()

    fun saveUsbTreeUri(uri: String) {
        prefs.edit().putString(KEY_USB_TREE_URI, uri).apply()
    }

    /** 关于本机自定义主机序列号；空串表示未自定义。 */
    fun loadHostSerial(): String = prefs.getString(KEY_HOST_SERIAL, "").orEmpty().trim()

    fun saveHostSerial(serial: String) {
        prefs.edit().putString(KEY_HOST_SERIAL, serial.trim()).apply()
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

    private fun parseAlbumPickerStorage(raw: String): FileStorageLocation =
        runCatching { FileStorageLocation.valueOf(raw) }.getOrDefault(FileStorageLocation.Local)

    companion object {
        private const val PREFS_NAME = "raylink_host_settings"
        private const val KEY_DISPLAY = "display_sound"
        private const val KEY_HOST_NET = "host_network"
        private const val KEY_SLAVE_NET = "slave_network"
        private const val KEY_TIME = "time_settings"
        private const val KEY_PROBE_TIME_SYNC = "probe_time_sync"
        private const val KEY_ALBUM = "album_settings"
        private const val KEY_ALBUM_MESSAGES = "album_messages"
        private const val KEY_PROBE_DETAIL_ORG_NAME = "probe_detail_org_name"
        private const val KEY_USB_TREE_URI = "usb_tree_uri"
        private const val KEY_HOST_SERIAL = "host_serial"

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
