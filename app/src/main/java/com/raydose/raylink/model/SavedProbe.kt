package com.raydose.raylink.model

import org.json.JSONObject

/**
 * 用户已保存的从机探头（设置 · 探头管理）。
 * [id] 稳定主键，优先用序列号，否则 `ip_protoAddr`。
 */
data class SavedProbe(
    val id: String,
    val model: String,
    val serial: String,
    val ip: String,
    val controlPort: Int,
    val dataPort: Int,
    val protoAddr: String,
    val displayName: String,
    val location: String = "",
) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id)
        put("model", model)
        put("serial", serial)
        put("ip", ip)
        put("controlPort", controlPort)
        put("dataPort", dataPort)
        put("protoAddr", protoAddr)
        put("displayName", displayName)
        put("location", location)
    }

    companion object {
        fun fromJson(o: JSONObject): SavedProbe = SavedProbe(
            id = o.getString("id"),
            model = o.optString("model", ""),
            serial = o.optString("serial", ""),
            ip = o.getString("ip"),
            controlPort = o.optInt("controlPort", 502),
            dataPort = o.optInt("dataPort", 503),
            protoAddr = o.optString("protoAddr", ""),
            displayName = o.optString("displayName", "Detector"),
            location = o.optString("location", ""),
        )
    }
}
