package com.raydose.raylink.data

import android.content.Context
import com.raydose.raylink.model.SavedProbe
import org.json.JSONArray

class ProbeConfigRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun load(): List<SavedProbe> {
        val raw = prefs.getString(KEY_PROBES, null) ?: return emptyList()
        return try {
            val arr = JSONArray(raw)
            buildList {
                for (i in 0 until arr.length()) {
                    add(SavedProbe.fromJson(arr.getJSONObject(i)))
                }
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun save(probes: List<SavedProbe>) {
        val arr = JSONArray()
        probes.forEach { arr.put(it.toJson()) }
        prefs.edit().putString(KEY_PROBES, arr.toString()).apply()
    }

    companion object {
        private const val PREFS_NAME = "raylink_probe_config"
        private const val KEY_PROBES = "saved_probes"
    }
}
