package com.raydose.raylink.data

import android.content.Context
import com.raydose.raylink.model.AlertLogKind
import com.raydose.raylink.model.SystemAlertLog
import org.json.JSONArray
import org.json.JSONObject

/** 下拉状态栏系统日志：最多 [MAX_LOGS] 条，持久化到 SharedPreferences。 */
class AlertLogRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun load(): List<SystemAlertLog> {
        val raw = prefs.getString(KEY_LOGS, null) ?: return emptyList()
        return try {
            val arr = JSONArray(raw)
            buildList {
                for (i in 0 until arr.length()) {
                    add(fromJson(arr.getJSONObject(i)))
                }
            }.take(MAX_LOGS)
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun save(logs: List<SystemAlertLog>) {
        val trimmed = logs.take(MAX_LOGS)
        val arr = JSONArray()
        trimmed.forEach { arr.put(toJson(it)) }
        prefs.edit().putString(KEY_LOGS, arr.toString()).apply()
    }

    fun clear() {
        prefs.edit().remove(KEY_LOGS).apply()
    }

    private fun toJson(log: SystemAlertLog): JSONObject = JSONObject().apply {
        put("id", log.id)
        put("timeText", log.timeText)
        put("message", log.message)
        put("kind", log.kind.name)
        put("timestampMillis", log.timestampMillis)
    }

    private fun fromJson(obj: JSONObject): SystemAlertLog {
        val kindName = obj.optString("kind", AlertLogKind.Info.name)
        val kind = runCatching { AlertLogKind.valueOf(kindName) }.getOrDefault(AlertLogKind.Info)
        return SystemAlertLog(
            id = obj.getLong("id"),
            timeText = obj.optString("timeText"),
            message = obj.optString("message"),
            kind = kind,
            timestampMillis = obj.optLong("timestampMillis", 0L),
        )
    }

    companion object {
        const val MAX_LOGS = 100
        private const val PREFS_NAME = "raylink_alert_logs"
        private const val KEY_LOGS = "alert_logs"
    }
}
