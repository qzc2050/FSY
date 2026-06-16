package com.raydose.netshield.model

/**
 * 转接板 0xEF 实时上传（0x23 start=0x0001）→ 主页本机环境。
 *
 * indices: 0 辐射(忽略) 1 温度 2 气压(Pa) 3 湿度 4 CO2 5 PM2.5 6 alarm 7 status(门 bit0)
 */
data class HostAdapterSnapshot(
    val envReadings: List<HostEnvReading> = emptyList(),
    /** status_bit bit0：0=门开 1=门关 */
    val doorOpen: Boolean? = null,
    val lastUpdateMillis: Long = 0L,
) {
    val hasData: Boolean get() = lastUpdateMillis > 0L

    companion object {
        fun empty() = HostAdapterSnapshot()
    }
}

fun defaultHostEnvPlaceholders(): List<HostEnvReading> = listOf(
    HostEnvReading("温度", "---"),
    HostEnvReading("湿度", "---"),
    HostEnvReading("CO2", "---"),
    HostEnvReading("气压", "---"),
    HostEnvReading("PM2.5", "---"),
)

fun parseHostAdapterUpload(values: List<Long>, nowMillis: Long = System.currentTimeMillis()): HostAdapterSnapshot? {
    if (values.size < 6) return null
    val doorOpen = if (values.size >= 8) (values[7] and 1L) == 0L else null
    return HostAdapterSnapshot(
        envReadings = listOf(
            HostEnvReading("温度", formatHostTemp(values[1])),
            HostEnvReading("湿度", "${values[3]}%"),
            HostEnvReading("CO2", formatHostCo2(values[4])),
            HostEnvReading("气压", formatHostPressureKpa(values[2])),
            HostEnvReading("PM2.5", formatHostPm25(values[5])),
        ),
        doorOpen = doorOpen,
        lastUpdateMillis = nowMillis,
    )
}

private fun formatHostTemp(raw: Long): String = "%.1f°C".format(raw / 10.0)

private fun formatHostCo2(raw: Long): String = "${raw} ppm"

private fun formatHostPm25(raw: Long): String = "%.1f μg/m³".format(raw / 10.0)
